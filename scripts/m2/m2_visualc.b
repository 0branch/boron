; m2 2.0.2 Visual-C++ NMake target


embed-manifest: false
vc-mach: 'x64   ;'x86

win32: func [blk] [do blk]
windows?: msvc: true

console: does [ct/cfg_console: true]

conv_slash: func [file] [replace/all copy file '/' '\']

generate_makefile: does [
    emit-makefile "NMAKE Makefile for Visual-C++"
    rejoin [
{AS       = } select [x64: "ml64" x86: "ml"] vc-mach {.exe
CC       = cl.exe
CXX      = cl.exe
LINK     = link.exe
MOC      = $(QTDIR)\bin\moc.exe
RCC      = $(QTDIR)\bin\rcc.exe
MT       = mt.exe
TAR      = tar.exe -cf
ZIP      = zip.exe -r -9
}
    ]
]


qt-includes: context [
    concurrent:
             does [ include_from {$(QTDIR)\include\QtConcurrent} ]
    core:    does [ include_from {$(QTDIR)\include\QtCore} ]
    gui:     does [ include_from {$(QTDIR)\include\QtGui} ]
    network: does [ include_from {$(QTDIR)\include\QtNetwork} ]
    opengl:  does [ include_from {$(QTDIR)\include\QtOpenGL} ]
    printsupport:
             does [ include_from {$(QTDIR)\include\QtPrintSupport} ]
    svg:     does [ include_from {$(QTDIR)\include\QtSvg} ]
    sql:     does [ include_from {$(QTDIR)\include\QtSql} ]
    support: does [ include_from {$(QTDIR)\include\Qt3Support}
                    cxxflags {-DQT3_SUPPORT} ]
    widgets: does [
        if gt? qt-version 4 [include_from {$(QTDIR)\include\QtWidgets}]
    ]
    xml:     does [ include_from {$(QTDIR)\include\QtXml} ]
]

qt-libraries: func [debug | n] [
    n: qt-version
    if debug [n: negate n]
    context select [
    5 [
        concurrent: " Qt5Concurrent"
        core:    " Qt5Core"
        gui:     " Qt5Gui"
        network: " Qt5Network"
        opengl:  " Qt5OpenGL"
        printsupport: " Qt5PrintSupport"
        svg:     " Qt5Svg"
        sql:     " Qt5Sql"
        main:    " qtmain"
        widgets: " Qt5Widgets"
        xml:     " Qt5Xml"
    ]
    -5 [
        concurrent: " Qt5Concurrentd"
        core:    " Qt5Cored"
        gui:     " Qt5Guid"
        network: " Qt5Networkd"
        opengl:  " Qt5OpenGLd"
        printsupport: " Qt5PrintSupportd"
        svg:     " Qt5Svgd"
        sql:     " Qt5Sqld"
        main:    " qtmaind"
        widgets: " Qt5Widgetsd"
        xml:     " Qt5Xmld"
    ]
    4 [
        core:    " QtCore4"
        gui:     " QtGui4"
        network: " QtNetwork4"
        opengl:  " QtOpenGL4"
        svg:     " QtSvg4"
        sql:     " QtSql4"
        support: " Qt3Support4"
        main:    " qtmain"
        xml:     " QtXml4"
    ]
    -4 [
        core:    " QtCored4"
        gui:     " QtGuid4"
        network: " QtNetworkd4"
        opengl:  " QtOpenGLd4"
        svg:     " QtSvgd4"
        sql:     " QtSqld4"
        support: " Qt3Supportd4"
        main:    " qtmaind"
        xml:     " QtXmld4"
    ]
    ] n
]


exe_target: make target_env
[
    obj_macro: none
    cfg_console: false

    append defines "_WIN32"

    config:
    [
        obj_macro: rejoin ["$(" uc_name "_OBJECTS)"]

        ; Most of libc gives warnings in VC8.
        cflags {-D_CRT_SECURE_NO_WARNINGS}

        cflags {/nologo}
        cxxflags {/EHsc}
        lflags {/nologo}    ; { /incremental:no}

        either cfg_console [
            lflags {/subsystem:console}
        ][
            lflags {/subsystem:windows}
        ]

        either cfg/warn [
            cflags {-W3}
        ][
            cflags {-W0}
        ]

        if cfg/debug [
            cflags {/MDd}
            cflags {-Zi -DDEBUG}
            lflags {/DEBUG}
        ]

        if cfg/release [
            cflags {/MD}
            cflags {-O2 -DNDEBUG}
        ]

        if cfg/opengl [
            libs {opengl32 glu32}
        ]

        if cfg/qt [
            include_from {$(QTDIR)\include}
            do bind copy cfg/qt in qt-includes 'gui
            if cfg/release [
                cxxflags {-DQT_NO_DEBUG}
            ]
        ]

        if cfg/x11 [
            include_from {/usr/X11R6/include}
            libs_from %/usr/X11R6/lib {Xext X11 lm}
        ]
    ]

    configure: does [
        output_file: rejoin [conv_slash output_dir name ".exe"]
        do config
        ;libs {user32}    ; gdi32
        if cfg/qt [
            append qt-libs: copy cfg/qt 'main
            libs_from %"$(QTDIR)/lib" 
                rejoin bind qt-libs qt-libraries cfg/debug

            libs {comdlg32 winspool advapi32 shell32 ole32}
        ]
    ]

    lib_string: func [items block! | str] [
        str: make string! 64
        forall items [
            append str either find items/1 ' ' [
                rejoin [{ "} items/1 {.lib"}]
            ][
                rejoin [{ } items/1 {.lib}]
            ]
        ]
        remove str
    ]

    macro_text: does [
        ifn empty? menv_aflags [
            emit [uc_name "_AFLAGS   = " menv_aflags eol]
        ]

        emit [
            uc_name "_CFLAGS   = " menv_cflags /*' ' gnu_string "/D" defines*/ eol
            uc_name "_CXXFLAGS = $(" uc_name "_CFLAGS) " menv_cxxflags eol
            uc_name "_INCPATH  = " gnu_string "/I" include_paths eol
            uc_name "_LFLAGS   = " menv_lflags eol
            uc_name "_LIBS     = " gnu_string "/libpath:" link_paths ' '
                    lib_string link_libs eol
        ]

        emit [
            uc_name "_SOURCES  = " expand_list_gnu source_files eol
            uc_name "_OBJECTS  = " expand_list_gnu object_files eol
        ]
    ]

    clean: does [
        rejoin [
            "^--@del " output_file ' ' obj_macro
            either empty? srcmoc_files [""] [join ' ' to-text srcmoc_files]
            eol
        ]
    ]

    dist: does [
        rejoin [" $(" uc_name "_SOURCES) $(" uc_name "_HEADERS)"]
    ]

    rule_text: [
        output_file ": " obj_macro local_libs link_libs
            sub-project-libs link_libs
        {^/^-$(LINK) /machine:} vc-mach { /out:$@ $(} uc_name {_LFLAGS) }
            obj_macro { $(} uc_name {_LIBS)} eol

        either embed-manifest [rejoin [
            {^-$(MT) -manifest } output_file
                {.manifest -outputresource:} output_file
                ';' pick [2 1] to-logic find output_file ".dll" eol
        ]] ""
    ]

    rule_makeobj: func [cc flags obj src] [
        rejoin ["^-$(" cc ") /c $(" uc_name flags ") /Fo" conv_slash obj
                " $(" uc_name "_INCPATH) " custom_src conv_slash src]
    ]

    makerule_asm: func [obj src] [
        rule_makeobj "AS" "_AFLAGS" obj custom_src src
    ]
]


lib_target: make exe_target [
    configure: does [
        output_file: rejoin [conv_slash output_dir name ".lib"]
        do config
    ]

    rule_text: [
        output_file ": " obj_macro sub-project-libs link_libs
        "^/^-lib /nologo /out:$@ " obj_macro " $(" uc_name
            "_LIBS) $(" uc_name "_LFLAGS)^/"
    ]
]


shlib_target: make exe_target [
    configure: does [
        output_file: rejoin [conv_slash output_dir name ".dll"]
        do config
        ;cflags {-MT}
        lflags {/DLL}
        if cfg/qt [
            libs_from %"$(QTDIR)/lib" 
                rejoin bind copy cfg/qt qt-libraries cfg/debug
        ]
    ]
]


;EOF
