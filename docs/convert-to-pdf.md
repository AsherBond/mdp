### Convert to PDF

- Install `md2pdf` by obtaining the [release](https://github.com/mandolyte/mdtopdf/releases) for your arch and OS or, if
  you have `go` installed, invoke:

```sh
$ go install github.com/mandolyte/mdtopdf/cmd/md2pdf@latest
```
- If you require syntax highlighting, download the [gohighlight lexers](https://github.com/jessp01/gohighlight/tree/master/syntax_files)

`md2pdf` supports all major `mdp` features and accepts local files, remote HTTP(s) URL and `STDIN` inputs.
The below command will convert an `mdp` compatible markdown file to a PDF with a dark theme,
syntax highlighting (you'll need to provide the language hint, of course), page/slide separation and a footer:

```sh
md2pdf -i https://github.com/jessp01/crash-course-in/raw/main/courses/apt_dpkg_deb/apt_dpkg_deb.md \
    -o apt_dpkg_deb.pdf \
    -s ~/.config/zaje/syntax_files \
    --theme dark \
    --new-page-on-hr \
    --with-footer \
    --author "Jesse Portnoy <jesse@packaman.io>" \
    --title "A crash course on handling deb packages"
```

Since `markdown` does not support the centering escape sequences (i.e: `->` and `<-`), you will want to remove these before converting, for example:

```sh
$ sed 's@^->\s*\(#.*\)\s*<-@\1@g' sample.md | ~/go/bin/md2pdf -o mdp.pdf \
    --theme dark --new-page-on-hr
```
