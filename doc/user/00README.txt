= Building the Recoll user manual

The Recoll user manual used to be written in DocBook SGML and used the
FreeBSD doc toolchain to produce the output formats. This had the advantage
of an easy way to produce all formats including a PDF manual, but presented
two problems:

 - Dependency on the FreeBSD platform.
 - No support for UTF-8 (last I looked), only latin1.

The manual has been converted to docbook XML a long time ago.

It uses xsltproc and the docbook style sheets to produce the single-page
HTML version, dblatex for the PDF version, and webhelp for the multi-page
HTML version.
