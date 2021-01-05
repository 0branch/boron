; m2 2.0.2 Linux-g++ target


system-qt: true

linux:
unix:  func [blk] [do blk]

generate_makefile: does [
    emit-makefile "GNU Makefile for Linux-g++" join
{AS       = as
CC       = gcc -Wdeclaration-after-statement
CXX      = g++
LINK     = gcc
LINK_CXX = g++
TAR      = tar -cf
ZIP      = gzip -9f
}
    case [
        not system-qt {{
            MOC      = $(QTDIR)/bin/moc
            RCC      = $(QTDIR)/bin/rcc
            QTINC    = $(QTDIR)/include
        }}
        eq? qt-version 5 {{
            MOC      = moc-qt5
            RCC      = rcc-qt5
            QTINC    = /usr/include/qt5
        }}
        true {{
            MOC      = moc-qt4
            RCC      = rcc-qt4
            QTINC    = /usr/include
        }}
    ]
]


qt-includes: context [
    concurrent: " $(QTINC)/QtConcurrent"
    core:       " $(QTINC)/QtCore"
    gui:        " $(QTINC)/QtGui"
    network:    " $(QTINC)/QtNetwork"
    opengl:     " $(QTINC)/QtOpenGL"
    printsupport: " $(QTINC)/QtPrintSupport"
    svg:        " $(QTINC)/QtSvg"
    sql:        " $(QTINC)/QtSql"
    support: does [
        cxxflags "-DQT3_SUPPORT"
        " $(QTINC)/Qt3Support"
    ]
    widgets: does [
        if lt? qt-version 5 [return ""]
        " $(QTINC)/QtWidgets"
    ]
    xml:        " $(QTINC)/QtXml"
]

qt-libraries: func [debug | blk it] [
    blk: select [
        5 [
            concurrent: " Qt5Concurrent"
            core:    " Qt5Core"
            gui:     " Qt5Gui"
            network: " Qt5Network"
            opengl:  " Qt5OpenGL"
            printsupport: " Qt5PrintSupport"
            svg:     " Qt5Svg"
            sql:     " Qt5Sql"
            widgets: " Qt5Widgets"
            xml:     " Qt5Xml"
        ]
        4 [
            core:    " QtCore"
            gui:     " QtGui"
            network: " QtNetwork"
            opengl:  " QtOpenGL"
            svg:     " QtSvg"
            sql:     " QtSql"
            support: " Qt3Support"
            xml:     " QtXml"
        ]
    ] qt-version

    if debug [
        map it blk [
            either string? it [join it "_debug"][it]
        ]
    ]
    context blk
]


exe_target: make target_env
[
    obj_macro: none

    append defines "__linux__"

    config:
    [
        obj_macro: rejoin ["$(" uc_name "_OBJECTS)"]

        cflags {-pipe}

        if cfg/warn [
            cflags {-Wall -W}
        ]

        if cfg/debug [
            cflags {-g -DDEBUG}
        ]

        if cfg/release [
            cflags {-O3 -DNDEBUG}
        ]

        if cfg/opengl [
            libs "GL"
        ]

        if cfg/qt [
            include_from {$(QTINC)}
            include_from rejoin bind copy cfg/qt qt-includes

            if eq? 5 qt-version [cxxflags {-fPIC}]
            if cfg/release [
                cxxflags {-DQT_NO_DEBUG}
            ]
        ]

        if cfg/x11 [
            include_from {/usr/X11R6/include}
            libs_from %/usr/X11R6/lib {Xext X11 m}
        ]
    ]

    configure: func [| qt-libs] [
        output_file: rejoin [output_dir name]
        do config
        if cfg/qt [
            qt-libs: rejoin bind copy cfg/qt qt-libraries cfg/debug
            either system-qt [
                libs qt-libs
            ][
                libs_from "$(QTDIR)/lib" qt-libs
            ]
        ]
    ]

    macro_text: does [
        ifn empty? menv_aflags [
            emit [uc_name "_AFLAGS   = " menv_aflags eol]
        ]

        emit [
            uc_name "_CFLAGS   = " menv_cflags eol
            uc_name "_CXXFLAGS = $(" uc_name "_CFLAGS) " menv_cxxflags eol
            uc_name "_INCPATH  = " gnu_string "-I" include_paths eol
            uc_name "_LFLAGS   = " menv_lflags eol
            uc_name "_LIBS     = " gnu_string "-L" link_paths ' '
                    gnu_string "-l" link_libs eol
        ]

        ;if cfg/qt [
        ;    emit [
        ;        uc_name "_SRCMOC  = " expand_list_gnu srcmoc_files eol
        ;        uc_name "_OBJMOC  = " expand_list_gnu objmoc_files eol
        ;    ]
        ;]

        emit [
            ;uc_name "_HEADERS  = " expand_list_gnu header_files eol
            uc_name "_SOURCES  = " expand_list_gnu source_files eol
            uc_name "_OBJECTS  = " expand_list_gnu object_files eol
        ]
    ]

    clean: does [
        rejoin [
            "^--rm -f " output_file ' ' obj_macro
            either empty? srcmoc_files [""] [join ' ' to-text srcmoc_files]
            eol
        ]
    ]

    dist: does [
        rejoin [" $(" uc_name "_SOURCES)"]
    ]

    rule_text: [
        output_file ": " obj_macro local_libs link_libs
            sub-project-libs link_libs
        {^/^-$(} either link_cxx ["LINK_CXX"]["LINK"]
            {) -o $@ $(} uc_name {_LFLAGS) } obj_macro
        { $(} uc_name {_LIBS)} eol
    ]
]


lib_target: make exe_target [
    configure: does [
        output_file: rejoin [output_dir "lib" name ".a"]
        do config
    ]

    rule_text: [
        output_file ": " obj_macro sub-project-libs link_libs
        rejoin pick [[
            "^/^-ar rc $@ " obj_macro " $(" uc_name "_LFLAGS)"
        ][
            ; Concatenate other libraries.
            "^/^-ld -Ur -o " objdir name "lib.o $^^ $(" uc_name
                "_LIBS) $(" uc_name "_LFLAGS)"
            "^/^-ar rc $@ " objdir name "lib.o"
        ]]
            empty? link_libs
        "^/^-ranlib $@^/"
        either cfg/release "^-strip -d $@^/" ""
    ]
]


shlib_target: make exe_target [
    version: lib_full: lib_m: lib_base: none
    _exe_clean: none

    configure: does [
        lib_full: rejoin ["lib" name ".so"]
        if version [
            lib_base: lib_full
            lib_m:    rejoin [lib_base '.' first version]
            lib_full: rejoin [
                ; Handles any number of version elements.
                lib_base '.' replace/all to-string version ',' '.'
            ]
        ]
        output_file: join output_dir lib_full
        do config
        cflags {-fPIC}
        lflags {-shared}
        if version [
            lflags join {-Wl,-soname,} lib_m

            rule_text: join rule_text [
                "^-ln -sf " lib_full ' ' lib_m    '^/'
                "^-ln -sf " lib_full ' ' lib_base '^/'
            ]
            _exe_clean: :clean
            clean: does [
                rejoin [slice _exe_clean -1 ' ' lib_m ' ' lib_base eol]
            ]
        ]
    ]
]


;EOF
