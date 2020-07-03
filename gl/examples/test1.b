;p: load-image %/home/karl/src/primal/gui/icons/logo.png
;save-png %out p

;print gl-extensions


fontA: make font!
    [%data/font/20thfont.ttf 22]
;   reduce [read %data/font/20thfont-22.rfont
;           load-image %data/font/20thfont-22.png]
 tex: fontA/texture
;tex: load-texture %/home/karl/src/primal/gui/icons/logo.png


test-sh: make shader! [
    vertex {{
        #version 310 es
        layout(location = 0) uniform mat4 transform;
        layout(location = 0) in vec3 position;
        layout(location = 1) in vec2 uv;
        out vec2 texCoord;
        void main() {
            texCoord = uv;
            gl_Position = transform * vec4(position, 1.0);
        }
    }}
    fragment {{
        #version 310 es
        precision mediump float;
        layout(location = 1) uniform vec4 color;
        uniform bool test_alpha;
        uniform sampler2D cmap;
        in vec2 texCoord;
        out vec4 fragColor;
        void main() {
            vec4 texel = texture(cmap, texCoord);
            if( test_alpha && texel.a == 0.0 )
                discard;
            fragColor = color * texel;
        }
    }}
    default [
        color: 1.0,1.0,1.0
        test_alpha: false
        cmap: tex
    ]
]


box-sh: copy test-sh
box-sh/cmap: load-texture %data/image/color_numbers.png
box-dl: draw-list append [
    depth-test on
    blend off
    shader box-sh
] load %data/model/box_tex.b


font-dl: draw-list [
    blend off

    shader test-sh
    uniform test_alpha false
    image tex

    font fontA
    color 255,155,10
   ;color 255,255,255
    blend trans
    uniform test_alpha true
    text 10,300 "Hello World!"

    (zoom: view-cam/orbit/3)
    text 10,280 "Zoom: "
    text /*10,280*/ :zoom
]

demo-exec draw-list [
    clear

    camera view-cam
    call box-dl

    camera gui-cam
    call font-dl
]
