/*
   Extract cfunc! documentation from C source files.
*/

ifn args [
    print "Please specify input C files."
    quit
]


cfuncs: make block! 100
hfuncs: make block! 40

forall args [
    f: read first args
    parse f [some[
        thru "-cf-" in: thru "*/" :in (append cfuncs trim/indent slice in -2)
    ]]
    parse f [some[
        thru "-hf-" in: thru "*/" :in (append hfuncs trim/indent slice in -2)
    ]]
]


doc:  make string! 1024
emit: func [txt] [append doc txt]
nl: '^/'
h1:   "=============================="
h2:   "------------------------------"

white: charset " ^-^/"
emit-funcs: func [funcs] [
    foreach f sort funcs [
    ;probe f
        parse f [
            name: to white :name thru white f:
        ]
        emit nl
        emit name
        emit nl
        emit slice h2 size? name
        emit nl
        emit f
    ]
]


emit
{================================
   Boron Function Reference
================================

:Version:   0.1
:Copyright: |copy| 2009 Karl Robillard

.. contents::
.. sectnum::

C Functions
===========

These are the built-in functions implemented in C.


Test-Func
---------
::

    test-func arg1 arg2

========  ================  ========================================
Argument  Valid Types       Description
========  ================  ========================================
arg1      string!           Blah blah 1
arg2       hx                 Blah blah 2
========  ================  ========================================

Return: some stuff.

About Test Func.

::

    example code
}

emit-funcs cfuncs

emit
{
Helper Functions
================

These are the built-in func! functions.

}

emit-funcs hfuncs

emit {
.. |copy| unicode:: 0xA9
}


print doc
;write "cfunc.rst" doc
