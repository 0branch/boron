; Some UTF-8 text from http://www.columbia.edu/kermit/utf8.html

strings:
[
French {Les naïfs ægithales hâtifs pondant à Noël où il gèle sont sûrs d'être déçus et de voir leurs drôles d'œufs abîmés.}

German {Falsches Üben von Xylophonmusik quält jeden größeren Zwerg.}

Greek-monotonic {ξεσκεπάζω την ψυχοφθόρα βδελυγμία}

Icelandic {Sævör grét áðan því úlpan var ónýt.}

Russian {В чащах юга жил-был цитрус? Да, но фальшивый экземпляр! ёъ.}

Emoticons "^(1f600) ^(1f635) ^(1f64c) ^(1f927)"
]

probe strings

foreach [l s] strings [
    print [" --" l encoding? s "--"]
    probe s
    probe uppercase s
    probe lowercase s
]
