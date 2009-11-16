
exe %boron-qt [
    warn
    debug
    ;release
    objdir %obj

    qt [gui]
    include_from [%. %.. %../urlan %../support %../util]
    libs_from %.. [%boron0]

    macx [
        libs %bz2
    ]
    linux [
        libs %bz2
    ]
    win32 [
        include_from %../win32
        sources_from %../win32 [%win32console.c]
    ]

    sources [
        %main.cpp
        %boron-qt.cpp
        %../util/CBParser.c
    ]
]

