print "---- Do get-word!"
probe do :gwval
gwval: 2
probe do :gwval
parse [10.0 :gwval] [decimal! tok: get-word! (probe do first tok)]
