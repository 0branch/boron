;p: load-png %/home/karl/src/primal/gui/icons/logo.png
;save-png %out p

;print gl-extensions
;ortho-cam: copy ortho-cam
clear-color 0.1,0.3,0.8


 fontA: make font! [%data/font/20thfont.ttf 22]
 tex: fontA/texture
;tex: load-texture %/home/karl/src/primal/gui/icons/logo.png


image-sh: make shader! [
    vertex {
void main() {
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = ftransform();
}
}
    fragment {
uniform sampler2D cmap;
void main() {
    gl_FragColor = texture2D(cmap, gl_TexCoord[0].st);
}
}
]

text-sh: make shader! [
    vertex {
void main() {
    gl_FrontColor = gl_Color;
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = ftransform();
}
}
    fragment {
uniform sampler2D cmap;
void main() {
    vec4 texel = texture2D(cmap, gl_TexCoord[0].st);
    if( texel.a == 0.0 )
        discard;
    gl_FragColor = gl_Color * texel;
}
}
]


image-sh/cmap: tex
text-sh/cmap: tex


box-sh: copy image-sh
box-sh/cmap: load-texture %data/image/color_numbers.png
box-dl: draw-list append [
    scale :zoom
    depth-test on
    blend off
    shader box-sh
] load %data/model/box_tex.b


font-dl: draw-list [
    blend off

    shader image-sh
    image tex

    font fontA
    color 255,155,10
   ;color 255,255,255
    blend trans
    shader text-sh
    text 10,300 "Hello World!"
]

main-dl: draw-list [
    clear

    camera view-cam
    call box-dl

    camera ortho-cam
    call font-dl
]


window: make widget! demo-window
;js: open [joystick]

while [true] [
    ;draw window
    draw main-dl
    display-swap

    ;while [ev: read js] [probe ev]
    ;sleep 0.3
    handle-events/wait window
]
