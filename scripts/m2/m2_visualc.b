; m2 Visual-C++ template


embed-manifest: true

win32: func [blk] [do blk]

console: does [ct/cfg_console: true]

conv_slash: func [file] [replace/all file '/' '\']

generate_makefile: does [
    ;foreach t targets [ probe t ] ;KR

    emit do_tags copy nmake_header

    emit "^/#------ Target settings"
    foreach t targets [ emit "^/^/" t/macro_text ]

    emit [ {^/ARCHIVE = } project_version {^/DIST_FILES = \^/} ]
    ifn empty? distribution_files [
        emit expand_list_gnu/part distribution_files
    ]
    emit expand_list_gnu header_files_used

    emit "^/^/#------ Build rules^/^/all:"
    foreach t targets [emit [' ' t/output_file]]
    emit eol

    foreach t targets [emit ' ' t/rule_text]

    emit ["^/^/" do_tags copy nmake_other_rules]

    foreach t targets [emit t/clean]

    emit "^/^/#------ Compile rules^/^/"
    foreach t targets [
        emit t/make_obj_rule
        if t/cfg/qt [emit t/moc_rule]
    ]

    emit "^/#EOF^/"
]


qt-includes: context [
   gui:     does [ include_from {$(QTDIR)\include\QtGui} ]
   network: does [ include_from {$(QTDIR)\include\QtNetwork} ]
   opengl:  does [ include_from {$(QTDIR)\include\QtOpenGL} ]
   xml:     does [ include_from {$(QTDIR)\include\QtXml} ]
   svg:     does [ include_from {$(QTDIR)\include\QtSvg} ]
   sql:     does [ include_from {$(QTDIR)\include\QtSql} ]
   support: does [ include_from {$(QTDIR)\include\Qt3Support}
                   cxxflags {-DQT3_SUPPORT} ]
]

qt-libraries: context [
   core:    { QtCore4}
   gui:     { QtGui4}
   network: { QtNetwork4}
   opengl:  { QtOpenGL4}
   xml:     { QtXml4}
   svg:     { QtSvg4}
   sql:     { QtSql4}
   support: { Qt3Support4}
   main:    { qtmain}
]

qt-debug-libraries: context [
   core:    { QtCored4}
   gui:     { QtGuid4}
   network: { QtNetworkd4}
   opengl:  { QtOpenGLd4}
   xml:     { QtXmld4}
   xml:     { QtXmld4}
   svg:     { QtSvgd4}
   sql:     { QtSqld4}
   support: { Qt3Supportd4}
   main:    { qtmaind}
]


exe_target: make target_env
[
    obj_macro: none
    cfg_console: false

    config:
    [
        obj_macro: rejoin ["$(" uc_name "_OBJECTS)"]

        ; Most of libc gives warnings in VC8.
        cflags {-D_CRT_SECURE_NO_WARNINGS}

        cflags {/nologo}
        cxxflags {/EHsc}
        lflags {/nologo /incremental:no}

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
            cflags {-O1 -DNDEBUG}
        ]

        if cfg/opengl [
            libs {opengl32 glu32}
        ]

        if cfg/qt [
            include_from {$(QTDIR)\include}
            include_from {$(QTDIR)\include\QtCore}
            do bind cfg/qt in qt-includes 'gui
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
            append qt4-libs: copy cfg/qt [core main]
            libs_from %"$(QTDIR)/lib" 
                rejoin bind qt4-libs
                    either cfg/debug [qt-debug-libraries] [qt-libraries]

            libs {comdlg32 winspool advapi32 shell32 ole32}
        ]
    ]

    lib_string: func [items block! | str]
    [
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

    macro_text: func [] [
        ifn empty? menv_aflags [
            emit [uc_name "_AFLAGS   = " menv_aflags eol]
        ]

        emit [
            uc_name "_CFLAGS   = " menv_cflags ' ' gnu_string "/D" defines eol
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

    clean: does [ rejoin ["^--@del " output_file ' ' obj_macro eol] ]

    dist: does [
        rejoin [" $(" uc_name "_SOURCES) $(" uc_name "_HEADERS)"]
    ]

    rule_text: does [
        emit [
            eol output_file ": " obj_macro local_libs link_libs
            {^/^-$(LINK) /out:$@ $(} uc_name {_LFLAGS) } obj_macro
            { $(} uc_name {_LIBS)} eol
        ]
        if embed-manifest [
            emit [
                {^-$(MT) -manifest } output_file
                {.manifest -outputresource:} output_file
                ';' pick [2 1] to-logic find output_file ".dll" eol
            ]
        ]
    ]

    rule_makeobj: func [cc flags obj src] [
        rejoin ["^-$(" cc ") /c $(" uc_name flags ") /Fo" obj 
                " $(" uc_name "_INCPATH) " custom_src src]
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

    rule_text: does [
        emit [
            eol output_file ": " obj_macro
            "^/^-lib /nologo /out:$@ " obj_macro " $(" uc_name
            "_LIBS) $(" uc_name "_LFLAGS)^/"
        ]
    ]
]


shlib_target: make exe_target [
    configure: does [
        output_file: rejoin [conv_slash output_dir name ".dll"]
        do config
        ;cflags {-MT}
        lflags {/DLL}
        if cfg/qt [
            append qt4-libs: copy cfg/qt [core]
            libs_from %"$(QTDIR)/lib" 
                rejoin bind qt4-libs
                    either cfg/debug [qt-debug-libraries] [qt-libraries]
        ]
    ]
]


nmake_header:
{#----------------------------------------------------------------------------
# Makefile for NMAKE
# Generated by m2 at <now/date>
# Project: <m2/project>
#----------------------------------------------------------------------------


#------ Compiler and tools

AS       = ml.exe
CC       = cl.exe
CXX      = cl.exe
LINK     = link.exe
TAR      = tar.exe -cf
ZIP      = zip.exe -r -9
MOC      = $(QTDIR)\bin\moc.exe
MT       = mt.exe

}


nmake_other_rules:
{<m2/makefile>: <project_file>
^-m2 <project_file>

.PHONY: dist
dist:
^-$(TAR) $(ARCHIVE).tar --exclude CVS --exclude .svn --exclude *.o <project_file> <m2/dist_files> $(DIST_FILES)
^-mkdir /tmp/$(ARCHIVE)
^-tar -C /tmp/$(ARCHIVE) -xf $(ARCHIVE).tar
^-tar -C /tmp -cf $(ARCHIVE).tar $(ARCHIVE)
^-rm -rf /tmp/$(ARCHIVE)
^-$(ZIP) $(ARCHIVE).tar

.PHONY: clean
clean:
}


;EOF
