print "---- tokenize"
a: int!/double!/word!
print [type? a a]


print "---- type test"
print same? a int!/double!
print equal? a 4
print equal? a type? 4
print equal? a type? "str"


print "---- compare to word"
; Handled by word_compare().
print eq? int! 'int!
print eq? int! first ['int!]
print switch type? "abc" [string! "Is string" none]
print switch type? "abc" [string!/file! "Is string or file" none]


print "---- make"
probe make datatype! logic!
probe make datatype! 'logic!
