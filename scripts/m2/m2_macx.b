; m2 Mac OS X template


macx: func [blk] [do blk]

bundle:    does [ct/cfg_bundle: true]
universal: does [ct/cfg_universal: true]

generate_makefile: does [
    foreach t targets [
        ;probe t

        ; make_obj_rule must be called before we write DIST_FILES since
        ; it finds the header dependencies.
        t/built_obj_rule: t/make_obj_rule

        moc_exe: {moc}  ; /usr/bin/moc-4.0
    ]

    emit do_tags copy gnu_header

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
        if t/cfg/qt [
            emit t/moc_rule
        ]
    ]

    emit "^/#EOF^/"
]


qt-libs: context [
   gui:     does [include_from {/Library/Frameworks/QtGui.framework/Headers}
                  lflags   {-framework QtGui}]
   network: does [include_from {/Library/Frameworks/QtNetwork.framework/Headers}
                  lflags   {-framework QtNetwork}]
   opengl:  does [include_from {/Library/Frameworks/QtOpenGL.framework/Headers}
                  lflags   {-framework QtOpenGL}]
   xml:     does [include_from {/Library/Frameworks/QtXml.framework/Headers}
                  lflags   {-framework QtXml}]
   svg:     does [include_from {/Library/Frameworks/QtSvg.framework/Headers}
                  lflags   {-framework QtSvg}]
   sql:     does [include_from {/Library/Frameworks/QtSql.framework/Headers}
                  lflags   {-framework QtSql}]
   support: does [
       include_from {/Library/Frameworks/Qt3Support.framework/Headers}
       cxxflags {-DQT3_SUPPORT}
       lflags   {-framework Qt3Support }
   ]
]

qt-static-libs: context [
   qlib: func [name] [
       include_from join {$(QTDIR)/include/} name
       lflags join {-l} name
   ]
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

        ; cflags {-isysroot /Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5}
        ; lflags {-Wl,-syslibroot,/Developer/SDKs/MacOSX10.5.sdk -mmacosx-version-min=10.5}
        if cfg_universal [
            cflags {-isysroot /Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386 -mmacosx-version-min=10.4}
            lflags {-Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk -arch ppc -arch i386 -mmacosx-version-min=10.4}
        ]

        if cfg/opengl [
            lflags {-framework OpenGL}
            ;libs {GL GLU}
        ]

        if cfg/qt [
            either cfg/qt-static [
                include_from {$(QTDIR)/include}
                include_from {$(QTDIR)/include/QtCore}
                lflags {-L$(QTDIR)/lib -lQtCore}
                lflags {-framework Carbon -lz -liconv}
                do bind cfg/qt qt-static-libs
            ][
                include_from {/Library/Frameworks/QtCore.framework/Headers}
                cflags {-F/Library/Frameworks}
                lflags {-F/Library/Frameworks -framework QtCore}
                lflags {-framework Carbon}
                do bind cfg/qt qt-libs
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

    macro_text: func []
    [
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
        rejoin [" $(" uc_name "_SOURCES)" ]
    ]

    rule_text: func [| bdir]
    [
        emit [ eol output_file ": " obj_macro local_libs link_libs
               sub-project-libs link_libs
            {^/^-$(}
                either link_cxx ["LINK_CXX"]["LINK"]
                {) -o $@ $(} uc_name {_LFLAGS) } obj_macro
            { $(} uc_name {_LIBS)} eol
        ]

        if cfg_bundle [
            bdir: to-file join name %.app/Contents/
            ifn exists? bdir [
                make-dir/all join bdir %MacOs
                write join bdir %PkgInfo #{4150504c 3f3f3f3f 0a}
                write join bdir %Info.plist rejoin bind info.plist 'name

                ;emit {^/# bundle}
            ]
        ]
    ]
]


lib_target: make exe_target [
    configure: does [
        output_file: rejoin [ output_dir "lib" name ".a" ]
        do config
    ]

    rule_text: does
    [
        emit [ eol output_file ": " obj_macro sub-project-libs link_libs
            "^/^-libtool -static -o $@ $^^ $(" uc_name "_LIBS)"
            "^/^-ranlib $@^/"
        ]
    ]
]


shlib_target: make exe_target [
    configure: does [
        output_file: rejoin [output_dir "lib" name ".dylib"]
        do config
        ;cflags {-fPIC}	; Default on Mac.
    ]

    rule_text: does [
        emit [ eol output_file ": " obj_macro sub-project-libs link_libs
            {^/^-$(} either link_cxx ["LINK_CXX"]["LINK"]
                {) -dynamiclib -o $@ } obj_macro
            { $(} uc_name {_LFLAGS) }
            { $(} uc_name {_LIBS)} eol
        ]
    ]
]


moc_exe: {$(QTDIR)/bin/moc}

gnu_header:
{#----------------------------------------------------------------------------
# Makefile for GNU make
# Generated by m2 at <now/date>
# Project: <m2/project>
#----------------------------------------------------------------------------


#------ Compiler and tools

AS       = as
CC       = gcc
CXX      = g++
LINK     = gcc
LINK_CXX = g++
TAR      = tar -cf
GZIP     = gzip -9f
MOC      = <moc_exe>

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
