/*
    Converts Wavefront OBJ files to Boron-GL buffers
    Version: 1.0.0
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


write-geo: func [
    geo
    | indices ic a b c key vi vcount new prev.l prev.k prim-end
][
    indices: /*set-stride 3*/ reserve make vector! 'i32 1000

    if geo/name [
        prin rejoin [{; object "} geo/name {"^/} ]
    ]
    if geo/surfs [
        write-surf geo/surfs
    ]

    prev.k: prev.l: none
    ic: 0

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

        vcount: size? vi
        switch key [
            'vn  [vcount: div vcount 2]
            'vt  [vcount: div vcount 2]
            'vtn [vcount: div vcount 3]
        ]

        new: false

        if ne? key    prev.k [new: true]
        if ne? vcount prev.l [new: true  prev.l: vcount]
        if new [
            if prev.k [prim-end]
            prev.k: key

            append clear indices vcount

            prin "^/buffer "
            prin select [
                'vn  "[vertex normal]"
                'vt  "[vertex texture/2]"
                'vtn "[vertex texture/2 normal]"
                'v   "[vertex]"
            ] key
            print " #["
        ]

        switch key [
            v [
                loop vcount [
                    a: skip geo/verts   mul 3 vi/1
                    ++ vi
                    print [' ' first a second a third a]
                    append indices ++ ic
                ]
            ]
            vn [
                loop vcount [
                    a: skip geo/verts   mul 3 vi/1
                    b: skip geo/normals mul 3 vi/2
                    vi: skip vi 2
                    print [' ' first a second a third a
                               first b second b third b]
                    append indices ++ ic
                ]
            ]
            vtn [
                loop vcount [
                    a: skip geo/verts   mul 3 vi/1
                    b: skip geo/uvs     mul 3 vi/2
                    c: skip geo/normals mul 3 vi/3
                    vi: skip vi 3
                    print [' ' a/1 a/2 a/3  b/1 sub 1.0 b/2  c/1 c/2 c/3]
                    append indices ++ ic
                ]
            ]
        ]
        ;print ["      " vi]
    ]

    if prev.k [prim-end]
]


white:       charset " ^-^D"
index-chars: charset "0123456789/"
digits:      charset "0123456789"

convert-face: func [line geo | verts len key t n] [
    len: 0
    parse line [some[
        some white | some index-chars (++ len)
    ]]

    ; Determine key.
    parse t: n: line [some white some digits '/' t: any digits '/' n:]

    key: either find digits t/1 [
        either find digits n/1 ['vtn]['vt]
    ][
        either find digits n/1 ['vn]['v]
    ]

    verts: to-block replace/all line '/' ' '
    map it verts [sub it 1]

    append geo/faces reduce [key verts]
]


eol: '^/'

; Return surf-contexts
load-mtl: func [file | sfs surf cl sl data] [
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


convert: func [file string!/file! | mtllib main geo cl sl data] [
    prin rejoin [
        "; Boron-GL Draw List^/; File: " file
         "^/; Date: " now/date "^/^/"
    ]

    main: make wf-geom []
    cl: [data: to eol :data skip]
    sl: [thru eol]

    parse read/text file [some[
          '#' thru eol
        | "mtllib" cl (mtllib: load-mtl trim data)
        | "usemtl" cl (geo/surfs: select mtllib trim data)
        | "g"  sl
        | "o"  cl (if geo [write-geo geo]
                   geo: make wf-geom [
                     name:    copy trim data
                     uvs:     main/uvs
                     normals: main/normals
                     verts:   main/verts
                   ])
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
geom-size: func [geo | min max v] [
    max: min: make vec3! geo/verts
    iter/3 geo/verts [
        v: make vec3! it
        min: minimum v min
        max: maximum v max
    ]
    sub max min
]
*/


forall args [convert first args]


;eof
