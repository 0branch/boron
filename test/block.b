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


print "---- make"
probe make block! 'a/b/c
probe make block! first [(1 2)]
probe to-block 'a/b/c
probe to-block first [(1 2)]


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
probe append a: [1 2 3 4 five] a      ; Append to self


print "---- insert"
probe insert [1 2 3] "end"
probe insert [1 2 3] [x y z]
probe insert/block [1 2 3] [x y z]
probe insert a: [1 2 3 4 five] a      ; Insert into self
probe head insert/part next [1 2 3] [x y z] 2


print "---- select"
opt: ["eh" 1 b 2 b 4 c 3]
probe select opt "eh"
probe select opt 'b
probe select opt 3
probe select opt 22
probe select/last opt 'b


print "---- set relation"
probe intersect [a b c d] [c 1 'a]
probe intersect b: ["h" 45 new 45] b
probe difference [a b c d] [c 1 'a]
probe difference b: ["h" 45 new 45] b
probe difference [3 2 2 0] [1 3 4]
probe union [3 2 2 0] [1 3 4]


print "---- change"
w: [a b c d e]
b: [1 2 3 4 5]
a: copy w
probe change next a b
probe a

;probe change/only a [x y]
;probe a

a: copy w
probe change/part next a b 2
probe a

a: copy w
probe change/part a b 8
probe a

a: copy w
probe change/part a b -33
probe a

a: copy w
probe change next a "new"
probe a
probe change tail a "new"
probe a


print "---- reverse"
probe reverse [1 2 3 4]
probe reverse/part [1 2 3 4 5] 3
