print "---- tokenize"
probe #[1 -22 3.55]
probe #[1.0 -22 3.55]
probe #[ 4 88/*89*/5 /*6*/]
probe #[
    1 1 1
   ;2 2 2
    3 3 3
]


print "---- append"
a: #[1 2 3]
probe append copy a 4
probe append copy a #[70 80 90]


;print "---- find"
;probe find a 2
