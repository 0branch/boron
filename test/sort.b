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
    context [part: 'head  size: 4 regen: 0]
    context [part: 'wing  size: 8 regen: 0]
    context [part: 'head  size: 2 regen: 1]
    context [other: none part: 'wing  size: 7 regen: 0]
]
probe sort/field ctxb [part size]

blkb: [
    [4 0 head]
    [8 0 wing]
    [2 1 head]
    [7 0 wing]
]
probe sort/field blkb [3 1]

strb: [
    "a-8"
    "X-2"
    "x-3"
    "j-9"
    "b-3"
    "m-0"
]
probe sort/field strb [3 1]
