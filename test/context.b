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
probe b/'words


print "---- infuse"
probe infuse [nest (a b c d)] c

ia: copy a
blk: ia/'words
set blk infuse copy blk context [x: none first: "one" second: "two"]
probe ia
