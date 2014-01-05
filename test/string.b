print "---- encoding"
print encoding? "abc"
u: "Sîne klâwen"
print [encoding? u  encoding? encode 'ucs2 u]
probe encode/bom 'utf8 u


print "---- append"
probe append "x" [a b c]


print "---- insert"
probe insert "string" '0'
probe insert "string" {_abc_}
probe insert/part skip orig: "string" 4 {_abc_} 3
probe orig


print "---- trim"
probe trim "  trim1  "
probe trim/lines "aa  bb"
probe trim/lines "  aa^/  bb  "
probe trim/indent {
    Line One.
      Line Two.
    Line Three.
}


print "---- caret escape"
probe "^""
print "^""
probe {^}^"}
print {^}^"}
probe "^}^""
print "^}^""
probe "}"
print "}"


print "---- caret hex"
a: "hash: ^(23) AE: ^(1E2) xai: ^(03e6)"
print [encoding? a size? a]
probe a
print a
b: encode 'latin1 a
probe b


print "---- compare"
probe equal? "str" "STR"


print "---- do"
print do "add 3 4"


print "---- change"
b: "12345"
a: "abcde"
probe change next a b
probe a

a: "abcde"
probe change next a "^/^/gg^/"
probe a

a: change "abc" 'z'
probe a
probe head a

a: change tail "abc" 'z'
probe a
probe head a

a: "abcde"
probe p: change/slice slice a 1,2 b
print index? p
probe a

a: "abcdefg"
probe p: change/slice slice a 5 "UU"
probe a


print "---- change/part"
a: change/part "abcde" "12" 4
probe a
probe head a

a: change/part "abcde" "12" 8
probe a
probe head a

/*
a: "abcde"
probe change/part a b -33
probe a
*/


print "---- remove"
a: "remove stuff"
probe remove a
probe remove/part next a 3
probe a


print "---- reverse"
probe reverse "vwxy"
probe reverse/part "vwxyz" 3


print "---- find"
sq: "A squirrel in winter rests"
u2: "Rests ЃԐ"
probe find sq "WINTER"
probe find/case sq "WINTER"
probe find/case sq lowercase "WINTER"
probe encoding? u2
probe find sq slice u2 5

; Find does't work with utf8 series & latin1 value.
;probe find encode 'utf8 "Some Random Bits" "Random"
