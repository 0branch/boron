
default [
    warn
   ;debug
    release
    objdir %obj

    include_from [
        %.
        %..
        %../urlan
    ]
    ;macx [universal]
]

shlib [%boron-gl 0,2,6] [
   ;cflags {-DNO_AUDIO}

    linux [
        cflags {-std=gnu99}
        cflags {-DUSE_XF86VMODE}
        include_from %../unix
        include_from %/usr/include/freetype2
       ;sources [%joystick.c]

        ;libs [%X11 %Xxf86vm]
        ;libs [%freetype %png %glv %GL %GLU
        ;      %openal %vorbis %vorbisfile]
    ]
    macx [
        cflags {-std=c99}
        include_from %../unix
        include_from %glv/mac
        sources [%glv/mac/glv.c]

        include_from %/usr/local/include/freetype2
    ]
    win32 [
        lib-path: %"C:/cygwin/home/karl/osrc/"

        include_from %glv/win32
        sources [%glv/win32/glv.c]

        include_from %../win32
        sources_from %../win32 [%win32console.c]

        include_from join lib-path %freetype2/include
        libs_from join lib-path %freetype2/objs [%freetype]

        include_from join lib-path %lpng128
        include_from join lib-path %zlib
        libs_from join lib-path %lpng128 [%libpng]
        libs_from join lib-path %zlib [%zlib]

        include_from %"C:/Program Files/OpenAL 1.1 SDK/include"
        libs_from %"C:/Program Files/OpenAL 1.1 SDK/libs/Win32" %OpenAL32
            ;%ALut
    ]

    sources [
        %audio.c
        %boron-gl.c
        %draw_prog.c
        %geo.c
        %glid.c
        %gui.c
       ;%particles.c
        %png_load.c
        %png_save.c
        %quat.c
        %raster.c
        %rfont.c
        %shader.c
        %TexFont.c
        %port_joystick.c
        %widget_button.c
        %widget_label.c
        %widget_lineedit.c
        %widget_list.c
        %widget_slider.c
    ]
]

exe %boron-gl [
    opengl
    include_from %../support
    cflags {-DBORON_GL}
    libs_from %. %boron-gl
    libs_from %.. %boron
    linux [
        ;libs_from %/usr/X11R6/lib [%X11 %Xxf86vm]
        libs [%X11 %Xxf86vm]
        libs [%freetype %png %glv %m]
        libs [%openal %vorbis %vorbisfile %pthread]
    ]
    macx [
        libs [%freetype %png]
        libs [%vorbis %vorbisfile]

       ;lflags {-framework ALUT}
        lflags {-framework OpenAL}
        lflags {-framework OpenGL}
        lflags {-framework AGL}
        lflags {-framework Carbon}
    ]
    sources [
        %../boron_console.c
    ]
]
