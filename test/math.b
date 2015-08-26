print "---- print decimal"
print [0.0 -0.0 1.0 -1.0 9.2 -9.2]
print [0.0005 5e-4 0.000005 5e-6 0.0000005 5e-7 0.0000000005 5e-10]
print [343043.003432 222111.00343201 2221190.0034320109]
print [-9.45e-06 9.0e+200 -9e-200 1e+308 1e+309 1e-311 /*1e-312*/]

print "---- rounding"
print div add 5 4 2     ; Rounded.
print div add 5 4 2.0   ; Not rounded.
print div add 5.0 1 2

print "---- char"
probe add '0' 3
probe add 0 '3'

print "---- vec3"
print add 0.0,1.1,0.5 1.8,3,-5.5
print mul 4.3,8,11.34 2.5

print "---- block"
print add 0 [1 2 3 4 5]
print add 0.0 [1.0 20.0 3]
print or 0 [0x10 0x03 0x8000]
