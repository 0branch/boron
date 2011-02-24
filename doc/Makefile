RST_STYLE = lsr.css


dox:
	doxygen
	cp boron_logo2.jpg ./html

html: UserManual.html UserManualGL.html

pdf: UserManual.pdf UserManualGL.pdf


%.html: %.rst
	rst2html --stylesheet=$(RST_STYLE) $^ >$@

# Must not use ".. sectnum::" when building PDF.
# pdflatex must be run twice to insert the table of contents, and a third
# time to re-adjust contents page numbers.
%.pdf : %.rst
	sed -e "s/.. sectnum:://" $< >tmp.out
	rst2latex --use-latex-toc tmp.out $*.tex
	pdflatex $*.tex
	pdflatex $*.tex
	pdflatex $*.tex

clean:
	rm -rf func_ref.html *.pdf *.aux *.log *.tex *.toc *.out
	rm -rf ./html ./latex
