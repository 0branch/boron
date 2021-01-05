#!/usr/bin/boron -s
; Bump Boron version.

old: 2,0,4
new: 2,0,5

files: [
    %INSTALL                ["VER=$v"]
    %boron.spec             ["Version: $v"]
    %project.b              ["%boron $c"]
    %eval/boot.b            ["version: $c"]
    %doc/UserManual.md      ["Version $v, "]
    %doc/boron.troff        ["Version $v" "boron $v"]
    %include/boron.h [
        {BORON_VERSION_STR  "$v"}
        {BORON_VERSION      0x0$m0$i0$r}
    ]
    %include/urlan.h [
        {UR_VERSION_STR  "$v"}
        {UR_VERSION      0x0$m0$i0$r}
    ]
]

mrule: func [version coord!] [
    replace: reduce [
        str: mold version       ; $c  Coordinate version "1,2,3"
        construct str [',' '.'] ; $v  Program version    "1.2.3"
        first  version          ; $m  Major version      "1"
        second version          ; $i  Minor version      "2"
        third  version          ; $r  Revision           "3"
    ]
    blk: make block! 10
    foreach it "cvmir" [
        append append blk join '$' it first ++ replace
    ]
    blk
]

old-rules: mrule old
new-rules: mrule new

crule: func [spec] [
    blk: make block! 4
    foreach it spec [
        append blk construct it old-rules
        append blk construct it new-rules
    ]
    blk
]

foreach [f mod] files [
   ;probe crule mod
    write f construct read/text f crule mod
]
print "Now run eval/mkboot, adjust manual dates, and make docs."
