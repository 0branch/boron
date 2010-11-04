print "---- make"
probe d: -44, 6, 9888
print [size? d d/1 d/4]

probe c: make coord! [1 2 10 -66 5]
print [size? c c/3 c/0]

print "---- tokenize"
probe to-block {1,2}    ; coord! at end of input.
