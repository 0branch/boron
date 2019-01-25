print "---- split-path"
probe split-path %path/to/file
probe split-path %some-file
probe split-path %C:/windows/file
;probe split-path %C:\windows\file


print "---- replace"
s: "comma,separated,words,"
probe replace     copy s ',' ' '
probe replace/all copy s ',' ' '
probe replace/all copy s ',' "-->"
s2: {include: ""}
probe replace/all copy s2 '"' {\"}


print "---- funct"
sum: func [a] [
    lsum: 0
    forall a [
        lsum: add lsum lval: first a
        if eq? lval 2 [print "Found lval 2."]
    ]
    lsum
]
probe sum [1 2 3]
print [value? 'lsum value? 'lval]
