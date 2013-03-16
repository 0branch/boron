print "---- Make Bitset"
print make bitset! 61
print make bitset! "abc"
print make bitset! "01234567890"
probe charset ' '


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
