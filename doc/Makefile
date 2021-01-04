STYLE = lsr.css
PDOPT = -s --toc -N -f markdown


dox:
	doxygen

html: UserManual.html

pdf: UserManual.pdf


UserManual.html: UserManual.md $(STYLE)
	pandoc $(PDOPT) --self-contained --css=$(STYLE) -o $@ UserManual.md

UserManual.pdf: UserManual.md
	pandoc $(PDOPT) -o $@ $^

clean:
	rm -rf func_ref.html *.pdf *.aux *.log *.tex *.toc *.out
	rm -rf ./html ./latex
