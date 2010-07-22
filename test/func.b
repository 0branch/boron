print "---- does"
hello: does [print 'hello]
hello

print "---- func"
add1: func [a b] [add add a b 1]
print add1 4 5 


print "---- nested"
r1: func [a b c] [hello mul c add1 a b]
print r1 4 5 2


print "---- locals"
a: 2
fl: func [x | a] [print [a x] a: 3 print a]
fl 8
print a 

f2: func [v] [reduce v]
f1: func [v | local-f1] [local-f1: 44 f2 [v local-f1]]
probe f1 "str"


print "---- return"
f: func [x] [if gt? x 4 [return "x > 4"] "x < 4"]
print f 1
print f 5


print "---- recursion"
factorial: func [x]
[
    if lt? x 2 [return 1]
    mul x factorial sub x 1
]
print factorial 30

fibonacci: func [x]
[
	if lt? x 3 [return 1]
    add  fibonacci sub x 1  fibonacci sub x 2
]
print fibonacci 25  ;1000000


print "---- argument validation"
af: func [n int!][add n 1]
print af 2
print try [af 2.0]


print "---- literal arguments"
num: 0
lf: func [n int! 'arg word!] [set arg add n 1]
lf 44 num
print num


print "---- options"
; Test option alone and that result is logic!.
optf: func [/opt] [print pick ["option set" "no option"] opt]
optf
optf/opt

optf: func [n int! /two] [add n either two [2][1]]
print optf 6 num
print optf/two 6 num


print "---- optional arguments"
oaf: func [a /op 'f b] [
    either op [
        print do f a b
    ][
        print [a f b]
    ]
]
oaf 1
oaf/op 10 add 3
oaf/op 10 sub 3


print "---- empty body"
ef:  func [blk block!] []
ef [1 2]
ef:  does []
ef


print "---- compare"
print equal? :ef :oaf
print same?  :ef :oaf
print equal? :ef :ef
print same?  :ef :ef


print "---- bugs fixed"
; Found problem in makeVerifyFuncBlock().
sources_from: func [path files block!] [verify_slash path]

; Optional argument type checking
oat: func [a int! /opt b string!] [
    if opt [return b]
    a
]
print oat 4
print oat/opt 4 "opt-b"
