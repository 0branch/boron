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
    group:  none
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
        thru {return:}         tok: to nl :tok (fob/return: trim tok)
        opt [some ws {group:}  tok: to nl :tok (fob/group:  trim tok)]
        tok: (fob/about: tok)
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

            append str {</td></tr>^/}
        ]
        rejoin [
{<p class="func-sec">Arguments</p>
<table border="1" class="func-ref">
<colgroup>
<col width="15%" />
<col width="75%" />
</colgroup>
<tbody valign="top">
}
            str {</tbody>^/</table>^/}
        ]
    ]
]


emit-toc: func [funcs | f it it2 cskip item] [
    item: does [
        f: f/name
        emit rejoin [
            {<td><a class="index link" href="#} f
            {" id="id_} f {">} f {</a></td>}
        ]
    ]

    emit {<ul class="auto-toc">^/<table width=90%">^/}
    emit trim/indent {
    <colgroup>
    <col width="30%" />
    <col width="30%" />
    <col width="30%" />
    </colgroup>
    }

    cskip: div add 2 size? funcs 3
    it: slice funcs cskip
    it2: skip funcs cskip
    forall it [
        emit {<tr>}
        f: first it
        item
        if f: first it2 [
            item
            if f: first skip it2 cskip [
                item
            ]
        ]
        ++ it2
        emit {</tr>^/}
    ]

    emit {</table>^/</ul>^/}
]


groups: [
    audio    [] "Audio"
    control  [] "Control Flow"
    data     [] "Data"
    eval     [] "Evaluation"
    math     [] "Math"
    io       [] "Input/Output"
    gl       [] "OpenGL"
    os       [] "Operating System"
    series   [] "Series"
    storage  [] "Storage"
]

emit-groups: func [| add-grp grp tok] [
    non-term: complement charset " ,^-^/"
    add-grp: [
        if f/group [
            parse f/group [some [
                tok: some non-term :tok (
                    grp: select groups to-word tok
                    either grp [
                        append grp f
                    ][
                        print ["Unknown group" f/group]
                    ]
                )
              | skip
            ]]
        ]
    ]
    foreach f cfuncs add-grp
    foreach f hfuncs add-grp

    foreach [word blk title] groups [
        ifn empty? blk [
            emit rejoin [{<li>} title]
            emit-toc blk
            emit {</li>^/}
        ]
    ]
]


context [
    state: none

    transit: func [new] [
        if ne? state new [
            switch state [
                code [emit {</pre>^/}]
                para [emit {</p>^/}]
            ]
            switch state: new [
                code [emit {<pre class="literal-block">}]
                para [emit {<p>}]
            ]
        ]
    ]

    set 'markup-lines func [txt | tok] [
        state: none
        parse txt [some[
            tok:
            4 ' ' thru nl :tok  (transit 'code emit skip tok 4)
          | nl                  (transit none)
          | thru nl :tok        (transit 'para emit tok)
        ]]
        transit none
    ]
]

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
            {</p>^/</div>^/}
        ]
        markup-lines f/about
        emit {</div>^/}
    ]
]


current-date: func [| cd] [
    cd: to-string now/date
    rejoin [
        pick [Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec]
            to-int slice cd 5,2
        ' ' slice cd 8,2
        ' ' slice cd 4
    ]
]


rhead: read/text %doc/func_ref_head.html
change find/last rhead "0.2.2" replace/all to-string environs/version ',' '.'
change find/last rhead "Nov 20 2011" current-date
emit rhead

emit {<div class="contents topic" id="contents">
<p class="topic-title first">Contents</p>
<ul class="auto-toc simple">
<li><a class="index link" href="#cfunc" id="id_ch1">1&nbsp;&nbsp;&nbsp;C Functions</a>
}
emit-toc cfuncs
emit {</li>
<li><a class="index link" href="#helpers" id="id_ch2">2&nbsp;&nbsp;&nbsp;Helper Functions</a>
}
emit-toc hfuncs
emit {</li>^/</ul>^/</div>}


emit {<p class="topic-title">Function Groups</p>
<ul class="auto-toc simple">
}
emit-groups
emit {</ul>^/</div>^/}


emit {<div class="section" id="cfunc">
<h1><a class="toc-backref" href="#id_ch1">1&nbsp;&nbsp;&nbsp;C Functions</a></h1>
<p>These are the built-in functions implemented in C.</p>}
emit-funcs cfuncs
emit {</div>
<div class="section" id="helpers">
<h1><a class="toc-backref" href="#id_ch2">2&nbsp;&nbsp;&nbsp;Helper Functions</a></h1>
<p>These are the built-in func! functions and aliases.</p>
}
emit-funcs hfuncs
emit {</div>
</body>
</html>
}


print doc
;write %doc/func_ref.html doc
