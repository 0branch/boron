print "---- string parse"
white: make bitset! " ."
word:  make bitset! "abcdefghijklmnopqrstuvwxyz"

print parse {   token...} [
	any white a: some word :a any white (print "eval OK")
]
print a


print "---- basic tokenizer"
white: charset " ^-^/"
non-white: complement copy white
words: []
print parse {some words ...} [some[
    any white a: some non-white :a (append words a)
]]
probe words


print "---- block parse"
print parse [1 2 token ...] [
	any int! a: word! :a any word!
]
probe a
