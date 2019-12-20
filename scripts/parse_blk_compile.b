#!/usr/bin/boron
; parse_blk_compile v1.0
;
; Compile rules into ur_parseBlockI() bytecode.

;--------------------------------------------------------------------------
; Parse arguments & load rules

file: none
prefix: ""
extract: false

forall args [
    switch first args [
        "-h" [
            print {{
                Usage: parse_blk_compile.b [-p <prefix>] [-e] <file>^/
                Options:
                  -e  Extract rules embedded in C comment.
                  -p  Module name prefix.  (none by default)
            }}
            quit
        ]
        "-e" [
            file: second ++ args
            extract: true
        ]
        "-p" [
            prefix: second ++ args
        ][
            rules: load first args
        ]
    ]
]

if extract [
    rules: none
    parse read/text file [
        thru "parse_blk_compile" thru '^/' rules: to "*/" :rules
    ]
    ifn rules [
        print ["No parse_blk_compile comment found in " file]
        return/quit 1
    ]
    rules: to-block rules
]


;--------------------------------------------------------------------------
; Compile rules to bytecode fragments

enum: 0
foreach it [    ; Matches ParseBlockInstruction from i_parse_blk.h
    PB_End
    PB_Flag
    PB_Report
    PB_ReportEnd
    PB_Next
    PB_Skip
    PB_LitWord
    PB_Rule
    PB_Type
    PB_Typeset
    PB_OptR
    PB_OptT
    PB_OptTs
    PB_AnyR
    PB_AnyT
    PB_AnyTs
    PB_SomeR
    PB_SomeT
    PB_SomeTs
    PB_ToT
    PB_ToTs
    PB_ToLitWord
    PB_ThruT
    PB_ThruTs
][
    set it ++ enum
]

bin: none
frag: []
last-ins: none
emit:  func [ins /extern last-ins] [append bin last-ins: ins]
emit2: func [ins dat /extern last-ins] [append append bin last-ins: ins dat]

; Get first word of each paren.
reports: map it collect paren! rules [first it]
reports: union reports reports
report-index: func [word] [
    add -1 index? find reports word
]

atoms: collect/unique lit-word! rules
atom-index: func [word] [
    add -1 index? find atoms word
]

typesets: []
offset-loc: []      ; Locations to insert correct offset.

sub-block: func [ins blk block! /extern bin] [
    append offset-loc tail bin
    emit2 ins size? frag            ; Offset to be replaced.

    save-bin: bin
    compile-blk blk
    bin: save-bin
]

type-word: func [ins word word!] [
    ai: index? word
    if lt? ai 64 [emit2 ins ai]
]

typeset: func [ins type datatype!] [
    append offset-loc tail bin
    mask: index? type
    either pos: find typesets mask [
        emit2 ins sub index? pos 1      ; Offset to be replaced.
    ][
        emit2 ins size? typesets        ; Offset to be replaced.
        append typesets mask
    ]
]

end-rule: does [
    either eq? last-ins PB_Report [
        change skip tail bin -2 PB_ReportEnd
    ][
        emit PB_End
    ]
    last-ins: none
]

compile-blk: func [blk /extern bin] [
    append frag bin: make binary! 32

    if alt: find blk '| [
        emit2 PB_Next 0xff              ; Skip to be replaced.
        next-loc: tail bin
    ]

    parse blk [some[
        it:
        paren!          (emit2 PB_Report report-index it/1/1)
      | '| (
            end-rule
            poke next-loc -1 sub size? bin sub index? next-loc 1
            if alt: find next alt '| [
                emit2 PB_Next 0xff      ; Skip to be replaced.
                next-loc: tail bin
            ]
        )
      | block!          (sub-block PB_Rule  first  it)
      | 'any  block!    (sub-block PB_AnyR  second it)
      | 'opt  block!    (sub-block PB_OptR  second it)
      | 'some block!    (sub-block PB_SomeR second it)
      | 'skip           (emit PB_Skip)
      | 'flag int!      (emit2 PB_Flag second it)
      | lit-word!       (emit2 PB_LitWord   atom-index first  it)
      | 'to lit-word!   (emit2 PB_ToLitWord atom-index second it)
      | datatype!       (typeset PB_Typeset first  it)
      | 'any  datatype! (typeset PB_AnyTs   second it)
      | 'opt  datatype! (typeset PB_OptTs   second it)
      | 'some datatype! (typeset PB_SomeTs  second it)
      | 'to   datatype! (typeset PB_ToTs    second it)
      | 'thru datatype! (typeset PB_ThruTs  second it)
      | 'any  word!     (type-word PB_AnyT  second it)
      | 'opt  word!     (type-word PB_OptT  second it)
      | 'some word!     (type-word PB_SomeT second it)
      | 'to   word!     (type-word PB_ToT   second it)
      | 'thru word!     (type-word PB_ThruT second it)
      | word!           (type-word PB_Type  first  it)
      | (error "Invalid parse_blk rule")
    ]]

    ifn empty? bin [
        end-rule
    ]
]

compile-blk rules
;probe frag

; Fixup offsets.
frag-offsets: []
frag-total: 0
foreach it frag [
    append frag-offsets frag-total
    frag-total: add frag-total size? it
]

fixup-offset: does [
    poke it 2 pick frag-offsets add 1 pick it 2
]
fixup-map: reduce [
    PB_Typeset [
        poke it 2 add frag-total mul 8 pick it 2
    ]
    PB_Rule  [fixup-offset]
    PB_OptR  [fixup-offset]
    PB_AnyR  [fixup-offset]
    PB_SomeR [fixup-offset]
]
foreach it offset-loc [
    switch first it fixup-map
]


;--------------------------------------------------------------------------
; Print C source

indent: "    "
cstr: make string! 64
cstr-emit: func [byte-str] [
    append append append cstr "0x" slice byte-str 2 ", "
]

cbytes: func [bin] [
    clear cstr
    str: slice mold bin 2,-1
    while [not empty? str] [
        cstr-emit str
        str: skip str 2
    ]
    slice cstr -1
]

rev-cbytes: func [str] [
    clear cstr
    str: tail str
    while [not head? str] [
        str: skip str -2
        cstr-emit str
    ]
    slice cstr -1
]

bitset64: func [n int!] [
    str: skip mold to-hex n 2
    rev-cbytes join skip "0000000000000000" size? str str
]

uc: func [str] [uppercase copy str]

prefix-up: either empty? prefix [prefix] [join uc prefix '_']

print rejoin ["enum " prefix "RulesReport^/{"]
foreach it reports [
    print rejoin [indent prefix-up "REP_" uc to-text it ',']
]
print "};^/"

print rejoin ["/*^/enum " prefix "RulesAtomIndex^/{"]
foreach it atoms [
    print rejoin [indent prefix-up "AI_" uc to-text it ',']
]
print "};^/*/^/"

print rejoin ["static const uint8_t " prefix "ParseRules[] =^/{"]
foreach it frag [
    print rejoin [
        indent "// " first ++ frag-offsets '^/'
        indent cbytes it
    ]
]
print ["    //" frag-total "- Typesets"]
foreach it typesets [
    print join indent bitset64 it
]
print "};"
