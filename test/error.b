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


print "---- error compare"
a: try [div 1 0]
b: try [div 1 0]
print eq? a a
print eq? a b


print "---- stack overflow"
factorial: func [x | pad1 pad2 pad3 pad4 pad5 pad6 pad7] [
    if lt? x 2 [return 1]
    mul x factorial sub x 1
]
msg: to-string try [print factorial 1000]
parse msg [thru "factorial" thru '^/' :msg]
print [msg "..."]
