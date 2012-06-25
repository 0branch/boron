print "---- make"
probe 1.2,5.8891
probe d: -44.0, 6, 9888
print [d/1 d/4]

probe c: make vec3! [1 2.2 10]
print [c/3 c/0]
probe to-vec3 next #[1.0 2 3 4.1]


print "---- tokenize"
print [1.1,2.51,-45 1.0,4]


print "---- ordinal"
v: 1.1,2.51,-45
print [v first v second v third v]
print [v/1 v/2 v/3]
print [pick v 1 pick v 2 pick v 3]


print "---- set element"
a: 0.0,1,2
a/1: 9.0
a/2: 10.0
a/3: 11.0
probe a
a: 0.0,1,2
b: poke a 1 4.4
b: poke b 2 5.5
b: poke b 3 6.6
probe a
probe b


print "---- compare"
foreach [n v] [
    1.1    1.1,0
    2.51   2.51,0
    -45.0  -45.0,0
    2.15   2.15,0
][
    print [n  eq? n first v  same? n first v]
]
