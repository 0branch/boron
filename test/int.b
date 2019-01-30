print "---- tokenize"
probe [0 +0 -0 +1 -1]
print try [do {32word}]

print "---- hex"
probe [0x0 0x10 0xffffffff]
print type? 0xffffffff
print try [do {0x}]

print "---- format"
print [to-dec 0x10 to-dec 0xffffffff  to-dec 0xffffffffffffffff]
print [to-hex 16   to-hex 4294967295  to-hex -1]

print "---- convert"
print to-int "-34 j"        ; Conversion stops at non-digit
print to-int "0xFF08f201"
