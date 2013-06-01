print "---- sort block!"
probe sort [1 50.0 20.0 5] 
probe sort b: ["then" "hello" "goodbye" "NOW" "now"]
probe sort/case b
probe sort [zulu alpha gamma beta]
probe sort/group [0 Apple  3 Dog  1 Box  2 Cat] 2

; Caught bug in qsortIndex.
probe sort [6 2 1 9 3 7 5 8 15 4 0 16 14 10 12 11 13 17]


print "---- sort field"
ctxb: reduce [
    context [part: 'head  size: 2 regen: false]
    context [part: 'wing  size: 4 regen: false]
    context [part: 'head  size: 1 regen: true]
    context [other: none part: 'wing  size: 3 regen: false]
]
probe sort/field copy ctxb [part size]
probe sort/field copy ctxb [part size /desc]

blkb: [
    [2 false head]
    [4 false wing]
    [1 true head]
    [3 false wing]
]
probe sort/field copy blkb [3 1]
probe sort/field copy blkb [3 1 /desc]

/*
strb: [
    "a-8"
    "X-2"
    "x-3"
    "j-9"
    "b-3"
    "m-0"
]
probe sort/field strb [3 1]
*/
