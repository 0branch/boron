print "---- basic"
c: 'a'
print [type? c c]
probe c

print "---- escape seq"
b: ['^0' '^D' '^-' '^/' '^^' '^'']
probe b
print ["size? b:" size? b]
print ["first line" '^/' '^-' "second line"]
