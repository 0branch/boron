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


print "---- find"
probe find b 1
probe find b #{0c01}
probe find/part b #{0203} 4
probe find/part b #{0203} 90


print "---- change"
w: #{0a0b0c0d0e}
b: #{0102030405}
a: copy w
probe change next a b
probe a

a: copy w
probe change/part next a b 2
probe a


print "---- swap"
probe swap #{FFFE680065006C006C006F00}
probe swap/group #{00112233} 4
probe swap/group #{0011223344556677} 3

