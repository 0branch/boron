print "---- loop"
loop 0 [print 'zero]
loop -1 [print 'negative]
loop 5 [print 'five]


print "---- range"
loop [a 3] [print a]
loop [a 5 7] [print ['loopB a]]
loop [a -18 0 5] [print a]


print "---- forever"
i: 0
forever [
    if eq? 10 ++ i [break]
]
print i


print "---- continue"
loop [a 3] [
    if eq? a 2 [continue]
    print ['loop-continue a]
]
foreach a [1 2 3 2] [
    if eq? a 2 [continue]
    print ['foreach-continue a]
]
a: [1 2 3 2]
forall a [
    if eq? first a 2 [continue]
    print ['forall-continue first a]
]
a: 1
while [lt? a 4] [
    x: ++ a
    if eq? x 2 [continue]
    print ['while-continue x]
]
