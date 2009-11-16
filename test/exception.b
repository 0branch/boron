print "---- throw/catch"
a: func [x] [if lt? x 4 [throw 'low] x]
print catch [a 3]
print catch [a 4]
