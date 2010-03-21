#!/usr/bin/boron
/*
   Print C enum statements for each context in input.
*/

hdr: make string! 1024

cc: func [word] [
    word: replace/all to-text word '-' '_'
    word/1: uppercase word/1
    word
]

uc: func [word] [
    replace/all uppercase to-text word '-' '_'
]

code: load args/1
parse code [some[
    tok: set-word! 'context block! (
        prefix: uc tok/1
        append hdr rejoin ["enum Context" cc tok/1 "^/{^/"]
        foreach v tok/3 [
            if set-word? v [
                append hdr rejoin ["    " prefix '_' uc v ",^/"]
            ]
        ]
        append hdr "};^/"
    )
    | skip
]]

prin hdr
