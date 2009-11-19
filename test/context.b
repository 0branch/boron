print "---- bind"
a: 1
c: context [
	a: 2
	b: "two"
	bind-me: func [a] [bind a 'b]	; local a
]
print bind [a b] c
print c/bind-me [a b]


print "---- proto"
a: context [first: 1 second: 2]
b: make a [third: 3]
probe a
probe b


print "---- infuse"
probe infuse [nest (a b c d)] c
