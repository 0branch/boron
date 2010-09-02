print "---- compare"
probe equal? true true
probe equal? false false
probe equal? true false
probe ne? true true
probe ne? false false 
probe ne? true false
a: true
b: true
probe eq? a b
probe eq? not a not b
