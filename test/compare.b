tf: func [blk] [
    map it blk [pick ['T' '.'] it]
]
comp-id: func [a b] [
    msg: [
        a '^-' b '^-'
        '(' tf reduce [same? a b equal? a b gt? a b lt? a b ge? a b le? a b] ')'
        type? a type? b
    ]
    print msg
    b: to-double b
    print msg
    a: to-double a
    print msg
]

print "---- numbers <"
comp-id -1 0

print "---- numbers ="
comp-id 0 0

print "---- numbers >"
comp-id 1 0
