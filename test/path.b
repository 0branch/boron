print "---- make"
p: make path! [a 2]
print [type? p p]


a: ["abc" "hij" "xyz"]
print reduce [make path! [a 2]]
probe a/1
probe a/4


print "---- lit-path"
p: 'some/path
print [type? p p]
lp: 'a/'b
print [type? lp lp type? first lp type? second lp]


print "---- block"
a: [x 33 y: 44]
print [a/x a/y]
