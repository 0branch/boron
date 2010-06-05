/*
   Run a script and reload it when 'reload is thrown.
*/

ifn args [
    print "Specify script to run"
    quit
]

while [true] [
    ex: catch load first args
    ifn eq? 'reload ex [
        throw ex
    ]
    print "Reload"
]
