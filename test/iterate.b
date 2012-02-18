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


print "---- forall series change"
d: [1 2 3]
foreach x d [if eq? 2 x [append d [4 5]] print x]


print "---- map"
probe map x [1 2 3] [add x 2]
probe map x {a-b-c;d-e} [
    switch x [
        '-' ' '
        ';' [break]
        x
    ]
]


print "---- remove-each"
arr2: copy arr: [1 1 a 0 2 2 b 0 3 3 c 0]
remove-each a arr [and int? a lt? a 2]
probe arr
remove-each [a b] arr2 [word? a]
probe arr2
