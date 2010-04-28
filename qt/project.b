
exe %boron-qt [
    warn
    debug
    ;release
    objdir %obj

    qt [gui]
    include_from [%. %.. %../urlan %../support %../util]
    libs_from %.. %boron

    macx [
        libs %bz2
    ]
    linux [
        libs %bz2
    ]
    win32 [
        include_from %../win32
        sources_from %../win32 [%win32console.c]
        libs %ws2_32
    ]

    sources [
        %main.cpp
        %boron-qt.cpp
        %UTreeModel.cpp
        %../util/CBParser.c
    ]
]

