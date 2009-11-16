project "examples"

default [
    warn
    debug
   ;release

    objdir %obj

    include_from %..

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
    libs_from %.. %urlan
    sources [%calculator.c]
]


exe %boron_mini [
    libs_from %.. [%boron0 %urlan]
    sources [%boron_mini.c]
]
