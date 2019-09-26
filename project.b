project: "boron"

options [
    -debug:   false         "Compile for debugging"
    assemble: false         "Enable assemble function (requires libjit)"
    checksum: true          "Enable checksum function"
    compress: 'zlib         "Include compressor ('zlib/'bzip2/none)"
    hashmap:  true          "Enable hash-map! datatype"
    execute:  true          "Enable execute function"
    random:   true          "Include random number generator"
    readline: 'linenoise    "Console editing ('linenoise/'gnu/none)"
    socket:   true          "Enable socket port!"
    ssl:      false         "Enable SSL/TLS port! (requires mbedtls)"
    static:   false         "Build static library and stand-alone executable"
    thread:   false         "Enable thread functions"
    timecode: false         "Enable timecode! datatype"
    atom-limit: 2048        "Set maximum number of words"
    atom-names: mul atom-limit 16   "Set byte size of word name buffer"
]

default [
    either -debug [debug] [release]

    objdir %obj
    include_from [%include %urlan %eval %support]

    macx [
        cflags {-std=c99}
        cflags {-pedantic}
        ;universal
    ]
    unix [
        ;cflags {-std=c99}
        cflags {-std=gnu99}    ; Try this if c99 fails.
        cflags {-pedantic}
    ]
    win32 [
        if msvc [include_from %win32]
        if thread [cflags {-D_WIN32_WINNT=0x0600}]
    ]
]

lib-spec: [
    cflags rejoin [
        {-DCONFIG_ATOM_LIMIT=} atom-limit
        { -DCONFIG_ATOM_NAMES=} atom-names
    ]
    if checksum [
        cflags {-DCONFIG_CHECKSUM}
    ]
    if eq? compress 'zlib [
        cflags {-DCONFIG_COMPRESS=1}
        win32 [libs either msvc [%zdll] [%z]]
        macx  [libs %z]
        unix  [libs %z]
    ]
    if eq? compress 'bzip2 [
        cflags {-DCONFIG_COMPRESS=2}
        win32 [libs %libbz2]
        macx  [libs %bz2]
        unix  [libs %bz2]
    ]
    if hashmap [
        cflags {-DCONFIG_HASHMAP}
        sources [%urlan/hashmap.c]
    ]
    if execute [
        cflags {-DCONFIG_EXECUTE}
    ]
    if random [
        cflags {-DCONFIG_RANDOM}
        sources [
            %support/well512.c
            %eval/random.c
        ]
    ]
    if ssl [
        socket: true
        cflags {-DCONFIG_SSL}
        libs %mbedtls
    ]
    if socket [
        cflags {-DCONFIG_SOCKET}
        sources [%eval/port_socket.c]
    ]
    if timecode [
        cflags {-DCONFIG_TIMECODE}
    ]
    if thread [
        cflags {-DCONFIG_THREAD}
        linux [libs %pthread]
        sources [%eval/port_thread.c]
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
        %vector.c

        %parse_binary.c
        %parse_block.c
        %parse_string.c
    ]

    sources [
        %support/str.c
        %support/mem_util.c
        %support/quickSortIndex.c
        %support/fpconv.c

        %eval/boron.c
        %eval/port_file.c
        %eval/wait.c
    ]

    macx  [sources [%unix/os.c]]
    unix  [
        sources [%unix/os.c]
        libs %m
    ]
    win32 [
        sources [%win32/os.c]
        ifn static [
            lflags either msvc ["/def:win32\boron.def"]["win32/boron.def"]
        ]
        libs %ws2_32
    ]
]

either static [
    exe-libs: []
    lib %boron bind lib-spec context [
        libs: func [l] [append exe-libs l]
    ]
][
    shlib [%boron 2,0,1] lib-spec
]

exe %boron [
    win32 [
        console
        libs %ws2_32
        readline: false
    ]
    libs_from %. %boron
    if static [
        foreach l exe-libs [libs l]
    ]
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
        %eval/main.c
    ]
]
