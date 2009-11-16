print "---- trace format"
print try [
	top: does [f 3]
	f: func [a] [
		add b a "ok"   ; <- This string tests that report is data, not text. 
	]
	top
]
