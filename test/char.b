print "---- basic"
c: 'a'
print [type? c c]
probe c

print "---- escape seq"
b: ['^0' '^D' '^-' '^/' '^^' '^'']
probe b
print ["size? b:" size? b]
print ["first line" '^/' '^-' "second line"]

print "---- escape hex"
eh: ['^(a9)' '^(0B6)' '^(0439)' '^(220b)']
probe eh
print eh
foreach c eh [prin c] prin '^/'
print to-string eh
