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


print "---- parse into"
name: age: x: none
print parse [person ["Janet" 38] "-x" 20.2 ] [some[
    'person into [set name skip set age skip]
  | "-x" set x skip 
]]
probe reduce [name age x]


print "---- sanity checks"
ogs: "frog clog dog smog bog woggle toggle"
print parse copy ogs [
    some [a: "smog" (clear a) | skip]
]


print "---- string case"
in: {Mixed case from FROM From}
rules: [some[ "From" (++ count) | skip ]]
count: 0  parse      in rules  print count
count: 0  parse/case in rules  print count
rules: [some[ thru "From" (++ count) ]]
count: 0  parse      in rules  print count
count: 0  parse/case in rules  print count


print "---- string UCS2"
str: {Рабочий стол Plasma, Чувар екрана}
a: b: none
parse str [thru "стол" some white a: to ',' :a]
probe a
parse str [to "Plasma" b: to #{2C20} :b]
probe b
