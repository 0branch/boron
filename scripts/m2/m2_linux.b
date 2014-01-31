; m2 Linux-g++ template


system-qt: true
qt-version: 4

linux:
unix:  func [blk] [do blk]

generate_makefile: does [
    foreach t targets [
        ;probe t

        ; make_obj_rule must be called before we write DIST_FILES since
        ; it finds the header dependencies.
        t/built_obj_rule: t/make_obj_rule
    ]

    emit do_tags copy gnu_header
    emit pick [
        {MOC      = moc-qt4^/QTINC    = /usr/include^/^/}
        {MOC      = $(QTDIR)/bin/moc^/QTINC    = $(QTDIR)/include^/^/}
    ] system-qt

    emit "^/#------ Target settings"
    foreach t targets [emit "^/^/" t/macro_text]

    emit [{^/ARCHIVE = } project_version {^/DIST_FILES = \^/}]
    ifn empty? distribution_files [
        emit expand_list_gnu/part distribution_files
    ]
    emit expand_list_gnu header_files_used

    emit "^/^/#------ Build rules^/^/all:"
    foreach t targets [ emit [' ' t/output_file] ]
    emit eol

    emit-sub-projects
    foreach t targets [ emit ' ' t/rule_text ]

    emit [ "^/^/" do_tags copy gnu_other_rules ]

    foreach t targets [ emit t/clean ]

    emit "^/^/#------ Compile rules^/^/"
    foreach t targets [
        emit t/built_obj_rule
        if t/cfg/qt [emit t/moc_rule]
    ]

    emit "^/#EOF^/"
]


qt-includes: context [
    concurrent: does [ include_from {$(QTINC)/QtConcurrent} ]
    gui: does [
         include_from {$(QTINC)/QtGui}
         if eq? 5 qt-version [include_from {$(QTINC)/QtWidgets}]
    ]
    network:    does [ include_from {$(QTINC)/QtNetwork} ]
    opengl:     does [ include_from {$(QTINC)/QtOpenGL} ]
    xml:        does [ include_from {$(QTINC)/QtXml} ]
    svg:        does [ include_from {$(QTINC)/QtSvg} ]
    sql:        does [ include_from {$(QTINC)/QtSql} ]
    support:    does [ include_from {$(QTINC)/Qt3Support}
                       cxxflags {-DQT3_SUPPORT} ]
]

either eq? 5 qt-version [
    qt-libraries: context [
        concurrent: " Qt5Concurrent"
        core:    " Qt5Core"
        gui:     " Qt5Widgets Qt5Gui"
        network: " Qt5Network"
        opengl:  " Qt5OpenGL"
        svg:     " Qt5Svg"
        sql:     " Qt5Sql"
        widgets: " Qt5Widgets"
        xml:     " Qt5Xml"
    ]
    qt-debug-libraries: context [
        concurrent: "Qt5Concurrent_debug"
        core:    "Qt5Core_debug"
        gui:     "Qt5Widgets_debug Qt5Gui_debug"
        network: "Qt5Network_debug"
        opengl:  "Qt5OpenGL_debug"
        svg:     "Qt5Svg_debug"
        sql:     "Qt5Sql_debug"
        widgets: "Qt5Widgets_debug"
        xml:     "Qt5Xml_debug"
    ]
][
    qt-libraries: context [
        core:    "QtCore"
        gui:     "QtGui"
        network: "QtNetwork"
        opengl:  "QtOpenGL"
        xml:     "QtXml"
        svg:     "QtSvg"
        sql:     "QtSql"
        support: "Qt3Support"
    ]
    qt-debug-libraries: context [
        core:    "QtCore_debug"
        gui:     "QtGui_debug"
        network: "QtNetwork_debug"
        opengl:  "QtOpenGL_debug"
        xml:     "QtXml_debug"
        svg:     "QtSvg_debug"
        sql:     "QtSql_debug"
        support: "Qt3Support_debug"
    ]
]


exe_target: make target_env
[
    ; built_obj_rule exists only to hold the output make_obj_rule
    built_obj_rule: none

    obj_macro: none

    append defines "__linux__"

    config:
    [
        obj_macro: rejoin [ "$(" uc_name "_OBJECTS)" ]

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
            libs {GL GLU}
            ;libs_from %/usr/X11R6/lib {glut Xi}
            ;libs {glut Xi Xmu}
        ]

        if cfg/qt [
            include_from {$(QTINC)}
            include_from {$(QTINC)/QtCore}
            do bind cfg/qt qt-includes
            if eq? 5 qt-version [cxxflags {-fPIC}]
            if cfg/release [
                cxxflags {-DQT_NO_DEBUG}
            ]
        ]

        if cfg/x11 [
            include_from {/usr/X11R6/include}
            libs_from "/usr/X11R6/lib" {Xext X11 m}
        ]
    ]

    configure: func [| qt-libs] [
        output_file: rejoin [output_dir name]
        do config
        if cfg/qt [
            qt-libs: copy cfg/qt
            ifn find qt-libs 'core [append qt-libs 'core]

            qt-libs: rejoin bind qt-libs
                either cfg/debug [qt-debug-libraries] [qt-libraries]

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

    rule_text: does
    [
        emit [ eol output_file ": " obj_macro local_libs link_libs
               sub-project-libs link_libs
            {^/^-$(}
                either link_cxx ["LINK_CXX"]["LINK"]
                {) -o $@ $(} uc_name {_LFLAGS) } obj_macro
            { $(} uc_name {_LIBS)} eol
        ]
    ]
]


lib_target: make exe_target [
    configure: does [
        output_file: rejoin [output_dir "lib" name ".a"]
        do config
    ]

    rule_text: does
    [
        emit [eol output_file ": " obj_macro sub-project-libs link_libs]
        emit either empty? link_libs [[
            "^/^-ar rc $@ " obj_macro " $(" uc_name "_LFLAGS)"
        ]] [[
            ; Concatenate other libraries.
            "^/^-ld -Ur -o " objdir name "lib.o $^^ $(" uc_name
                "_LIBS) $(" uc_name "_LFLAGS)"
            "^/^-ar rc $@ " objdir name "lib.o"
        ]]
        emit "^/^-ranlib $@^/"
        if cfg/release [
            emit "^-strip -d $@^/"
        ]
    ]
]


shlib_target: make exe_target [
    version: lib_full: lib_m: lib_base: none
    _exe_rule: _exe_clean: none

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

            _exe_rule: :rule_text
            rule_text: does [
                _exe_rule
                emit rejoin ["^-ln -sf " lib_full ' ' lib_m    '^/'
                             "^-ln -sf " lib_full ' ' lib_base '^/']
            ]
            _exe_clean: :clean
            clean: does [
                rejoin [slice _exe_clean -1 ' ' lib_m ' ' lib_base eol]
            ]
        ]
    ]
]


gnu_header:
{#----------------------------------------------------------------------------
# Makefile for GNU make
# Generated by m2 at <now/date>
# Project: <m2/project>
#----------------------------------------------------------------------------


#------ Compiler and tools

AS       = as
CC       = gcc -Wdeclaration-after-statement
CXX      = g++
LINK     = gcc
LINK_CXX = g++
TAR      = tar -cf
GZIP     = gzip -9f

}
;#------ Project-wide settings


gnu_other_rules:
{<m2/makefile>: <project_file>
^-m2 <project_file>

.PHONY: dist
dist:
^-$(TAR) $(ARCHIVE).tar --exclude CVS --exclude .svn --exclude *.o <project_file> <m2/dist_files> $(DIST_FILES)
^-mkdir /tmp/$(ARCHIVE)
^-tar -C /tmp/$(ARCHIVE) -xf $(ARCHIVE).tar
^-tar -C /tmp -cf $(ARCHIVE).tar $(ARCHIVE)
^-rm -rf /tmp/$(ARCHIVE)
^-$(GZIP) $(ARCHIVE).tar

.PHONY: clean
clean:
^--rm -f core
}


;EOF
