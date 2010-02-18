==============================
     Boron User Manual
==============================

:Version:   0.1.2

.. sectnum::
.. contents::


Overview
========

Boron is an interpreted, prototype-based, scripting language.

Language features include:

   * Garbage collected datatype system with prototype based objects.
   * Written in C to work well as an embedded scripting language.
   * Small (but not tiny) binary & run-time environment.


About This Document
-------------------

This manual is largely incomplete.

There is a separate `function reference`_ and `code documentation`_
available online at http://urlan.sourceforge.net/boron.



Scripts
=======

Scripts are UTF-8 encoded text files.


Comments
--------

Single line comments begin with a semi-colon.

Block comments are the same as C block comments.  They begin with '``/*``' and
continue through '``*/``'.  Unlike C, block comments can be nested.

Comment examples:
::

   ; line comment

   add 2 4	; result is 6

   /*
     Block comment
   */


Shell Invocation
----------------

The first line of a script may be a UNIX shell sha-bang (``#!``) command.

::

    #!/usr/bin/boron


Command Line Arguments
----------------------

If the interpreter is invoked with a script then the *args* word will be set
to either a block of strings, or *none* if no script arguments were given.

So this Boron command::

    boron -e "probe args" file1 -p 2

Will print this::

    ["file1" "-p" "2"]



Datatypes
=========

============  ==========
Datatype      Examples
============  ==========
`unset!`_
`datatype!`_  logic! int!/decimal!
`none!`_      none
`logic!`_     true false
`word!`_      hello focal-len .s
`lit-word!`_  'hello 'focal-len '.s
`set-word!`_  hello: focal-len: .s:
`get-word!`_  :hello :focal-len :.s
`char!`_      'a' '^-'
`int!`_       1 455 -22
`decimal!`_   3.05  -4.
`coord!`_     0,255,100  -1, 0, 0 
`vec3!`_      0.0,255.0,100.0  -1.0, 0, 0 
`string!`_    "hello"  {hello}
`binary!`_    #{01afed}  #{00 33 ff a0}
time!         10:02 -0:0:32.08
`context!`_
`block!`_     []  [a b c]
`paren!`_     ()  (a b c)
path!         obj/x my-block/2
set-path!     obj/x: my-block/2:
lit-path!     'obj/x 'my-block/2
`vector!`_    #[1 2 3]  #[-85.33 2 44.8]
`func!`_      inc2: func [n] [add n 2]
============  ==========


Unset!
------

Unset is used to indicate that a word has not been assigned a value.


Datatype!
---------

A value which represents a type.


None!
-----

A value used to denote nothing.


Logic!
------

A boolean value of *true* or *false*.


Char!
-----

Special characters can be specified with a caret (^).

========  ===============
Sequence  Character
========  ===============
``'^0'``  Nul (0x00)
``'^-'``  Tab (0x09)
``'^/'``  New line (0x0A)
========  ===============


Int!
----

Integers can be specified in decimal, or if prefixed with '0x', as hexadecimal.

Example integers::

    24
    0x1e


Decimal!
--------

A floating point number.

Example decimal values::

    -3.5685
    24.


Word!
-----

A word is a series of characters which does not contain white space.  The first
character must not be a digit.  All other characters may be alpha-numeric,
mathematical symbols, or punctuation.  Case is ignored in words.

Example words::

    app_version
    _60kHz_flag
    MTP-3
    >


Lit-word!
---------

A literal word evaluates to a word! value.


Set-word!
---------

Used to assign a value to a word.

::

    )> a: 42
    == 42
    )> a
    == 42


Get-word!
---------

Used to get the value of a word without evaluating it.


Binary!
-------

A binary value references a series of bytes.
Binary data is specified with hexadecimal values following a hash and
opening brace (#{) and is terminated with a closing brace (}).
White space is allowed and ignored inside the braces.

::

     #{0000ff01}

     #{0000ff01 0000f000 
       03ad4480 d17e0021}

::

     )> to-binary "hello"
     == #{68656C6C6F}


String!
-------

Strings are UTF-8 text enclosed with either double quotes or braces.
The text can span multiple lines in the script when braces are used.

String examples:

::

   "Alpha Centari"

   {This string
   spans multiple lines.}


Vector!
-------

Vectors hold a series of numbers using less memory than a block!.

All numbers in a vector are either 32-bit integers or floating point values.
If the first number is specified as a decimal!, all numbers will be floating
point.


Coord!
------

Integer coordinate that is handy for specifying screen coordinates, rectangles,
colors, etc.

A coord! can hold up to 6 16-bit integers.

::

   640,480       ; Screen coordinate
   45,10, 45,18  ; Rectangle
   255,10,0      ; RGB triplet


Vec3!
-----

Vec3 stores 3 floating point values.

A Vec3 is specified as two or three decimal numbers separated by commas.
If none of the numbers has a decimal point then the value will be a coord!.

::

    0.0, 1.0     ; Third component will be 0.0
    1.0,0,100


Block!
------

A block is a series of values within brackets.

::

    [1 one "one"]


Paren!
------

Similar to a block, but automatically evaluated.


Func!
-----

Functions can be defined with or without arguments.
The return value of a function is the last evaluated expression.

The *does* word is used to create a function with no arguments.
::

    hello: does [print "Hello World"]

Local functions values can be declared in the signature block. 
These locals are initialized to *none*.

::

    ; Here is a function with two arguments and one local variable.
    my-function: func [arg1 arg2 | var1] [
        ; var1 is none.

        ; TODO: Write this function body.
    ]

Arguments can be limited to certain types by following the argument name with
a datatype in the signature block.

::

    func [
        blk block!
        count int!/decimal!
    ][
        ; ...
    ]


Context!
--------

A context holds word/value pairs.

Example context::

    entry: make context! [
      name: "John"
      age: 44
      job: 'farmer
    ]
    
Contexts can be created from existing ones.  So given the previous entry
context a new farmer could be created using *make* again.
::

    joe: make entry [name: "Joe" age: 32]

The *context* word is normally used to make a new context instead of
*make context!*::

    entry: context [type: hybrid level: 2]



.. _`function reference`: http://urlan.sf.net/boron/doc/func_ref.html
.. _`code documentation`: http://urlan.sf.net/boron/doc/html/
