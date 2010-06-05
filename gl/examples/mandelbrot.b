; Mandelbrot Set

/*
do load %scripts/timer_bar.t
matte-sh: load-shader %data/shader/matte.b
set 'sim-update :update-timer-bar
*/


do load %data/script/gradiant_tex.b

mandel-sh: make shader! [
    vertex {
void main() {
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = ftransform();
}
    }

    fragment {
uniform sampler1D cmap;
uniform vec2 center;
uniform float scale;
uniform float iter;

void main() {
    vec2 z, c;
    float i;

    c.x = 1.3333 * (gl_TexCoord[0].x - 0.5) * scale - center.x;
    c.y =          (gl_TexCoord[0].y - 0.5) * scale - center.y;

    z = c;
    for( i = 0.0; i < iter; ++i ) {
        z = vec2(z.x * z.x - z.y * z.y,
                 z.y * z.x + z.x * z.y) + c;
        if( dot(z, z) > 4.0 )
            break;
    }

    gl_FragColor = texture1D( cmap, (i == iter) ? 0.0 : i / iter );
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
