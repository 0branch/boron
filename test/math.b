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
