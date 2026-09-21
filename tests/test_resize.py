#!/usr/bin/env python3
"""
Automated tests for mdp's in-screen "terminal too small" error handling and
its automatic recovery on SIGWINCH (terminal resize).

Instead of exiting when the terminal is too small to display the current
slide, mdp now clears the screen, prints a word-wrapped error message
(naming the offending slide/line for width errors), and waits for a resize.
Once the terminal becomes large enough again, it automatically re-measures
and redraws - both on initial load and while a slide is already being
viewed.

We drive this with real pty resizes: fcntl.ioctl(..., TIOCSWINSZ, ...) sets
the pty's window size, and os.kill(pid, SIGWINCH) notifies the foreground
process, exactly like a real terminal emulator would on a window resize.

Run with: python3 tests/test_resize.py
"""

import fcntl
import os
import pty
import re
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

MDP = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "mdp")

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]|\x1b\][^\x07]*\x07|\x1b[()][A-Za-z0-9]|\x1b.")

# a single unbreakable "word" long enough to trigger a width error at any
# terminal width used below, while still being valid slide content
LONG_WORD = "x" * 100

# enough short lines in a single slide (no "---" separator) to trigger a
# height error at any terminal height used below, but few enough to still
# fit comfortably once the terminal is grown back
TALL_CONTENT = "\n\n".join(f"paragraph {i}" for i in range(10))

failures = []
passed = 0


def set_size(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def start(content, rows, cols):
    """Spawn mdp attached to a pty of the given size. Returns (proc, master, path)."""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as f:
        f.write(content)
        path = f.name
    master, slave = pty.openpty()
    set_size(slave, rows, cols)
    env = dict(os.environ)
    env.setdefault("TERM", "xterm")
    proc = subprocess.Popen([MDP, path], stdin=slave, stdout=slave, stderr=slave,
                             env=env, close_fds=True)
    os.close(slave)
    return proc, master, path


def read_available(master, timeout=1.0):
    out = b""
    end = time.time() + timeout
    while time.time() < end:
        try:
            import select
            r, _, _ = select.select([master], [], [], 0.2)
        except OSError:
            break
        if master in r:
            try:
                chunk = os.read(master, 65536)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
    return ANSI_ESCAPE.sub("", out.decode("utf-8", errors="replace"))


def resize(proc, master, rows, cols):
    set_size(master, rows, cols)
    os.kill(proc.pid, signal.SIGWINCH)


def stop(proc, master, path):
    try:
        os.write(master, b"q")
    except OSError:
        pass
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
    os.close(master)
    os.unlink(path)


def check(name, condition, detail=""):
    global passed
    if condition:
        passed += 1
        print(f"PASS: {name}")
    else:
        failures.append(name)
        print(f"FAIL: {name} {detail}")


# ---------------------------------------------------------------------------
# Case 1: a terminal too narrow for an unbreakable line shows an in-screen
# width error naming the offending slide/line, instead of exiting
# ---------------------------------------------------------------------------
def test_width_error_shown_on_load():
    proc, master, path = start(LONG_WORD + "\n", rows=24, cols=40)
    try:
        out = read_available(master, timeout=1.5)
        check("narrow terminal shows in-screen width error on load",
              "Terminal width" in out and "too small" in out and "slide 1" in out,
              detail=repr(out[:300]))
        check("mdp keeps running (waiting for resize) instead of exiting",
              proc.poll() is None, detail=f"exited with {proc.poll()}")
    finally:
        stop(proc, master, path)


# ---------------------------------------------------------------------------
# Case 2: widening the terminal after a width error auto-recovers and
# renders the slide, without needing a keypress
# ---------------------------------------------------------------------------
def test_width_error_recovers_on_resize():
    proc, master, path = start(LONG_WORD + "\n", rows=24, cols=40)
    try:
        read_available(master, timeout=1.0)  # let the error show first
        resize(proc, master, rows=24, cols=120)
        out = read_available(master, timeout=1.5)
        check("widening the terminal clears the width error and renders content",
              LONG_WORD in out and "Terminal width" not in out,
              detail=repr(out[:300]))
    finally:
        stop(proc, master, path)


# ---------------------------------------------------------------------------
# Case 3: a terminal too short for a slide shows an in-screen height error
# ---------------------------------------------------------------------------
def test_height_error_shown_on_load():
    proc, master, path = start(TALL_CONTENT + "\n", rows=6, cols=80)
    try:
        out = read_available(master, timeout=1.5)
        check("short terminal shows in-screen height error on load",
              "Terminal height" in out and "too small" in out,
              detail=repr(out[:300]))
        check("mdp keeps running (waiting for resize) instead of exiting",
              proc.poll() is None, detail=f"exited with {proc.poll()}")
    finally:
        stop(proc, master, path)


# ---------------------------------------------------------------------------
# Case 4: growing the terminal after a height error auto-recovers and
# renders the slide
# ---------------------------------------------------------------------------
def test_height_error_recovers_on_resize():
    proc, master, path = start(TALL_CONTENT + "\n", rows=6, cols=80)
    try:
        read_available(master, timeout=1.0)  # let the error show first
        resize(proc, master, rows=45, cols=80)
        out = read_available(master, timeout=1.5)
        check("growing the terminal clears the height error and renders content",
              "paragraph 0" in out and "Terminal height" not in out,
              detail=repr(out[:300]))
    finally:
        stop(proc, master, path)


# ---------------------------------------------------------------------------
# Case 5: shrinking below the required size *during* an active session (not
# just on load) also shows the error, and growing back recovers
# ---------------------------------------------------------------------------
def test_resize_during_active_session():
    content = LONG_WORD + "\n"
    proc, master, path = start(content, rows=24, cols=120)
    try:
        out = read_available(master, timeout=1.0)
        check("session starts by rendering normally",
              LONG_WORD in out, detail=repr(out[:300]))

        resize(proc, master, rows=24, cols=40)
        out = read_available(master, timeout=1.5)
        check("shrinking mid-session below the needed width shows the error",
              "Terminal width" in out and "too small" in out,
              detail=repr(out[:300]))

        resize(proc, master, rows=24, cols=120)
        out = read_available(master, timeout=1.5)
        check("growing back mid-session recovers and redraws the slide",
              LONG_WORD in out and "Terminal width" not in out,
              detail=repr(out[:300]))
    finally:
        stop(proc, master, path)


def main():
    if not os.path.isfile(MDP):
        print(f"error: mdp binary not found at {MDP}; run `make` first.", file=sys.stderr)
        return 1

    tests = [
        test_width_error_shown_on_load,
        test_width_error_recovers_on_resize,
        test_height_error_shown_on_load,
        test_height_error_recovers_on_resize,
        test_resize_during_active_session,
    ]

    for t in tests:
        t()

    print(f"\n{passed}/{passed + len(failures)} checks passed.")
    if failures:
        print("Failures:")
        for f in failures:
            print(f"  - {f}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
