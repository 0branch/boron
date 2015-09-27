; Make _cfuncTable and serialized signatures from individual addCFunc calls.

ft:  make string! 1024
sig: make block! 512
s2: empty: ""

append ft "BoronCFunc _cfuncTable[] =^/{^/"
parse read/text %boron.c [
    thru "CFUNC_TABLE_START^/"
    some [
        "    addCFunc(" f: thru ',' :f thru '"' s: to '"' :s thru '^/'
            opt [some ' ' '"' s2: to '"' :s2 thru '^/']
        (
            append ft rejoin ["    " f '^/']

            blk: probe to-block rejoin ["^/" s s2]
            blk/1: to-set-word blk/1
            append sig blk
            s2: empty
        )
    ]
]
append ft "};^/^/"

c-array: func [name bin | out] [
    out: make string! 1024
    append out rejoin ["uint8_t " name "[] =^/{^/  // " size? bin " bytes"]
    forall bin [
        if eq? 1 and index? bin 15 [append out "^/  "]
        append out first bin
        append out ','
    ]
    append out "^/};^/"
    out
]

append ft c-array "_cfuncSigs" serialize sig
write %cfuncTable.c ft
