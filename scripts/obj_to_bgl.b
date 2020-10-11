#!/usr/bin/boron -sp
/*
    Converts Wavefront OBJ files to Boron-GL buffers
    Version: 1.2
*/


wf-surf: context [
    name: none
    diffuse: 0.8,0.8,0.8
    ambient:
    specular:
    color_map:
    bump_map: none
]

wf-geom: context [
    name: none
    verts: []
    normals: []
    uvs: []
    surfs: none
    faces: []
]


write-rgb: func [color-blk] [
    print to-coord mul make vec3! color-blk 255.0
]

write-surf: func [surf] [
    if surf/name [
        prin rejoin [{; usemtl "} surf/name {"^/}]
    ]
    prin "color "
    write-rgb surf/diffuse
    /*
    write-color "ambient"  surf/ambient
    write-color "diffuse"  surf/diffuse
    write-color "specular" surf/specular
    if surf/color_map [
        prin rejoin ["    color_map [ %" surf/color_map " ]^/"]
    ]
    if surf/bump_map [
        prin rejoin ["    bump_map [ %" surf/bump_map " ]^/"]
    ]
    */
]

vindex: []

; Search for a copy of the vertex at the end of buf and return its index.
emit-vertex-similar: func [buf stride] [
    new-attrib: skip tail buf negate stride
    either pos: find vindex new-attrib [
        ;print "; reused"
        clear new-attrib
    ][
        pos: tail vindex
        append vindex mark-sol slice new-attrib stride
        print [' ' to-text new-attrib]
    ]
    sub index? pos 1
]

; Print vertex at the end of buf and return its index.
emit-vertex: func [buf stride] [
    new-attrib: skip tail buf negate stride
    print [' ' to-text new-attrib]
    div index? new-attrib stride
]

write-geo: func [
    geo
    /local key vi
][
    indices: /*set-stride 3*/ reserve make vector! 'u16 1000

    if geo/name [
        prin rejoin [{; object "} geo/name {"^/} ]
    ]
    if geo/surfs [
        write-surf geo/surfs
    ]

    prev.k: prev.l: none
    ; ic: 0
    clear vindex
    attrib: make vector! 'f32

    prim-end: does [
        print ']'
        prin select [
            3  "^/tris "
            4  "^/quads "
               "^/polys "
        ] first head indices
        print to-string next indices
    ]

    foreach [key vi] geo/faces [
        ;print [key vi]

        vcount: div size? vi select ['vn 2 'vt 2 'vtn 3 'v 1] key

        new: false

        if ne? key    prev.k [new: true]
        if ne? vcount prev.l [new: true  prev.l: vcount]
        if new [
            if prev.k [prim-end]
            prev.k: key

            append clear indices vcount

            print [
                "^/buffer" select [
                    'vn  "[vertex normal]"
                    'vt  "[vertex texture 2]"
                    'vtn "[vertex texture 2 normal]"
                    'v   "[vertex]"
                ] key "#["
            ]
        ]

        switch key [
            v [
                loop vcount [
                    a: skip geo/verts   mul 3 vi/1
                    ++ vi

                    append attrib slice a 3
                    append indices emit-vertex attrib 3
                ]
            ]
            vn [
                loop vcount [
                    a: skip geo/verts   mul 3 vi/1
                    b: skip geo/normals mul 3 vi/2
                    vi: skip vi 2

                    append attrib slice a 3
                    append attrib slice b 3
                    append indices emit-vertex attrib 6
                ]
            ]
            vtn [
                loop vcount [
                    a: skip geo/verts   mul 3 vi/1
                    b: skip geo/uvs     mul 2 vi/2
                    c: skip geo/normals mul 3 vi/3
                    vi: skip vi 3

                    append attrib slice a 3
                    append attrib slice b 2
                    append attrib slice c 3
                    append indices emit-vertex attrib 8
                ]
            ]
        ]
        ;print ["      " vi]
    ]

    if prev.k [prim-end]
]


white:  charset " ^-^D"
digits: charset "0123456789"

convert-face: func [line geo] [
    ; Determine key.
    parse t: n: line [some white some digits '/' t: any digits '/' n:]

    key: ['v 'vn 'vt 'vtn]
    if find digits t/1 [key: skip key 2]
    if find digits n/1 [++ key]

    verts: to-block replace/all line '/' ' '
    map it verts [sub it 1]

    append geo/faces reduce [first key verts]
]


eol: '^/'

; Return surf-contexts
load-mtl: func [file] [
    sfs: make block! 0

    ; Material
    either exists? file [
        cl: [data: to eol :data skip]
        sl: [thru eol]
        parse read/text file [some[
              '#' thru eol
            | "newmtl"  cl (surf: make wf-surf [name: trim data]
                            append sfs surf/name
                            append sfs surf)
            | "Ns"      sl
            | "d"       sl
            | "illum"   sl
            | "Ka"      cl (surf/ambient: to-block data)
            | "Kd"      cl (surf/diffuse: to-block data)
            | "Ke"      sl
            | "Ks"      cl (surf/specular: to-block data)
            | "map_Kd"  cl (surf/color_map: trim data)
            | "map_Bump" cl (surf/bump_map: trim data)
            | sl  ; cl (print [";" data])
            | end skip
        ]]
    ][
        print ["; Material file not found:" file eol]
    ]

    sfs
]


convert: func [file string!/file!] [
    print rejoin [
        "; Boron-GL Draw List^/; File: " file
         "^/; Date: " now/date '^/'
    ]

    main: make wf-geom []
    cl: [data: to eol :data skip]
    ;sl: [thru eol]
    make-geo: does [
        geo: make wf-geom [
            name:    copy trim data
            uvs:     main/uvs
            normals: main/normals
            verts:   main/verts
        ]
    ]

    parse read/text file [some[
          '#' thru eol
        | "mtllib" cl (mtllib: load-mtl trim data)
        | "usemtl" cl (if mtllib [geo/surfs: select mtllib trim data])
        | "g"  cl (ifn geo [make-geo])
        | "o"  cl (if geo [write-geo geo] make-geo)
        | "vt" cl (append main/uvs     to-block data)
        | "vn" cl (append main/normals to-block data)
        | "v"  cl (append main/verts   to-block data)
        | "f"  cl (convert-face data geo)
        ;| "f"  data: to end :data (print "<END>"
        ;                           convert-face data geo)
        | eol skip
        | cl (print [";" data])
    ]]

    write-geo geo
]


/*
geom-size: func [geo] [
    max: min: make vec3! geo/verts
    iter/3 geo/verts [
        v: make vec3! it
        min: minimum v min
        max: maximum v max
    ]
    sub max min
]
*/


files: []
forall args [
    switch first args [
        "-s" [emit-vertex: :emit-vertex-similar]
        "-h" [
            print {{
                Usage: obj_to_bgl.b [-h] [-s] <obj-file> ...

                Options:
                  -h   Print this help and quit
                  -s   Eliminate similar vertices
            }}
            quit
        ]
        [append files first args]
    ]
]

forall files [
    convert first files
]


;eof
