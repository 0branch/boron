STYLE = lsr.css
PDOPT = -s -N -f markdown


dox:
	doxygen

html: UserManual.html copr.html

pdf: UserManual.pdf


UserManual.html: UserManual.md $(STYLE)
	pandoc $(PDOPT) --self-contained --toc --css=$(STYLE) -o $@ UserManual.md

UserManual.pdf: UserManual.md
	pandoc $(PDOPT) --toc -o $@ $^

copr.html: copr.md $(STYLE)
	pandoc $(PDOPT) --css=$(STYLE) -o $@ $<

clean:
	rm -rf func_ref.html *.pdf *.aux *.log *.tex *.toc *.out
	rm -rf ./html ./latex
