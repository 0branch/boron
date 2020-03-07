---
title:  Boron User Manual
date:   Version 2.0.2, 2020-03-07
---


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

There is a separate [function reference] and [code documentation]
available online at <http://urlan.sourceforge.net/boron>.



Scripts
=======

Scripts are UTF-8 encoded text files.


Comments
--------

Single line comments begin with a semi-colon.

Block comments are the same as C block comments.  They begin with '``/*``' and
continue through '``*/``'.  Block comments cannot be nested.

Comment examples:

    ; line comment

    add 2 4 ; result is 6

    /*
      Block comment
    */


Shell Invocation
----------------

The first line of a script may be a UNIX shell sha-bang (``#!``) command.

    #!/usr/bin/boron


Command Line Usage
------------------

Usage:

    boron [options] [script] [arguments]


### Command Line Options

----------  ------------------------
-e "*exp*"  Evaluate expression
-h          Show help and exit
-p          Disable prompt and exit on exception
-s          Disable security
----------  ------------------------


### Command Line Arguments

If the interpreter is invoked with a script then the *args* word will be set
to either a block of strings, or *none* if no script arguments were given.

So this Boron command:

    boron -e "probe args" file1 -p 2

Will print this:

    ["file1" "-p" "2"]



Datatypes
=========

Datatype                Examples
----------------------  --------------------
[unset!](#unset)
[datatype!](#datatype)  logic! int!/double!
[none!](#none)          none
[logic!](#logic)        true false
[word!](#word)          hello focal-len .s
[lit-word!](#lit-word)  'hello 'focal-len '.s
[set-word!](#set-word)  hello: focal-len: .s:
[get-word!](#get-word)  :hello :focal-len :.s
[option!](#option)      /hello /focal-len /.s
[char!](#char)          'a' '^-' '^(01f3)'
[int!](#int)            1 455 -22
[double!](#double)      3.05  -4.  6.503e-8
[coord!](#coord)        0,255,100  -1, 0, 0
[vec3!](#vec3)          0.0,255.0,100.0  -1.0, 0, 0
[string!](#string)      "hello"  {hello}
[file!](#file)          %main.c %"/mnt/Project Backup/"
[binary!](#binary)      #{01afed}  #{00 33 ff a0}  2#{00010010}
[bitset!](#bitset)      make bitset! "abc"
time!                   10:02 -0:0:32.08
[vector!](#vector)      #[1 2 3]  #[-85.33 2 44.8] i16#[10 0 -4 0]
[block!](#block)        []  [a b c]
[paren!](#paren)        ()  (a b c)
[path!](#path)          obj/x my-block/2
lit-path!               'obj/x 'my-block/2
set-path!               obj/x: my-block/2:
[context!](#context)    context [area: 4,5 color: red]
[hash-map!](#hash-map)  make hash-map! [area 4,5 "color" red]
error!
[func!](#func)          inc2: func [n] [add n 2]
[cfunc!](#cfunc)
[port!](#port)
----------------------  --------------------


Unset!
------

Unset is used to indicate that a word has not been assigned a value.


Datatype!
---------

A value which represents a type or set of types.  To declare a type use it's
type word, which have names ending with an exclamation (!) character.
To declare a set use a series of type words separated by slashes (/).

	none!
	char!/int!/double!


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

Sequence             Character Value
-------------------  -----------------------
``^-``               Tab, 0x09
``^/``               New line, 0x0A
``^^``               Caret, 0x5E
``^0`` - ``^F``      Hexidecimal nibble, 0x00 - 0x0F
``^(xxxx)``          Hexidecimal number, 0x0000 - 0xFFFF
-------------------  -----------------------

For example, a new line character could be declared in any of the following
ways:

    '^/' '^a' '^(0A)'


Int!
----

A 64-bit integer number.
Integers can be specified in decimal, or if prefixed with '0x', as hexadecimal.

Example integers:

    24
    0x1e


Double!
--------

A 64-bit floating point number.

Example double values:

    -3.5685
    24.
    6.503e-8


Coord!
------

Integer coordinate that is handy for specifying screen positions, rectangles,
colors, etc.

A coord! can hold up to six 16-bit integers.

    640,480       ; Screen size
    45,10, 45,18  ; Rectangle
    255,10,0      ; RGB triplet


Vec3!
-----

Vec3 stores 3 floating point values.

A Vec3 is specified as two or three decimal numbers separated by commas.
If none of the numbers has a decimal point then the value will be a coord!.

    0.0, 1.0     ; Third component will be 0.0
    1.0,0,100


Word!
-----

A word is a series of ASCII characters which does not contain white space.
The first character must not be a digit.  All other characters may be
alpha-numeric, mathematical symbols, or punctuation.  Case is ignored in words.

Example words:

    app_version
    _60kHz_flag
    MTP-3
    >


Lit-word!
---------

A literal word is a variant of a *word!* that begins with a single quote (')
character.

It evaluates to a word! value rather than the value from any bound context.

    )> 'sleep
    == sleep


Set-word!
---------

A set-word value is a variant of a *word!* that ends with a colon (:)
character.

It is used to assign a value to a word.

    )> a: 42
    == 42
    )> a
    == 42


Get-word!
---------

A get-word value is a variant of a *word!* that begins with a colon (:)
character.

    :my-function

It is used to obtain the value of a word without evaluating it further.
For many datatypes this will be the same as using the word itself, but for
[func!](#func) values it will suppress invocation of the function.


Option!
-------

An option is a variant of a *word!* that begins with a slash (/) character.

	/outside

An option is inert will evaluate to itself.


Binary!
-------

A binary value references a series of bytes.
Binary data is specified with hexadecimal values following a hash and
opening brace (#{) and is terminated with a closing brace (}).
White space is allowed and ignored inside the braces.

~~~~
#{0000ff01}

#{0000ff01 0000f000
  03ad4480 d17e0021}
~~~~

~~~~
)> to-binary "hello"
== #{68656C6C6F}
~~~~

Alternative encodings for base 2 and base 64 can be used by putting the
base number before the initial hash (#) character.

    )> print to-string 2#{01101000 01100101 01101100 01101100 01101111}
    hello

    )> print to-string 64#{aGVsbG8=}
    hello

Partial base 64 triplets will automatically be padded with equal (=)
characters.

    )> print 64#{aGVsbG8}   ; "hello"
    64#{aGVsbG8=}

    )> print 64#{ZG9vcg}    ; "door"
    64#{ZG9vcg==}

Use the *encode* function to change the encoding base.

    )> encode 16 2#{11001010 10110010}
    == #{CAB2}


Bitset!
-------

The *charset* function is a shortcut for ``make bitset!``.


String!
-------

Strings are UTF-8 text enclosed with either double quotes (") or braces ({}).
They can include the same caret character sequences as [char!](#char)
values.

Double quoted strings cannot contain a new line character or double quote
unless it is in escaped form.

    "Alpha Centari"

    "First line with ^"quotes^".^/Second line.^/"

There are two brace formats, both of which can span multiple lines in the
script.

A single left brace will track pairs of left/right braces before
terminating with a single right brace.  Other brace combinations must use
escape sequences.

Two or more left braces followed by a new line will start the string on the
next line and terminate it on the line prior to a line ending with a matching
number of right braces.  The enclosed text will be automatically unindented.

    {This string
        has three lines and
        will preserve all whitespace.}

    {Braces allow "quoting" without escape sequences.}

    {{
        This is four lines that will be unindented.
        Item 1
          - Subitem A
          - Subitem B
    }}


File!
-----

A file value is a string which names a file or directory on the local
filesystem.  They begin with a percent (%) character.  If any spaces are
present in the path then it must be enclosed in double quotes.

File examples:

    %/tmp/dump.out
    %"../Input Files/test42"
    %C:\windows\system32.exe


Vector!
-------

Vectors hold a series of numbers using less memory than a [block!](#block).
All numbers in a vector are integers or floating point values of the same size.

A 32-bit vector! is specified with numbers following a hash and opening square
bracket (#[) and are terminated with a closing square bracket (]).
If the first number contains a decimal point, all numbers will be floating
point, otherwise they will all be integers.

Other types of numbers can be specified with the form name immediately before
the hash.

Name       C Type
---------  ---------
i16        int16\_t
u16        uint16\_t
i32        int32\_t
u32        uint32\_t
f32        float
f64        double
---------  ---------

Some vector! examples:

    )> a: #[1 2 3 4]
    == #[1 2 3 4]

    )> b: #[1.0 2 3 4]
    == #[1.0 2.0 3.0 4.0]

    )> c: i16#[1 2 3 4]
    == i16#[1 2 3 4]

    )> foreach w [a b c] [v: get w print [w type? last v size? to-binary v]]
    a int! 16
    b double! 16
    c int! 8


Block!
------

A block is a series of values within square brackets.

    [1 one "one"]

	[
		item1: [some sub-block]
		item2: [another sub-block]
	]

Paren!
------

A paren is similar to a block!, but it will be automatically evaluated.

	(1 two "three")


Path!
-----

A path is a word! followed by one or more word!/get-word!/int! values
separated by slash (/) characters.

Example paths:

    object/order/1

    list/:variable


Context!
--------

A context holds word/value pairs.

Example context:

    entry: make context! [
      name: "John"
      age: 44
      job: 'farmer
    ]

Contexts can be created from existing ones.  So given the previous entry
context a new farmer could be created using *make* again.

    joe: make entry [name: "Joe" age: 32]

The set-word! values in the outermost specification block become members of
the context, but not those in any inner blocks.

The *context* word is normally used to make a new context instead of
*make context!*.

    unit: context [type: 'hybrid level: 2]


Hash-Map!
---------

A hash-map holds key/value pairs.

Use *make* to create a hash-map from a block containing the key & value pairs.

	level-map: make hash-map! [
		0.0  "Minimum"
		0.5  "Average"
		1.0  "Maximum"
	]

Use *pick* & *poke* to get and set values.  If the key does not exist *pick*
will return *none!*.

	)> pick level-map 0.0
	== "Minimum"

	)> pick level-map 0.1
	== none

	)> poke level-map 0.1 "Slight"
	== make hash-map! [
		0.0 "Minimum"
		0.5 "Average"
		1.0 "Maximum"
		0.1 "Slight"
	]

	)> pick level-map 0.1
	== "Slight"

Func!
-----

Functions can be defined with or without arguments.
The return value of a function is the last evaluated expression.

The *does* word can be used to create a function from a block of code when no
arguments or local variables are needed.  In the following example 'name is
bound to an external context.

    hello: does [print ["Hello" name]]

The *func* word is used when arguments or local variables are required.
It is followed by the signature block and code block.
Required arguments, optional arguments, local values, and external values are
declared in the signature block.
Any local values and unused optional arguments are initialized to *none!*.

    ; Here is a function with two arguments and one local variable.
    my-function: func [arg1 arg2 /local var] [
        ; var is none! here.

        foreach var arg1 [print add var arg2]	; var is set by foreach.
    ]

The required arguments are *word!* values at the start of the signature block.
Optional arguments are specified after this with *option!* values followed by
zero or more *word!* values.
The */local* and */extern* options will be last, each followed by one or more
*word!* values.

Any variable assigned in the function with a *set-word!* value is automatically
made a local value.  To prevent this and keep the original binding from when
the function was created use the */extern* option.

	append-item: func [item /extern list-size] [
		was-empty: empty? obj-list		; was-empty is local.
		append obj-list item
		list-size: size? obj-list       ; list-size is external.
		was-empty
	]

Arguments can be limited to certain types by following the argument name with
a datatype in the signature block.  This example includes an optional argument
and both the required and optional arguments are type checked.

	play-music: func [path file! /volume loudness int!/double!] [
		if volume [
			set-audio-volume loudness
		]
	    play-audio-file path
	]

Optional arguments are used by calling the function with a *path!*. If there
are multiple options the order in the path does not matter, but this path order
does indicate the order in which any option arguments must appear.
The play-music function above can be called two ways:

	play-music %/data/interlude.ogg				; Invoke without option.

	play-music/volume %/data/interlude.ogg 0.5	; Invoke with /volume option.


Cfunc!
------

This type is for the built-in functions written in C.
See the [function reference] for the available functions.


Port!
-----

Ports are a general interface for various input/ouput devices.

The *open* and *close* functions create and destroy ports.
The *read* and *write* functions are used to recieve and send data.


### Standard IO Ports

To use ``stdin``, ``stdout``, and ``stderr`` streams use *open* with the
integer 0, 1, or 2.

To read commands from stdin:

    t: open 0
    cmd: ""
    forever [
        wait t
        read/into t cmd
        if eq? cmd "quit^/" [break]
        print cmd
    ]


### Network Ports

Here is a simple TCP server which sends clients a message:

    s: open "tcp://:6044"
    forever [
        con: read wait s
        write con "Hello, client.^/"
        close con
    ]

And the client:

    s: open "tcp://localhost:6044"
    print to-string read s
    close s


Parse Language
==============

The *parse* function can operate on strings, blocks, and binary values.
It returns *true* if the end of the input is reached.


Block Parse
-----------

Rule-Statement    Operation
----------------  -------------------------------------------
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
----------------  -------------------------------------------


String Parse
------------

Rule-Statement    Operation
----------------  -------------------------------------------
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
----------------  -------------------------------------------


Value
---------  -------------------------------------------
bitset!    Match any character in the set.
block!     Sub-rules.
char!      Match a single character.
string!    Match a string.
word!      Match value of word.
---------  -------------------------------------------


[function reference]: http://urlan.sf.net/boron/doc/func_ref.html
[code documentation]: http://urlan.sf.net/boron/doc/html/
