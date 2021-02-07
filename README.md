About Boron
===========

Boron is an interpreted, prototype-based, scripting language similar to Rebol.
The interpreter and datatype system is a C library useful for building
domain-specific languages (DSLs) embedded in C/C++ applications.

It is a smaller language than Rebol with fewer built-in data types, no infix
operators, and no built-in internet protocols.  It does add the capability to
slice series values and store data in a serialized binary format.

The library may be copied under the terms of the LGPLv3, which is included in
the LICENSE file.


How to compile
==============

These commands can be used to build the shared library and interpreter program
on UNIX systems:

    ./configure
    make

To see the configure options run:

    ./configure -h

Use the make `install` & `install-dev` targets to copy files into the local
system directories.  If the DESTDIR is not provided then the files will be
placed under `/usr/local`.

    sudo make install DESTDIR=/usr
    sudo make install-dev DESTDIR=/usr


Boron links
===========

Home Page
http://urlan.sourceforge.net/boron/

Git Repository
https://git.code.sf.net/p/urlan/boron/code

Author Email
wickedsmoke@users.sf.net
