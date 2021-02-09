print "---- Make Bitset"
print make bitset! 61
print make bitset! "abc"
print make bitset! "01234567890"
probe charset ' '


print "---- Bitset pick"
c: charset "^0abc"
probe c
foreach i [0 1 2 97 98 '`' 'a' 'b' 'c' 'd'] [prin rejoin [' ' i ':' pick c i]]
prin '^/'


print "---- Bitset poke"
foreach [i v] [2 true 'A' 1 'b' false] [poke c i do v]
probe c
b: make bitset! 32
poke b 12 2
poke b 13 0.02
probe b
poke b 12 0
poke b 13 0.0
probe b


print "---- Bitset operators"
a: charset "abc"
b: charset "123b"
probe and a b
probe or  a b
probe xor a b


print "---- Bitset compare"
c: make bitset! #{01000000000000000600}
d: charset "^0AB"
probe c
probe d
probe same? c c
probe same? c d
probe eq?   c d
probe eq?   c a


print "---- Charset range"
probe n: make bitset! s: "_0-9A-F"
probe a: make bitset! "_0123456789ABCDEF"
probe b: charset s
print [eq? n b eq? a b]
