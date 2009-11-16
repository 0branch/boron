print "---- binary foreach"
foreach [a b] #{00010203} [
	print [a b]
]
foreach [a b] #{0001020304} [
	print [a b]
]


print "---- string foreach"
foreach [a b] "four" [
	print [a b]
]
foreach [a b] "hello" [
	print [a b]
]


print "---- block foreach"
foreach [a b] [four words to iterate] [
	print [a b]
]
foreach [a b] [five words to iterate extra] [
	print [a b]
]


print "---- block forall"
a: [1 2 3]
forall a [probe a]
probe a
a: head a
forall a [probe a/1]


print "---- forall tweaking"
a: [1 2 3]
forall a [
    if equal? first a 2 [
        poke a 1 'two
        a: head a
    ]
    print first a
]
