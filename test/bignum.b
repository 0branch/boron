print "---- bignum hex"
big: $7FFFFFFFFFFFFFFF
print [type? big big]
print to-dec big
probe 0x18f00000123     ; Leading zeros on low half.

/*
print "---- bignum sub"
print sub big 200000000000000007
print sub big big

print "---- bignum mul"
print 1 1 bignum! as mul
print 2.5 bignum! as 2 mul
print -2.5 bignum! as 2 mul
print to-hex mul big 0.03125

print "---- bignum cmp"
print [gt? big 3  lt? big 3  gt? big 3.3  lt? big 3.3]
*/
