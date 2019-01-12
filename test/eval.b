print "---- Do various types"
probe do [1 2 3]
probe type? do 'hi

print "---- Do get-word!"
probe do :gwval
gwval: 2
probe do :gwval
parse [10.0 :gwval] [double! tok: get-word! (probe do first tok)]

print "---- Do functions"
addf: func [a b] [add a b]
probe do reduce [:add 2 3]
probe do reduce [:addf 2 3]
