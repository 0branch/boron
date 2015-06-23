==============================
     Boron User Manual
==============================

:Version:   0.2.12
:Date:      |date|

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


Command Line Usage
------------------

Usage::

    boron [options] [script] [arguments]


Command Line Options
~~~~~~~~~~~~~~~~~~~~

==========  ========================
-e "*exp*"  Evaluate expression
-h          Show help and exit
-p          Disable prompt and exit on exception
-s          Disable security
==========  ========================


Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

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
`char!`_      'a' '^-' '^(01f3)'
`int!`_       1 455 -22
`decimal!`_   3.05  -4.
`coord!`_     0,255,100  -1, 0, 0 
`vec3!`_      0.0,255.0,100.0  -1.0, 0, 0 
`string!`_    "hello"  {hello}
`file!`_      %main.c %"/mnt/Project Backup/"
`binary!`_    #{01afed}  #{00 33 ff a0}
`bitset!`_    make bitset! "abc"
time!         10:02 -0:0:32.08
`vector!`_    #[1 2 3]  #[-85.33 2 44.8]
`block!`_     []  [a b c]
`paren!`_     ()  (a b c)
`path!`_       obj/x my-block/2
lit-path!     'obj/x 'my-block/2
set-path!     obj/x: my-block/2:
`context!`_   context [area: 4,5 color: red]
error!
`func!`_      inc2: func [n] [add n 2]
`port!`_
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

A Unicode character.  A char! can be specified with either a UTF-8 character
between two single quotes, or an ASCII caret (^) sequence between two single
quotes.

The following caret sequences can be used:

===================  =======================
Sequence             Character Value
===================  =======================
``^-``               Tab, 0x09
``^/``               New line, 0x0A
``^^``               Caret, 0x5E
``^0`` - ``^F``      Hexidecimal nibble, 0x00 - 0x0F
``^(xxxx)``          Hexidecimal number, 0x0000 - 0xFFFF
===================  =======================

For example, a new line character could be declared in any of the following
ways::

    '^/' '^a' '^(0A)'


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


Coord!
------

Integer coordinate that is handy for specifying screen positions, rectangles,
colors, etc.

A coord! can hold up to six 16-bit integers.

::

   640,480       ; Screen size
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


Word!
-----

A word is a series of ASCII characters which does not contain white space.
The first character must not be a digit.  All other characters may be
alpha-numeric, mathematical symbols, or punctuation.  Case is ignored in words.

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


Bitset!
-------

The *charset* function is a shortcut for ``make bitset!``.



String!
-------

Strings are UTF-8 text enclosed with either double quotes or braces.
The text can span multiple lines in the script when braces are used.

Strings can include the same caret character sequences as `char!`_ values.

String examples::

   "Alpha Centari"

   {This string
   spans multiple lines.}

   "First line^/Second line^/"


File!
-----

A file value is a string which names a file or directory on the local
filesystem.  They begin with a percent (%) character.  If any spaces are
present in the path then it must be enclosed in double quotes.

File examples::

    %/tmp/dump.out
    %"../input files/test42"
    %C:\windows\system32.exe


Vector!
-------

Vectors hold a series of numbers using less memory than a block!.

All numbers in a vector are either 32-bit integers or floating point values.
If the first number is specified as a decimal!, all numbers will be floating
point.


Block!
------

A block is a series of values within brackets.

::

    [1 one "one"]


Paren!
------

Similar to a block, but automatically evaluated.


Path!
-----

Example paths::

    object/entries/1


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

    unit: context [type: 'hybrid level: 2]


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


Port!
-----

Ports are a general interface for various input/ouput devices.

The *open* and *close* functions create and destroy ports.
The *read* and *write* functions are used to recieve and send data.


Standard IO Ports
~~~~~~~~~~~~~~~~~

To use ``stdin``, ``stdout``, and ``stderr`` streams use *open* with the
integer 0, 1, or 2.

To read commands from stdin::

    t: open 0
    cmd: ""
    forever [
        wait t
        read/into t cmd
        if eq? cmd "quit^/" [break]
        print cmd
    ]


Network Ports
~~~~~~~~~~~~~

Here is a simple TCP server which sends clients a message::

    s: open "tcp://:6044"
    forever [
        con: read wait s
        write con "Hello, client.^/"
        close con
    ]

And the client::

    s: open "tcp://localhost:6044"
    print to-string read s
    close s


Parse Language
==============

The *parse* function can operate on strings, blocks, and binary values.
It returns *true* if the end of the input is reached.


Block Parse
-----------

================  ===========================================
Rule-Statement    Operation
================  ===========================================
\|                Start an alternate rule.
any <val>         Match the value zero or more times.
break             Stop the current sub-rule as a successful match.
into <rules>      Parse block at current input position with a new set of rules.
opt <val>         Match the value zero or one time.
place <ser>       Set the current input position to the given series position.
set <word>        Set the specified word to the current input value.
skip              Skip a single value.
some <val>        Match the value one or more times.
thru <val>        Skip input until the value is found, then continue through it.
to <val>          Skip input until the value is found.
int! <val>        Match a value an exact number of times.
int! int! <val>   Match a value a variable number of times.
int! skip         Skip a number of values.
block!            Sub-rules.
datatype!         Match a single value of the given type.
paren!            Evaluate Boron code.
set-word!         Set word to the current input position.
get-word!         Set slice end to the current input position.
lit-word!         Match the word in the input.
================  ===========================================


String Parse
------------

================  ===========================================
Rule-Statement    Operation
================  ===========================================
\|                Start an alternate rule.
any <val>         Match the value zero or more times.
break             Stop the current sub-rule as a successful match.
opt <val>         Match the value zero or one time.
place <ser>       Set the current input position to the given series position.
skip              Skip a single character.
some <val>        Match the value one or more times.
thru <val>        Skip input until the value is found, then continue through it.
to <val>          Skip input until the value is found.
int! <val>        Match a value an exact number of times.
int! int! <val>   Match a value a variable number of times.
int! skip         Skip a number of characters.
paren!            Evaluate Boron code.
set-word!         Set word to the current input position.
get-word!         Set slice end to the current input position.
================  ===========================================

=========  ==================================================
Value      
=========  ==================================================
bitset!    Match any character in the set.
block!     Sub-rules.
char!      Match a single character.
string!    Match a string.
word!      Match value of word.
=========  ==================================================



.. |date| date::
.. _`function reference`: http://urlan.sf.net/boron/doc/func_ref.html
.. _`code documentation`: http://urlan.sf.net/boron/doc/html/
