print "---- make binary"
probe construct binary! [big-endian 1 2 3 4]
probe construct binary! [little-endian 1 2 3 4]
probe construct binary! [big-endian u16 1 2 3 4]
probe construct binary! [#{DA7A0000} u16 1000,2000,3000 u8 1,2,3,4]
probe construct binary! [big-endian u16 1000,2000,3000 u32 1,2]


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
