#!/usr/bin/env python3
"""
Automated tests for mdp's HTML comment (<!-- ... -->) stripping and
top-of-file comment/legacy metadata parsing.

Two testing strategies are used:

  1. `-d` (debug) mode: mdp prints `headers: N` / `slides: N` /
     `slide N: M lines` to stderr without requiring a TTY. This is enough
     to confirm *counts* (e.g. no headers captured, or 3 headers captured,
     or line counts after comments are stripped/collapsed).

  2. Full render via a pseudo-terminal (pty): mdp always calls ncurses'
     initscr(), even in debug mode, so a real TTY is required to run it
     at all. We spawn mdp attached to a pty, let it draw the first slide,
     send 'q' to quit cleanly, and inspect the captured raw output
     (stripped of ANSI/terminal escape sequences) to confirm the actual
     visible text - this is the only way to check that comment content
     and markers were correctly hidden from rendered slides, since debug
     mode does not dump line text/content.

Run with: python3 tests/test_comments.py
"""

import os
import pty
import re
import subprocess
import sys
import tempfile
import time

MDP = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "mdp")

ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]|\x1b\][^\x07]*\x07|\x1b[()][A-Za-z0-9]|\x1b.")

failures = []
passed = 0


def run_debug(content, debug_level=1):
    """Run `mdp -d[d...] <file>` and return parsed stderr debug output."""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as f:
        f.write(content)
        path = f.name
    try:
        args = [MDP] + ["-d"] * debug_level + [path]
        master, slave = pty.openpty()
        proc = subprocess.Popen(args, stdin=slave, stdout=slave, stderr=subprocess.PIPE,
                                 close_fds=True)
        os.close(slave)
        time.sleep(0.3)
        try:
            os.write(master, b"q")
        except OSError:
            pass
        try:
            _, err = proc.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            _, err = proc.communicate()
        os.close(master)
        return err.decode("utf-8", errors="replace")
    finally:
        os.unlink(path)


def render(content, timeout=5, extra_args=None):
    """Run mdp attached to a pty, quit immediately, and return the visible
    plain-text output with terminal escape sequences stripped.

    Note: mdp's default slide-number display ("1 / 1") is drawn in the
    same bottom-right corner as a 3rd metadata header (e.g. a 2nd
    footer/date entry), overwriting it. Pass extra_args=["-s"] to hide
    the slide number when a test needs to see a 3rd header value."""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".md", delete=False) as f:
        f.write(content)
        path = f.name
    try:
        master, slave = pty.openpty()
        env = dict(os.environ)
        env.setdefault("TERM", "xterm")
        args = [MDP] + (extra_args or []) + [path]
        proc = subprocess.Popen(args, stdin=slave, stdout=slave, stderr=slave,
                                 env=env, close_fds=True)
        os.close(slave)
        time.sleep(0.4)
        try:
            os.write(master, b"q")
        except OSError:
            pass
        out = b""
        end = time.time() + timeout
        while time.time() < end:
            try:
                chunk = os.read(master, 65536)
            except OSError:
                break
            if not chunk:
                break
            out += chunk
            if proc.poll() is not None:
                break
        proc.wait(timeout=2)
        os.close(master)
        text = out.decode("utf-8", errors="replace")
        return ANSI_ESCAPE.sub("", text)
    finally:
        os.unlink(path)


def check(name, condition, detail=""):
    global passed
    if condition:
        passed += 1
        print(f"PASS: {name}")
    else:
        failures.append(name)
        print(f"FAIL: {name} {detail}")


def headers_count(debug_text):
    m = re.search(r"headers:\s*(\d+)", debug_text)
    return int(m.group(1)) if m else None


def slides_count(debug_text):
    m = re.search(r"slides:\s*(\d+)", debug_text)
    return int(m.group(1)) if m else None


# ---------------------------------------------------------------------------
# Case 1: single-line inline comment removed, spacing normalized
# ---------------------------------------------------------------------------
def test_inline_comment_same_line():
    out = render("test <!-- bla --> blubb\n")
    check("inline comment collapses to single space",
          "test blubb" in out and "bla" not in out,
          detail=repr(out[:200]))


# ---------------------------------------------------------------------------
# Case 2: comment-only line removed entirely (not top-of-file metadata)
# ---------------------------------------------------------------------------
def test_comment_only_line_removed():
    content = "content before\n\n<!-- bla -->\n\ncontent after\n"
    out = render(content)
    check("stand-alone comment line hidden, surrounding text kept",
          "content before" in out and "content after" in out and "bla" not in out,
          detail=repr(out[:300]))


# ---------------------------------------------------------------------------
# Case 3: multi-line comment block, all enclosed lines + markers removed
# ---------------------------------------------------------------------------
def test_multiline_comment_block():
    content = "jkl\n<!--\nmno\n-->\npqr\n"
    out = render(content)
    check("multi-line comment block fully hidden",
          "jkl" in out and "pqr" in out and "mno" not in out and "-->" not in out and "<!--" not in out,
          detail=repr(out[:300]))


# ---------------------------------------------------------------------------
# Case 4: comment opens mid-line, closes on a later line
# ---------------------------------------------------------------------------
def test_comment_open_mid_line_close_later():
    content = "stu <!--\nvwx\n--> yza\n"
    out = render(content)
    check("comment spanning from mid-line to a later line is hidden",
          "stu" in out and "yza" in out and "vwx" not in out and "-->" not in out and "<!--" not in out,
          detail=repr(out[:300]))


# ---------------------------------------------------------------------------
# Case 5: combined example given by user - mixed inline/multi-line comments,
# none at the top of the file, so no metadata should be captured
# ---------------------------------------------------------------------------
def test_combined_example_not_top_of_file():
    content = (
        "abc <!-- def --> ghi\n"
        "\n"
        "jkl\n"
        "<!--\n"
        "mno\n"
        "-->\n"
        "pqr\n"
        "\n"
        "stu <!--\n"
        "vwx\n"
        "--> yza\n"
    )
    dbg = run_debug(content, debug_level=1)
    check("combined example: no headers captured",
          headers_count(dbg) == 0, detail=dbg)

    out = render(content)
    check("combined example: all visible text present, hidden text/markers absent",
          all(s in out for s in ("abc ghi", "jkl", "pqr", "stu", "yza")) and
          not any(s in out for s in ("def", "mno", "vwx", "<!--", "-->")),
          detail=repr(out[:500]))


# ---------------------------------------------------------------------------
# Case 6: inline comment as the very first line must NOT be mistaken for
# top-of-file metadata (regression: previously triggered false-positive
# metadata capture because "<!--" appeared anywhere in the line)
# ---------------------------------------------------------------------------
def test_inline_comment_first_line_not_metadata():
    content = "abc <!-- def --> ghi\n\nnext slide content\n"
    dbg = run_debug(content, debug_level=1)
    check("inline comment on first line does not trigger metadata capture",
          headers_count(dbg) == 0, detail=dbg)

    out = render(content)
    check("inline comment on first line still renders visible text, hides comment",
          "abc ghi" in out and "def" not in out,
          detail=repr(out[:300]))


# ---------------------------------------------------------------------------
# Case 7: top-of-file single-line comment metadata
# ---------------------------------------------------------------------------
def test_single_line_metadata():
    content = "<!-- title: Demo -->\n\nSlide content\n"
    dbg = run_debug(content, debug_level=1)
    check("single-line top-of-file metadata captures 1 header",
          headers_count(dbg) == 1, detail=dbg)

    out = render(content)
    check("single-line metadata: title visible, marker hidden, content intact",
          "Demo" in out and "Slide content" in out and "<!--" not in out and "-->" not in out,
          detail=repr(out[:300]))


# ---------------------------------------------------------------------------
# Case 8: top-of-file multi-line comment metadata (title + 2 footers)
# ---------------------------------------------------------------------------
def test_multiline_metadata():
    content = (
        "<!--\n"
        "title: Demo\n"
        "footer: Someone\n"
        "footer: 2026-01-01\n"
        "-->\n"
        "\n"
        "Slide content\n"
    )
    dbg = run_debug(content, debug_level=1)
    check("multi-line top-of-file metadata captures 3 headers",
          headers_count(dbg) == 3, detail=dbg)

    # use -s to hide the slide number, since it would otherwise overwrite
    # the bottom-right corner where the 3rd header (2nd footer) is drawn
    out = render(content, extra_args=["-s"])
    check("multi-line metadata: values visible, no leftover markers, content intact",
          "Demo" in out and "Someone" in out and "2026-01-01" in out and
          "Slide content" in out and "<!--" not in out and "-->" not in out,
          detail=repr(out[:400]))


# ---------------------------------------------------------------------------
# Case 9: legacy '%' header format still works (and does not segfault on
# malformed lines missing a colon, which was the original bug report)
# ---------------------------------------------------------------------------
def test_legacy_header_format():
    content = "%title: Demo\n%author Someone\n%date: 2026-01-01\n\nSlide content\n"
    dbg = run_debug(content, debug_level=1)
    check("legacy '%' header format captures headers without crashing",
          headers_count(dbg) is not None and headers_count(dbg) >= 1, detail=dbg)

    out = render(content)
    check("legacy header format renders without crashing, content intact",
          "Slide content" in out,
          detail=repr(out[:300]))


# ---------------------------------------------------------------------------
# Case 10: no metadata / no comment at top of file -> headers: 0 baseline
# ---------------------------------------------------------------------------
def test_no_metadata_baseline():
    content = "# Just a normal heading\n\nSome content\n"
    dbg = run_debug(content, debug_level=1)
    check("plain file with no comments/legacy header yields headers: 0",
          headers_count(dbg) == 0, detail=dbg)


def main():
    if not os.path.isfile(MDP):
        print(f"error: mdp binary not found at {MDP}; run `make` first.", file=sys.stderr)
        return 1

    tests = [
        test_inline_comment_same_line,
        test_comment_only_line_removed,
        test_multiline_comment_block,
        test_comment_open_mid_line_close_later,
        test_combined_example_not_top_of_file,
        test_inline_comment_first_line_not_metadata,
        test_single_line_metadata,
        test_multiline_metadata,
        test_legacy_header_format,
        test_no_metadata_baseline,
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
