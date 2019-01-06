print "---- if"
print if same?  2 add 1 1 ["same"]
print if same?  2 3 ["same"]

print if equal? 2 add 1 1 ["equal"]
print if equal? 2 1 ["equal"]

probe if lt? 2 3 "2 < 3"
probe if lt? 4 2 "4 < 2"

probe if gt? 4 2 "4 > 2"
probe if gt? 2 3 "2 > 3"


print "---- either"
print either true  ['yes]['no]
print either false ['yes]['no]
a: ['eval-yes]
b: ['eval-no]
print either equal? 2 add 1 1  a b
print either true 'e-yes 'e-no
print either none 'e-yes 'e-no


print "---- all"
print all [true]
print all [false]
print all [equal? 2 add 1 1  gt? 4 2]
print all [equal? 2 add 1 1  gt? 2 4]


print "---- any"
print any [true]
print any [false]
print any [equal? 2 add 2 1  gt? 4 2]
print any [equal? 2 add 2 1  gt? 2 4]


print "---- switch"
cases: [
    0 ['zero]
    1 ['one]
    2 ['two]
    ['default]
]
print switch 1 cases
print switch 4 cases


print "---- while"
a: 3
print while [gt? a 0] [print a  -- a]
a: 3
print while [gt? a 0] [
    print a
    ++ a
    if gt? a 5 [print "Done" break]
]


print "---- case"
foreach x [3 ^ 0 box] [
    case [
        find [^ & |] x       [print ['operator x]]
        word? x              [print ['word x]]
        all [int? x gt? x 2] [print "integer > 2"]
    ]
]
