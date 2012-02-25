print "---- bind"
a: 1
c: context [
	a: 2
	b: "two"
	bind-me: func [a] [bind a 'b]	; local a
]
print bind [a b] c
print c/bind-me [a b]


print "---- get"
probe get 'a
probe get in c 'b
probe get c


print "---- proto"
a: context [first: 1 second: 2]
b: make a [third: 3]
probe a
probe b
probe words-of b
probe values-of b


print "---- infuse"
probe infuse [nest (a b c d)] c

ia: copy a
blk: words-of ia
set blk infuse copy blk context [x: none first: "one" second: "two"]
probe ia


print "---- append"
c: context [a: 1 b: 2]
set append c 'extra 3.0
probe c


print "---- print recursion"
c1: context [a: 1 b: none]
c2: make c1 [a: 2 b: c1]
c1/b: c2
probe c1


print "---- self"
c1: context [
    f: does [self/v]
    v: 1
]
c2: make c1 [v: 2]
print [c1/f c2/f]
probe c1/self


print "---- self override"
co: context [
    self: 'override
    f: does [probe self]
]
co/f
probe co/self
