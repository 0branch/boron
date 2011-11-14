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
eh: ['^xa9' '^x0B6' '^x0439' '^x220b']
probe eh
print eh
foreach c eh [prin c] prin '^/'
print to-string eh
