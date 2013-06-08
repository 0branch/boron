print "---- make binary"
probe construct binary! [big-endian 1 2 3 4]
probe construct binary! [little-endian 1 2 3 4]
probe construct binary! [big-endian u16 1 2 3 4]


print "---- append binary"
blk: [23 43 43 2309 84 823]
bin: #{FAFB}
probe construct bin [big-endian u16 blk]


print "---- copy string and edit"
inp: {This is some ... type-of "text"}
format: 'HTML
probe construct inp [
   ;replace
    '<'  "&lt;"
    '>'  "&gt;"
    '&'  "&amp;"
    '^'' "&apos;"
    '"'  "&quot;"
    " ..." none
    "type-of" format
]
