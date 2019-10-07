; Mandelbrot Set

/*
do load %scripts/timer_bar.t
matte-sh: load-shader %data/shader/matte.b
set 'sim-update :update-timer-bar
*/


do load %data/script/gradiant_tex.b

mandel-sh: make shader! [
    vertex {{
        #version 300 es
        uniform mat4 matrix;

        layout(location = 0) in vec3 position;
        layout(location = 1) in vec2 uv;
        out vec2 texCoord;

        void main() {
            texCoord = uv;
            gl_Position = matrix * vec4(position, 1.0);
        }
    }}

    fragment {
#version 300 es
precision mediump float;

uniform sampler2D cmap;
uniform vec2 center;
uniform float scale;
uniform float iter;

in vec2 texCoord;

out vec4 fragColor;

void main() {
    vec2 z, c;
    float i;

    c.x = 1.3333 * (texCoord.x - 0.5) * scale - center.x;
    c.y =          (texCoord.y - 0.5) * scale - center.y;

    z = c;
    for( i = 0.0; i < iter; ++i ) {
        z = vec2(z.x * z.x - z.y * z.y,
                 z.y * z.x + z.x * z.y) + c;
        if( dot(z, z) > 4.0 )
            break;
    }

    fragColor = texture( cmap, vec2((i == iter) ? 0.0 : i / iter, 0.0) );
}
    }

    default [
        center: 0.5, 0.6
        scale: 0.2
        iter: 150.0
        cmap: gradient-texture 64 [
            0.0    0,  0,  0
            0.2  255,128,  0
            0.4  255,255,255
            0.5    0,255,255
            1.0   40,128,  0
        ]
    ]
]

cube-dl: draw-list load %data/model/box_tex.b

demo-exec draw-list [
    clear

    camera view-cam
    scale :zoom
    cull on
    shader mandel-sh
    call cube-dl

    ;camera ortho-cam
    ;shader matte-sh
    ;call timer-bar-dl
]

;eof
