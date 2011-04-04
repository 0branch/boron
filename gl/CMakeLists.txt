cmake_minimum_required(VERSION 2.6)      # Shut up warnings.

project(BoronGL C)

#set(CMAKE_BUILD_TYPE Debug)
 set(CMAKE_BUILD_TYPE Release)


# Why isn't this the default?
# Having to repeat the conditional expression is retarded.
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../cmake_modules/")


find_package( Boron REQUIRED )
find_package( Freetype REQUIRED )
find_package( OpenAL REQUIRED )
find_package( OpenGL REQUIRED )
find_package( GLV REQUIRED )
find_package( PNG REQUIRED )


if (APPLE)
    include_directories(../unix)
    add_definitions(-std=c99)
#    set(GLV_FILE glv/mac/glv.c)
endif ()

if (UNIX AND NOT APPLE)
    include_directories(../unix)
    add_definitions(-std=gnu99 -DUSE_XF86VMODE)
#    find_package( X11 REQUIRED )
#    set(GLV_FILE glv/x11/glv.c)
    set(JOYSTICK_FILE port_joystick.c)
endif ()

if (WIN32)
    include_directories(../win32)
#    set(GLV_FILE glv/win32/glv.c)
endif ()


add_definitions(
    -DBORON_GL
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif ()

include_directories(
	.
    ..
    ../support
    ../urlan
#   ${BORON_INCLUDE_DIR}
    ${FREETYPE_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
)


add_library(boronGL-lib SHARED
    audio.c
    boron-gl.c
    draw_prog.c
    geo.c
    glid.c
    gui.c
    png_load.c
    png_save.c
    quat.c
    raster.c
    rfont.c
    shader.c
    TexFont.c
    widget_script.c
    widget_button.c
    widget_label.c
    widget_lineedit.c
    widget_list.c
#   ${GLV_FILE}
    ${JOYSTICK_FILE}
)

set_target_properties(boronGL-lib PROPERTIES
	VERSION 0.1.11
	SOVERSION 0
	OUTPUT_NAME "boron-gl")

if (APPLE)
    target_link_libraries(boronGL-lib m)
endif ()

if (UNIX AND NOT APPLE)
    target_link_libraries(boronGL-lib vorbis vorbisfile m pthread)
endif ()

if (WIN32)
    target_link_libraries(boronGL-lib ws2_32)
endif ()

link_directories(../)
target_link_libraries(boronGL-lib
    ${BORON_LIBRARY}
    ${PNG_LIBRARY}
    ${FREETYPE_LIBRARY}
    ${GLV_LIBRARY}
    ${OPENGL_gl_LIBRARY}
    ${OPENGL_glu_LIBRARY}
    ${OPENAL_LIBRARY}
)


add_executable(boron-gl ../boron_console.c)
target_link_libraries(boron-gl boronGL-lib)


if (UNIX)
install(TARGETS boron-gl     DESTINATION bin)
install(TARGETS boronGL-lib  DESTINATION lib)
install(FILES   boron-gl.h gui.h TexFont.h DESTINATION include/boron)
endif ()


# eof
