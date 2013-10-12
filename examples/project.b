project: "examples"

default [
    debug
   ;release

    objdir %obj

    include_from [%../include %../urlan]
    libs_from %.. %boron

    macx [
        cflags {-std=c99}
        cflags {-pedantic}
        libs {m}
        universal
    ]
    unix [
        cflags {-std=c99}
        ;cflags {-std=gnu99}    ; Try this if c99 fails.
        cflags {-pedantic}
        libs {m}
    ]
    win32 [
        include_from %../win32
        console
    ]
]

exe %calculator [
    sources [%calculator.c]
]

exe %boron_mini [
    sources [%boron_mini.c]
]
