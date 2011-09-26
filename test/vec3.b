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
