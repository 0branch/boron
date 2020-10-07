white: charset " ^-^/"

print "---- not found"
probe split "a bit of time" ':'
probe split "heel-on-shoe" white
probe split #{CC00DEAD00BEEF} 0x01
probe split [blk | of time | blah] 'x

print "---- normal"
probe split "a bit of time" ' '
probe split "heel on^/shoe" white
probe split #{CC00DEAD00BEEF} 0x00
probe split [blk | of time | blah] '|

print "---- subsequent delim"
probe split "  a  bit   of  time  " ' '
probe split "^/^-heel^/^-on   shoe^/^- " white
probe split #{0000CC0000DEAD000000BEEF0000} 0x00
probe split [+ + blk + + of time + + + blah + +] '+
