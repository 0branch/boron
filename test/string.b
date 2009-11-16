print "---- encoding"
print encoding? "abc"


print "---- append"
probe append "x" [a b c]


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

