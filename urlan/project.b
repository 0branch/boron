project: "urlan"

timecode: false

do-any %project.config

default [
   ;debug
    release

    objdir %obj
    include_from [%. %../support]

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
        include_from %../win32
    ]
]

shlib %urlan [
    if timecode [
        cflags {-DCONFIG_TIMECODE}
    ]
    ;cflags {-DTRACK_MALLOC} sources [%memtrack.c]

    sources_from %../support [
        %str.c
        %mem_util.c
    ]

    sources [
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

    macx [libs %m]
    unix [libs %m]
    win32 [
        lflags "/def:win32\urlan.def"
    ]
]

exe %calc [
    linux [lflags {-Wl,-z,origin,-rpath,'$$ORIGIN/'}]
    win32 [console]
    libs_from %. %urlan
    sources [ %../examples/calculator.c]
]
