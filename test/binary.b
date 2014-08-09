print "---- compare"
a: #{010203}
b: #{0a0b0c010203}
print same? a a
print same? a b
print equal? a a
print equal? a b
;print equal? #{414243} "ABC"


print "---- append"
probe append copy a "end"
probe append copy a #{F00BAD}
probe append copy a 'A'
probe append copy a 255


print "---- insert"
probe insert copy a "end"
probe insert copy a #{F00BAD}
probe insert copy a 'A'
probe insert copy a 255


print "---- insert part"
in-a: does [skip c: copy a 2]
probe insert/part in-a "end" 0
probe insert/part in-a #{F00BAD} 2
probe c


print "---- reverse"
probe reverse #{01020304}
probe reverse/part #{0102030405} 3


print "---- find"
probe find b 1
probe find b #{0c01}
probe find/part b #{0203} 4
probe find/part b #{0203} 90
s: charset "^0^D"
probe find #{ffff0d11 11002222} s
probe find/last #{ffff0d11 11002222} s


print "---- change"
w: #{0a0b0c0d0e}
b: #{0102030405}
a: copy w
probe change next a b
probe a

a: copy w
probe change/part next a b 2
probe a

a: copy w
probe change/part a b 8
probe a


print "---- swap"
probe swap #{FFFE680065006C006C006F00}
probe swap/group #{00112233} 4
probe swap/group #{0011223344556677} 3


print "---- rejoin"
a: #{ABCD}
probe rejoin [#{0001} 2 a a]


print "---- encoding"
probe b: #{abcd012345}
probe b2: 2#{10101011 11001101 00000001 00100011 01000101}
probe b64: 64#{q80BI0U=}

probe eq? b b2
probe encode 16 b2

probe eq? b b64
probe encode 16 b64

probe encode 64 b 
probe reduce [slice b 1 slice b 2 slice b 3]
probe [64#{qw==} 64#{q80=} 64#{q80B}]
