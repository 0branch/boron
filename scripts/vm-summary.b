#!/usr/bin/boron
;  Summarize Valgrind Massif files.

eol: '^/'
heap: 0

parse read/text first args [
    thru "cmd: " cmd: to eol :cmd
    some [
        thru "time=" t: to eol :t (
            time: to-int t time
        )
        thru "mem_heap_B=" t: to eol :t (
            hb: to-int t
        )
        thru "mem_heap_extra_B=" t: to eol :t (
            hb: add hb to-int t
            if gt? hb heap [heap: hb]
        )
    ]
]

pad: func [v] [
    v: to-string v
    rejoin [skip "          " size? v v]
]

print [pad time pad heap cmd]
