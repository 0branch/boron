; m2 2.0.2 Mac OS X target


macx: func [blk] [do blk]

bundle:    does [ct/cfg_bundle: true]
universal: does [ct/cfg_universal: true]

generate_makefile: does [
    emit-makefile "GNU Makefile for Mac OSX"
{AS       = as
CC       = gcc  # cc
CXX      = g++  # c++
LINK     = gcc  # cc
LINK_CXX = g++  # c++
MOC      = $(QTDIR)/bin/moc
RCC      = $(QTDIR)/bin/rcc
TAR      = tar -cf
ZIP      = gzip -9f
}
]


qt-fw: func [name] [
    include_from rejoin ["/Library/Frameworks/" name ".framework/Headers"]
    lflags join "-framework " name
]

qt-libs: does [
    context pick [[
        concurrent:  does [qt-fw "QtConcurrent"]
        core:        does [qt-fw "QtCore"]
        gui:         does [qt-fw "QtGui"]
        network:     does [qt-fw "QtNetwork"]
        opengl:      does [qt-fw "QtOpenGL"]
        printsupport: does [qt-fw "QtPrintSupport"]
        svg:         does [qt-fw "QtSvg"]
        sql:         does [qt-fw "QtSql"]
        widgets:     does [qt-fw "QtWidgets"]
        xml:         does [qt-fw "QtXml"]
    ][
        gui:     does [qt-fw "QtGui"]
        network: does [qt-fw "QtNetwork"]
        opengl:  does [qt-fw "QtOpenGL"]
        xml:     does [qt-fw "QtXml"]
        svg:     does [qt-fw "QtSvg"]
        sql:     does [qt-fw "QtSql"]
        support: does [
            cxxflags {-DQT3_SUPPORT}
            qt-fw "Qt3Support"
        ]
    ]] eq? 5 qt-version
]

qt-static-libs: context [
   qlib: func [name] [
       include_from join {$(QTDIR)/include/} name
       lflags join {-l} name
   ]
   core:    does [qlib {QtCore}]
   gui:     does [qlib {QtGui}]
   network: does [qlib {QtNetwork}]
   opengl:  does [qlib {QtOpenGL}]
   xml:     does [qlib {QtXml}]
   svg:     does [qlib {QtSvg}]
   sql:     does [qlib {QtSql}]
   support: does [qlib {Qt3Support} cxxflags {-DQT3_SUPPORT}]
]

exe_target: make target_env
[
    ; built_obj_rule exists only to hold the output make_obj_rule
    built_obj_rule: none

    cfg_bundle: none
    cfg_universal: none

    obj_macro: none

    append defines "__APPLE__"

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

        /*
        cflags {-isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5}
        lflags {-Wl,-syslibroot,/Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5}
        */
        if cfg_universal [
            cflags {-isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386 -mmacosx-version-min=10.4}
            lflags {-Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386 -mmacosx-version-min=10.4}
        ]

        if cfg/opengl [
            lflags {-framework OpenGL}
        ]

        if cfg/qt [
            either cfg/qt-static [
                include_from {$(QTDIR)/include}
                lflags {-L$(QTDIR)/lib}
                lflags {-framework Carbon -lz -liconv}
                do bind copy cfg/qt qt-static-libs
            ][
                cflags {-F/Library/Frameworks}
                lflags {-F/Library/Frameworks}
                lflags {-framework Carbon}
                do bind copy cfg/qt qt-libs
            ]
            if cfg/release [
                cxxflags {-DQT_NO_DEBUG}
            ]
        ]

        if cfg/x11 [
            include_from {/usr/X11R6/include}
            libs_from %/usr/X11R6/lib {Xext X11 m}
        ]
    ]

    configure: does [
        output_file: rejoin either cfg_bundle
                        [[name %.app/Contents/MacOs/ name]]
                        [[output_dir name]]
        do config
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
        {^/^-$(}
            either link_cxx ["LINK_CXX"]["LINK"]
            {) -o $@ $(} uc_name {_LFLAGS) } obj_macro
        { $(} uc_name {_LIBS)} eol

        either cfg_bundle [
            init-bundle name rejoin bind info.plist 'name
            ""
        ] ""
    ]

    init-bundle: func [app-name plist string!] [
        bdir: to-file join app-name %.app/Contents/
        ifn exists? bdir [
            make-dir/all join bdir %MacOs
            write join bdir %PkgInfo #{4150504c 3f3f3f3f 0a}
            write join bdir %Info.plist plist
        ]
    ]
]


lib_target: make exe_target [
    configure: does [
        output_file: rejoin [output_dir "lib" name ".a"]
        do config
    ]

    rule_text: [
        output_file ": " obj_macro sub-project-libs link_libs
        "^/^-libtool -static -o $@ $^^ $(" uc_name "_LIBS)"
        "^/^-ranlib $@^/"
    ]
]


shlib_target: make exe_target [
    configure: does [
        output_file: rejoin [output_dir "lib" name ".dylib"]
        do config
        ;cflags {-fPIC}     ; Default on Mac.
        lflags join "-install_name @rpath/" output_file
    ]

    rule_text: [
        output_file ": " obj_macro sub-project-libs link_libs
            {^/^-$(} either link_cxx ["LINK_CXX"]["LINK"]
                {) -dynamiclib -o $@ } obj_macro
            { $(} uc_name {_LFLAGS) }
            { $(} uc_name {_LIBS)} eol
    ]
]


info.plist: [
{<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD
PLIST 1.0//EN"
"http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>} name {</string>
    <key>CFBundleIdentifier</key>
    <string>com.mycompany.} name {</string>
    <key>CFBundleDisplayName</key>
    <string>} uc_name {</string>
    <key>CFBundleName</key>
    <string>} uc_name {</string>
    <key>CFBundleIconFile</key>
    <string>} name {</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
</dict>
</plist>
}
]


;EOF
