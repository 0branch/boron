#!/usr/bin/boron -s
; Bump Version v1.1

usage: {{
Usage: bump-version [OPTIONS]

Options:
  -b            Use built-in specification.
  -f <file>     Use version specification file.  (default: ./version-up.b)
  -h            Print this help and quit.
  -r            Revert to old version; reverse mapping of old to new.
}}

spec-file: %version-up.b
finish: none
vorder: [old new]

forall args [
    switch first args [
        "-b" [spec-file: none]
        "-f" [spec-file: to-file second ++ args]
        "-h" [print usage quit]
        "-r" [swap vorder]
    ]
]

either spec-file [
    do spec-file
][
    old: 2,0,6
    new: 2,0,8
    files: [
        %Makefile               ["VER=$v"]
        %dist/boron.spec        ["Version: $v"]
        %dist/control           ["Version: $v"]
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
    finish: [
        print "Now run eval/mkboot, adjust manual dates, and make docs."
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


old-rules: mrule get first  vorder
new-rules: mrule get second vorder

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
do finish
