print "---- Multiple set"
a: b: c: true
print [a b c]
set [e f g] [1 2]
print [e f g]


print "---- Mixed Case"
probe ['MixedCase A_very_long_word_of_40_characters-yes_40]


print "---- set func"
set 'wx none
print wx

words: [x y]
set words 21
print words
