print "---- block newline formatting"
probe [
	stuff: [
		1 2 3 some [] (		; This paren becomes ()
		)
	]
]
probe [ stuff2: [
		1 2 3 some [] (a
			b
		)
	]
]


print "---- paren eval"
print [[mul 3 4]]
print [(mul 3 4)]


print "---- compare"
a: [1 2 3]
b: [a b c 1 2 3]
print same? a a
print same? a b
print equal? a a
print equal? a b


print "---- append"
probe append [1 2 3] "end"
probe append [1 2 3] [x y z]
probe append/block [1 2 3] [x y z]


print "---- select"
opt: ["eh" 1 b 2 c 3]
probe select opt "eh"
probe select opt 'b
probe select opt 3
probe select opt 22


print "---- set relation"
probe intersect [a b c d] [c 1 'a]
probe intersect b: ["h" 45 new 45] b
probe difference [a b c d] [c 1 'a]
probe difference b: ["h" 45 new 45] b


print "---- sort"
probe sort [1 50.0 20.0 5] 
probe sort ["then" "hello" "goodbye" "NOW"]
probe sort [zulu alpha gamma beta]
