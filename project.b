project: "boron"

assemble: false
checksum: true
compress: 'zlib
execute:  true
random:   true
readline: 'linenoise
socket:   true
thread:   false
timecode: false

do-any %project.config

default [
   ;debug
    release

    objdir %obj
    include_from [%. %urlan %support]

    macx [
        cflags {-std=c99}
        cflags {-pedantic}
        universal
    ]
    unix [
        ;cflags {-std=c99}
        cflags {-std=gnu99}    ; Try this if c99 fails.
        cflags {-pedantic}
    ]
    win32 [
        include_from %win32
    ]
]

shlib [%boron 0,2,4] [
    if checksum [
        cflags {-DCONFIG_CHECKSUM}
    ]
    if eq? compress 'zlib [
        cflags {-DCONFIG_COMPRESS=1}
        win32 [libs %zdll]
        macx  [libs %z]
        unix  [libs {z m}]
    ]
    if eq? compress 'bzip2 [
        cflags {-DCONFIG_COMPRESS=2}
        win32 [libs %libbz2]
        macx  [libs %bz2]
        unix  [libs {bz2 m}]
    ]
    if execute [
        cflags {-DCONFIG_EXECUTE}
    ]
    if random [
        cflags {-DCONFIG_RANDOM}
        sources [%support/well512.c]
    ]
    if socket [
        cflags {-DCONFIG_SOCKET}
        sources [%port_socket.c]
    ]
    if timecode [
        cflags {-DCONFIG_TIMECODE}
    ]
    if thread [
        cflags {-DCONFIG_THREAD}
        linux [libs %pthread]
        sources [%port_thread.c]
    ]
    if assemble [
        cflags {-DCONFIG_ASSEMBLE}
        libs %jit
    ]
    ;cflags {-DTRACK_MALLOC} sources [%urlan/memtrack.c]

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
        %serialize.c
        %tokenize.c
        %bignum.c
        %vector.c

        %parse_binary.c
        %parse_block.c
        %parse_string.c
    ]

    sources [
        %support/str.c
        %support/mem_util.c
        %support/quickSortIndex.c

        %boron.c
        %port_file.c
    ]

    macx  [sources [%unix/os.c]]
    unix  [sources [%unix/os.c]]
    win32 [
        sources [%win32/os.c]
        lflags "/def:win32\boron.def"
        libs %ws2_32
    ]
]

exe %boron [
    win32 [
        console
        libs %ws2_32
        readline: false
    ]
    libs_from %. %boron
    switch readline [
        linenoise [
            cflags {-DCONFIG_LINENOISE}
            sources [%support/linenoise.c]
        ]
        gnu [
            cflags {-DCONFIG_READLINE}
            libs [%readline %history]
        ]
    ]
    sources [
        %boron_console.c
    ]
]
