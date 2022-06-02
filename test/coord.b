print "---- make"
probe d: -44, 6, 9888
print [size? d d/1 d/4]

probe c: make coord! [1 2 10 -66 5]
print [size? c c/3 c/0]

print "---- tokenize"
probe to-block {1,2}    ; coord! at end of input.

print "---- math"
probe add 1,2 3,4,5
probe sub 1,2 3,4,5
probe mul 1,2 3,4,5
probe div 5,2 2,1
probe and 5,2 2,2

print "---- math int!"
probe add 1,2 3
probe sub 1,2,3,4 3
probe mul 1,2,3,4,5,6 3
probe div 5,2 2
probe and 5,2 2

print "---- slice"
x: 0,1,2,3,4
probe slice x 2
probe slice x -2
probe slice x 2,2
probe slice 10,11 4     ; increase length
probe slice 10,11,2 1,4
