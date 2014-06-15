; Heightmap
;
; Requries: set-matrix 'transpose, demo view-cam


;  0 0,0 - 3 1,0 - 6 2,0
;    |       |       |     dim: 3
;    |       |       |
;  1 0,1 - 4 1,1 - 7 2,1
;    |       |       |
;    |       |       |
;  2 0,2 - 5 1,2 - 8 2,2

heightmap-geo: funct [dim | x z] [
    vec: reserve make vector! 'f32 mul 3 mul dim dim
    idx: make vector! 'i32

    zl: reduce ['z 0 sub dim 1]

    loop reduce ['x 0 sub dim 1] [
        loop zl [
            append append append vec x 0.0 z
        ]
    ]

    loop reduce ['x 0 sub dim 2] [
        xi: mul x dim
        loop zl [
            i: add z xi
            append idx add dim i
            append idx i
        ]
        if lt? x sub dim 2 [        ; Two degenerate tris between strips.
            append idx i
            append idx add add i dim 1
        ]
    ]

    context [vertices: vec indices: idx]
]


cam-space-vec: 0.0, 0.0, 1.0

solid-sh: load-shader %data/shader/solid.b

terrain-sh: make shader! [
    vertex {
uniform sampler2D hmap;
uniform vec2 hdim;
uniform float elev_scale;
varying vec3 normal;
varying vec2 uv;

void main()
{
    uv = vec2(gl_Vertex.x + 0.5, gl_Vertex.z + 0.5);

    vec2 ep = uv / hdim;
    vec2 tdim = vec2(1.0, 1.0) / hdim;
    vec3 center;
    vec3 adj[4];

    center = vec3( gl_Vertex.x,
                   texture2D( hmap, ep ).r * elev_scale,
                   gl_Vertex.z );

    adj[0] = vec3( gl_Vertex.x - 1.0,
               texture2D( hmap, vec2(ep.x - tdim.x, ep.y) ).r * elev_scale,
               gl_Vertex.z );

    adj[1] = vec3( gl_Vertex.x,
               texture2D( hmap, vec2(ep.x, ep.y - tdim.x) ).r * elev_scale,
               gl_Vertex.z - 1.0 );

    adj[2] = vec3( gl_Vertex.x + 1.0,
               texture2D( hmap, vec2(ep.x + tdim.x, ep.y) ).r * elev_scale,
               gl_Vertex.z );
                   
    adj[3] = vec3( gl_Vertex.x,
               texture2D( hmap, vec2(ep.x, ep.y + tdim.x) ).r * elev_scale,
               gl_Vertex.z + 1.0 );

    normal = normalize( cross( adj[0] - center, adj[1] - center ) );
    normal += normalize( cross( adj[1] - center, adj[2] - center ) );
    normal += normalize( cross( adj[2] - center, adj[3] - center ) );
    normal += normalize( cross( adj[3] - center, adj[0] - center ) );
    normal /= 4.0;

    gl_Position = gl_ModelViewProjectionMatrix * vec4(center, 1.0);
    gl_FrontColor = gl_Color;
}
}

    fragment {
uniform sampler2D cmap;
uniform vec3 light_dir;     // Light direction vector * modelView matrix.
//uniform vec3 light_half;    // Light half vector (also in camera space).
uniform float ambient;
varying vec3 normal;
varying vec2 uv;

void main()
{
    vec3 n, col;
    float shade, NdotHV;
    float shine = 18.0;

    n = normalize(normal);
    shade = dot(light_dir, n);
    col = texture2D( cmap, uv ).rgb * max(shade, ambient);
    /*
    if( shade > 0.0 )
    {
        NdotHV = max(dot(light_half, n), 0.0);
        col += pow(NdotHV, shine);
    }
    */

    gl_FragColor = vec4(col, 1.0);  
}
}
    default [
        light_dir: 0.0,1.0,0.0
        ;light_half: normalize add cam-space-vec light_dir
        hmap: load-texture %data/image/height01.png
        cmap: load-texture/mipmap %data/image/ground01.png
        hdim: to-vec3 hmap/size
        elev_scale: 20.0
        ambient: 0.2
    ]
]


geo: heightmap-geo to-int terrain-sh/hdim/1
vec: geo/vertices
idx: geo/indices

normal-matrix: copy unit-matrix

; Main directional light.
sun-dir: normalize 0.1,-1.0,0.0


terrain-dl: draw-list [
    clear
    camera view-cam
    (
        ; Setup for directional lighting with no model transform.
        set-matrix normal-matrix view-cam/orient
        poke normal-matrix 13 0.0,0,0
        set-matrix normal-matrix 'transpose

        ;terrain-sh/light_half: normalize add cam-space-vec
            terrain-sh/light_dir: mul-matrix sun-dir normal-matrix
    )

    depth-test on
    cull on
    blend off

    color 0.0,0.2,1.0
    shader solid-sh
    sphere 1.0 32,32

    shader terrain-sh
    ;sphere 1.0 32,32
    buffer [vertex] vec
    translate -64.0,0.0,-64.0
    tri-strip idx

    translate  127.0,0.0,0.0 tri-strip idx
    translate  127.0,0.0,0.0 tri-strip idx
    translate  0.0,0.0,127.0 tri-strip idx
    translate -127.0,0.0,0.0 tri-strip idx
    translate -127.0,0.0,0.0 tri-strip idx
    translate  0.0,0.0,127.0 tri-strip idx
    translate  127.0,0.0,0.0 tri-strip idx
    translate  127.0,0.0,0.0 tri-strip idx
]

view-cam/near: 0.33
view-cam/far: 3000.0

move-cam: func ['axis delta] bind [
    focal-pnt: switch axis [
        z [add focal-pnt mul to-vec3 skip orient 8 delta]
        y [add focal-pnt mul to-vec3 skip orient 4 delta]
        x [add focal-pnt mul to-vec3 orient delta]
        0.0,0,0
    ]
    turntable view-cam 0,0
] view-cam


window: make widget! [
    root [
        close: [quit]
        key-down: [
            up      [move-cam z -1.0]
            down    [move-cam z  1.0]
            left    [move-cam x -1.0]
            right   [move-cam x  1.0]
            home    [move-cam home 0]
            pg-up   [move-cam y  1.0]
            pg-down [move-cam y -1.0]
            esc  [quit]
            r    [recycle]
            f9   [throw 'reload]
        ]
        mouse-move: [
            ifn eq? 0 and event/5 mouse-lb [
                turntable view-cam mul 0.5,-0.5 slice event 2,2
            ]
        ]
        mouse-wheel: [
            zoom: mul zoom pick [0.90909 1.1] gt? event 0
            view-cam/orbit/3: mul 15.0 zoom
            turntable view-cam 0,0
        ]
        resize: [view-cam/viewport: event]
    ]
]

forever [
    draw terrain-dl
    display-swap
    handle-events/wait window
]
