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


print "---- make"
probe to-word int!
;probe to-word int!/decimal!


print "---- toText"
words: [a 'b c: :d]
foreach w words [prin w prin ' '  probe w]


print "---- tokenize"
words: [+ +1 +a+ - -1 -a- * & / /a]  ; '/ /:
foreach w words [print [w type? w]]


print "---- lit-words"
probe ['= '== '!= '> '< '<= '>=]
