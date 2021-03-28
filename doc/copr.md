---
title:  Copr
date:   Version 0.2.2, 2021-03-28
---


Overview
========

Copr is the **Co**mpile **Pr**ogram tool (pronounced as copper).
It's a [Boron] script that provides a simple alternative to make (or cmake,
xmake, etc.) for C/C++ projects that need to be built on various operating
systems.

Copr reads a project file which declares what programs and libraries to build.
From this a list of dependencies and commands to run is generated and cached.
For an executable built from a single file it can look this this:

    exe %hello [
        sources [%main.c]
    ]

Unlike more complicated build systems it does not search the build environment
trying to find compilers and/or libraries.  These are kept under strict
control of the user and configured via the project and target definition files.


Installation
============

The script can be downloaded directly to your `~/bin` with these commands:

    curl -sL -o ~/bin/copr "https://sf.net/p/urlan/boron/code/ci/master/tree/scripts/copr.b?format=raw"
    chmod +x ~/bin/copr


Options
=======

Running Copr with the `-h` option will show the usage and other available
options.

    copr version 0.2.0

    Copr Options:
      -a              Archive source files.
      -c              Clean up (remove) previously built files & project cache.
      -d              Build in debug mode.         (default is release)
      -e <env_file>   Override build environment.
      -h              Print this help and quit
      -j <count>      Use specified number of job threads.  (1-6, default is 1)
      -r              Do a dry run and only print commands.
      -t <os>         Set target operating system. (default is auto-detected)
      -v <level>      Set verbosity level.         (0-4, default is 2)
      --clear         Remove caches of all projects.
      <project>       Specify project file         (default is project.b)
      <opt>:<value>   Set project option

    Project Options:
      debug_mode:     Build in debug mode.

When run without options it will build the files specified by the `project.b`
file in the current directory.

## Archive
Create a compressed archive of the source files using `tar`.
This be used with the [dry run] option `-r`.

## Help
Help simply prints the usage.

## Clean
Remove previously built files and the project cache.

## Debug
The `-d` option changes the compiler and linker flags to enable debugging.
The default flags are set for an optimized release build.

The default settings can also be changed by using `debug_mode: true` in a
project.config file.

## Build Environment
A file can be specified to override the [target system] settings.

## Jobs
Use `-j` to run commands using multiple threads.

## Dry Run
Use the `-r` option to print the commands that would otherwise be run.
The output is controlled by the [Verbosity] level.

## Target System
This is used to generate commands for a specific OS and compiler.

Target  System
------  --------------------
linux   Linux with cc/c++
mingw   Linux with MinGW cross-compiler
macx    macOS with cc/c++
win32   Windows with VisualC

## Verbosity
Use the `-v` option to control what is printed to stdout.
The default level is 2.

Level  Output
-----  --------------------
  0    No output
  1    Print job summary
  2    Print full commands (Default)
  3    Enable debug output
  4    Full debug output


Project File
============

The project file is a Boron script which invokes functions provided by Copr.
The primary functions declare what sort of targets to build.

The following example builds a library named "moduleX" from two source files
and a program named "my_program" which uses it.

    default [
        include_from %moduleX
    ]

    lib %moduleX [
        sources_from %moduleX [
            %source1.c
            %source2.c
        ]
    ]

    exe %my_program [
        libs_from %. %moduleX
        sources [
            %main.c
        ]
    ]

## options
Project specific build options can be defined in the options block using
triplets of name, default-value, & description (`set-word!` value `string!`).
For example:

    options [
        extra-features: false    "Build with extra features"
        zone-limit:       500    "Maximum number of zones"
    ]

Only a single options command can be used in any project file.

There is one predefined project option called `debug_mode` which if set true
will configure the build in the same was as the [debug] command line option.

The defined options can be set on the Copr command line using the colon
argument syntax.

If a file named `project.config` exists in the same directory as the project
file, it will be evaluated and can be used to set the build options.
Command line options will override any set in the project.config.

## default
The default function defines a common set of instructions for all targets.

## exe
To build binary executables use the *exe* target.

## gen
Gen is used to generate a generic target.  It is passed an output file,
a dependency file, and one or more commands.

## lib
Builds a static object module library.

## shlib
Builds a shared (dynamic) object module library.


Target Functions
================

The following functions can be used inside any [default], [exe], [lib],
or [shlib] block.

## aflags
Specify assembler options.

## cflags
Specify C compiler options.

## cxxflags
Specify C++ compiler options.

## include_from
Specify header include directories.

## libs
Specify libraries to be linked.

## libs_from
Specify libraries from a specific path to be linked.

## objdir
The default output directory for object files is `.copr/obj`.  If another
place is desired then use objdir.

## sources
This is the list of files to compile and link together.

If specific compiler options are required for a few files then the `/cflags`
option! can be used.

## sources_from

## opengl
This will link the target with the OpenGL library.

## qt
This will link the target with the specified Qt 5 libraries.

    gui widgets network concurrent opengl printsupport svg sql xml

A basic QtWidget application need only include this command:

    qt [widgets]


Project Cache
=============


Support
=======

If you have any questions or issues you can email
wickedsmoke [at] users.sf.net.


[Boron]: http://urlan.sourceforge.net/boron/
