project "boron"

compress: true

default [
    warn
    debug
   ;release

    objdir %obj
    include_from [%. %urlan %support]

    macx [
        cflags {-std=c99}
        cflags {-pedantic}
        universal
    ]
    unix [
        cflags {-std=c99}
        ;cflags {-std=gnu99}    ; Try this if c99 fails.
        cflags {-pedantic}
    ]
    win32 [
        include_from %win32
    ]
]

shlib %boron [
    
    if compress [
        cflags {-DCONFIG_COMPRESS}
        win32 [libs %libbz2]
    ]
    ;cflags {-DTRACK_MALLOC} sources [%urlan/memtrack.c]

    win32 [lflags "/def:win32\boron.def"]

    sources_from %urlan [
        %env.c
        %array.c
        %binary.c
        %block.c
        %coord.c
        %date.c
        %path.c
        %string.c
        %context.c
        %gc.c
        %tokenize.c
        %bignum.c

        %parse_string.c
        %parse_block.c
    ]

    sources [
        %support/str.c
        %support/mem_util.c
        %support/quickSortIndex.c
        ;%support/well512.c

        %boron.c
    ]

    macx  [sources [%unix/os.c]]
    unix  [sources [%unix/os.c]]
    win32 [sources [%win32/os.c]]
]

exe %boron [
    libs_from %. %boron
    macx  [libs {m bz2}]
    unix  [libs {m bz2}]
    win32 [
        console
        libs %ws2_32
    ]
    sources [
        %boron_console.c
    ]
]
