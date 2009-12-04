cmake_minimum_required(VERSION 2.6)      # Shut up warnings.

project(Boron C)

#set(CMAKE_BUILD_TYPE Debug)
 set(CMAKE_BUILD_TYPE Release)


# Why isn't this the default?
# Having to repeat the conditional expression is retarded.
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS true)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")

set(CMAKE_SKIP_RPATH true)

# Config defines
#add_definitions(
#    -DUR_CONFIG_BZIP2
#    -DUR_CONFIG_MATH3D
#    -DUR_CONFIG_NET
#)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif ()


if (UNIX)
    include_directories(. urlan support unix)
    add_definitions(-std=c99 -pedantic -D_GNU_SOURCE)
    set(OS_FILE unix/os.c)
endif ()

if (WIN32)
    include_directories(. urlan support win32)
    set(OS_FILE win32/os.c)
endif ()


add_library(boron-lib SHARED
    urlan/array.c
    urlan/bignum.c
    urlan/binary.c
    urlan/block.c
    urlan/coord.c
    urlan/context.c
    urlan/date.c
    urlan/env.c
    urlan/gc.c
    urlan/parse_block.c
    urlan/parse_string.c
    urlan/path.c
    urlan/string.c
    urlan/tokenize.c
    support/mem_util.c
    support/str.c
    support/quickSortIndex.c
#   support/well512.c
    boron.c
    ${OS_FILE}
)

set_target_properties(boron-lib PROPERTIES
   VERSION 0.1.0
   SOVERSION 0
   OUTPUT_NAME "boron")

if (APPLE)
    target_link_libraries(boron-lib m bz2)
endif ()
if (UNIX AND NOT APPLE)
    target_link_libraries(boron-lib m bz2 pthread)
endif ()
if (WIN32)
    target_link_libraries(boron-lib ws2_32)
endif ()


add_executable(boron-bin
    boron_console.c
)
set_target_properties(boron-bin PROPERTIES OUTPUT_NAME "boron")

# NOTE: This links the binary with the libraries boron-lib links against.
# This creates unneeded linker dependencies, but the CMake FAQ says that
# there is no way to change this behavior.
# Here is a blog entry which talks about this problem:
#   http://blogs.sun.com/rie/entry/tt_dependencies_tt_define_what

target_link_libraries(boron-bin boron-lib)


if (UNIX)
install(TARGETS boron-bin DESTINATION /usr/local/bin)
install(TARGETS boron-lib DESTINATION /usr/local/lib)
install(FILES boron.h urlan/urlan.h urlan/urlan_atoms.h urlan/bignum.h DESTINATION /usr/local/include/boron)
endif ()

# eof
