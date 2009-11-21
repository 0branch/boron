/*
   Extract cfunc! documentation from C source files.
*/

ifn args [
    print "Please specify input C files."
    quit
]


func-info: context [
    name:   none
    args:   []
    return: none
    about:  none
]

ws: charset {^/^- }
non-ws: complement copy ws
nl: '^/'

parse-doc: func [txt | fob tok] [
    fob: make func-info []
    parse txt [
        any ws x: tok: to ws :tok (fob/name: tok) thru nl
        any [
            4 8 ' ' tok: to nl :tok skip (append fob/args tok)
        ]
        thru {return:} tok: to nl :tok (fob/return: trim tok)
        tok: (fob/about: trim tok)
    ]
    fob
]


cfuncs: make block! 100
hfuncs: make block! 40

forall args [
    f: read/text first args
    parse f [some[
        thru "-cf-" in: thru "*/" :in (append cfuncs trim/indent slice in -2)
    ]]
    parse f [some[
        thru "-hf-" in: thru "*/" :in (append hfuncs trim/indent slice in -2)
    ]]
]


to-obj: func [funcs | a] [
    map a sort funcs [parse-doc a]
]

cfuncs: to-obj cfuncs
hfuncs: to-obj hfuncs


doc:  make string! 1024
emit: func [txt] [append doc txt]


argument-names: func [args | str a p] [
    if empty? args [return ""]
    str: clear "" ;make string! 80
    foreach a args [
        append str ' '
        append str
            either p: find a ' ' [slice a p] [a]
    ]
    str
]

argument-table: func [args | str a p opt] [
    either empty? args [nl][
        str: make string! 80
        foreach a args [
            append str {  <tr><td>}

            either equal? first a '/'
                [opt: true]
                [if opt [append str {&nbsp;&nbsp;&nbsp;}]]

            either p: find a ' ' [
                append str slice a p
                append str {</td><td>}
                append str trim p
            ][
                append str a
                append str {</td><td>}
            ]

            append str {</td></tr>}
        ]
        rejoin [
{<p class="func-sec">Arguments</p>
<table border="1" class="func-ref">
<colgroup>
<col width="13%" />
<col width="25%" />
<col width="63%" />
</colgroup>
<tbody valign="top">
}
            str {</tbody>^/</table>^/}
        ]
    ]
]


emit-toc: func [funcs | f] [
    foreach f funcs [
        emit rejoin [
            {<li><a class="index link" href="#} f/name
            {" id="id_} f/name {">} f/name {</a></li>}
        ]
    ]
]

;white: charset " ^-^/"
;parse f [name: to white :name thru white f:]

emit-funcs: func [funcs | f] [
    foreach f funcs [
        emit rejoin [
            {^/<div class="section" id="}
            f/name
            {">^/<h2><a class="toc-backref" href="#id_}
            f/name
            {">}
            uppercase first f/name
            next f/name
            {</a></h2>^/<pre class="func-sig"><b>}
            f/name
            {</b>}
            argument-names f/args
            {</pre>^/}
            argument-table f/args
            {<p class="func-sec">Return</p>^/<div class="return-block">^/<p>}
            f/return
            {</p>^/</div>^/<p>}
            f/about
            {</p>^/</div>^/}
        ]
    ]
]


emit read/text %doc/func_ref_head.html

emit {<div class="contents topic" id="contents">
<p class="topic-title first">Contents</p>
<ul class="auto-toc simple">
<li><a class="index link" href="#cfunc" id="id_ch1">1&nbsp;&nbsp;&nbsp;C Functions</a>
<ul class="auto-toc">
}
emit-toc cfuncs
emit {</ul>
</li>
<li><a class="index link" href="#helpers" id="id_ch2">2&nbsp;&nbsp;&nbsp;Helper Functions</a>
<ul class="auto-toc">}
emit-toc hfuncs
emit {</ul>^/</li>^/</ul>^/</div>}

emit {<div class="section" id="cfunc">
<h1><a class="toc-backref" href="#id_ch1">1&nbsp;&nbsp;&nbsp;C Functions</a></h1>
<p>These are the built-in functions implemented in C.</p>}
emit-funcs cfuncs
emit {</div>
<div class="section" id="helpers">
<h1><a class="toc-backref" href="#id_ch2">2&nbsp;&nbsp;&nbsp;Helper Functions</a></h1>
<p>These are the built-in func! functions.</p>
}
emit-funcs hfuncs
emit {</div>
</body>
</html>
}


print doc
;write %doc/func_ref.html doc
