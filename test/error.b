print "---- trace format"
print try [
	top: does [f 3]
	f: func [a] [
		add b a "ok"   ; <- This string tests that report is data, not text. 
	]
	top
]


print "---- error func"
print try [error "Test error"]
fe: func [a] [
    if gt? a 5 [
        error rejoin ["number (" a ") is bigger than 5"]
    ]
]
print try [fe 22]
