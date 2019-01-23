; Create win32/boron.def
; Run from repository root.

n: 1
type-char: make bitset! "cilsvuU"
def-format: func [sym] [
    append def format ["   " 26 '@' 1 '^/'] [sym ++ n]
]
process-header: func [f file! prefix string!] [
    parse read/text f [
        thru {extern "C"}
        thru {#endif}
        some [
            "#define" thru '^/'
          | '^/'
          | type-char to prefix tok: to '(' :tok thru ");^/" (def-format tok)
        ]
    ]
]

def: make string! 8192
append def
{LIBRARY      boron.dll
EXPORTS
}
process-header %include/boron.h "boron_"
process-header %include/urlan.h "ur_"
foreach sym [
    find_uint8_t
    find_uint16_t
    find_uint32_t
    find_pattern_8
    find_pattern_ic_8
    find_pattern_16
    find_pattern_ic_16
    unset_bind
    unset_compare
    unset_copy
    unset_destroy
    unset_make
    unset_mark
    unset_operate
    unset_select
    unset_toShared
    unset_toString
    binary_copy
    binary_mark
    binary_toShared
    block_markBuf
    cfunc_free
    vec3_toString
][
    def-format sym
]
append def ';' def-format 'ur_stringToTimeCode
write %win32/boron.def def
