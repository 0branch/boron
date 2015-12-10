STYLE = lsr.css


dox:
	doxygen
	cp boron_logo2.jpg ./html

html: UserManual.html UserManualGL.html

pdf: UserManual.pdf


UserManual.html: UserManual.md
	pandoc -s --css=$(STYLE) --toc -N -f markdown -o $@ $^

UserManualGL.html: UserManualGL.md mkref-widgets.b
	boron -s mkref-widgets.b
	pandoc -s --css=$(STYLE) --toc -N -f markdown -o $@ UserManualGL.md widgets.md

UserManual.pdf: UserManual.md
	pandoc -s --toc -N -f markdown -o $@ $^

clean:
	rm -rf func_ref.html *.pdf *.aux *.log *.tex *.toc *.out
	rm -rf ./html ./latex
