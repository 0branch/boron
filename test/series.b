print "---- ordinal"
v: "hello"
print first v
print second v


print "---- query"
print series? 2
foreach s [
    "123"
    ""
	#{010203}
    #{}
    [1 2 3]
    []
][
    print [type? s series? s empty? s]
]


print "---- next"
probe prev v
probe n: next next v
probe prev n


print "---- ++/--"
it: [1 2 3 4]
print ++ it
print it
print -- it
print -- it


print "---- clear"
clear it
probe it


print "---- pick"
b: [word1 word2 word3]
print [pick b -1  pick b 0  pick b 2  pick b 4]
print [pick tail b -2  pick tail b 0]


print "---- append"
probe append b 5
probe append "abc" 'd'
probe append #{0102} 3
probe append #{0102} #{beef}


print "---- join"
probe join 'x' "abc"
probe join 23 "abc"
v: "Good"
probe join v " job"
probe v             ; v unchanged.


print "---- pop"
b: [1 2 3 4]
print [pop b pop b b]
s: {smog}
print [pop s pop s s]
probe pop []


print "---- terminate"
p: "path"
probe terminate p '/'
probe terminate p '/'
probe terminate p 'b'
probe terminate p '\'
probe terminate/dir p '/'


print "---- slice"
t: "<test>"
probe s: slice t 1,-1
probe slice s 2
probe t
probe slice s none
probe slice "some tiny example" 5,4


print "---- slice to tail"
t: "<tail>"
s: slice t size? t
append t "<more>"
probe s


print "---- skip"
b: [1 2]
probe skip b 4
probe skip b -4
b: [1 2 3]
probe skip b 2
probe skip b false
probe skip b true


print "---- skip/wrap"
b: [1 2 3]
probe skip/wrap b 7
probe skip/wrap b 0
probe skip/wrap b -1
b: []
probe skip/wrap b 2
