print "---- throw/catch"
a: func [x] [if lt? x 4 [throw 'low] x]
print catch [a 3]
print catch [a 4]


print "---- throw/catch name"
a: func [x] [if lt? x 4 [throw/name 8 'signal] x]
print catch/name [a 3] 'signal
print catch/name [a 4] 'signal
y: 0
x: catch/name [y: catch/name [a 3] 'sig1] [sig2 signal]
print [x y]
