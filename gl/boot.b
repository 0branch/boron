; Boron-GL Data


camera: context [
    orient:   none          ; matrix
    viewport: 0,0,640,480   ; x,y,width,height
    fov:  65.0
    clip: 0.1,10.0
]

pixmap-camera: make camera [
    fov: 'pixels            ; 'pixels to match viewport or left,right vec3!
    clip: -10.0,10.0
]

orbit-camera: make camera [
    orbit:     0.0, 0.0, 10.0   ; azimuth, elevation, dist (angles in radians)
    focal-pnt: 0.0, 0.0, 0.0
]


; Must match order of ContextIndexStyle in gui.h
gui-style-proto: context [
    widget-sh:
    texture:
    tex-size:
    control-font:
    title-font:
    edit-font:
    list-font:

    ; Display list variables
    label:
    area:

    ; Display lists & sizes
    start-dl:           ; Compiled by user.
    window-margin:
    window:
    button-size:
    button-up:
    button-down:
    button-hover:
    checkbox-size:
    checkbox-up:
    checkbox-down:
    choice-size:
    choice:
    choice-item:
    cmenu-bg:
    cmenu-item-selected:
    menu-margin:
    menu-bg:
    menu-item-selected:
    label-dl:
    editor:
    editor-active:
    editor-cursor:
    list-bg:
    list-header:
    list-item:
    list-item-selected:
    slider-size:
    slider:
    vslider:
    slider-groove:
    vslider-groove:
    scroll-size:
    scroll-bar:
    vscroll-bar:
    scroll-knob:
    vscroll-knob:
        none
]


/*-hf- draw-list
        blk
   return: draw-prog!
   Same as make draw-prog! blk.
*/
draw-list: func [blk] [make draw-prog! blk]


;-----------------------------------------------------------------------------
; Input


switch environs/os [
    linux [mouse-lb: 0x100 mouse-mb: 0x200 mouse-rb: 0x400]
          [mouse-lb: 1     mouse-mb: 3     mouse-rb: 2]
]


;-----------------------------------------------------------------------------
; Load functions


/*
; Override 'load
load: func [file string!] [
    read file 16
    dup "RIFF" match ift (file load-wav return)

    read file
    dup "#!" match ift (eol find)
    to-block kernel-ops infuse
]
*/


/*-hf- load-shader
        file
    return: shader!
    group: io
*/
load-shader: func [file] [make shader! load file]

/*-hf- load-texture
        file    PNG or JPEG filename
        /mipmap
        /clamp
    return: texture!
    group: io
*/
load-texture: func [file /mipmap /clamp | spec] [
    spec: file: load-image file
    if mipmap [spec: [file 'mipmap]]
    if clamp  [spec: [file 'linear 'clamp]]
    make texture! spec
]


;-----------------------------------------------------------------------------


/*-hf- load-wav
        file
    return: Audio-sample context.
    group: audio, io
*/
load-wav: func [file] [
    parse read file [
        "RIFF" 4 skip "WAVEfmt "
        bits [
            u32
            format:   u16
            channels: u16
            srate:    u32 u32 u16
            bps:      u16
        ]
        thru "data"
        bits [csize: u32]
        sdata:
    ]
    case [
        none? bps    [error "WAVE header is malformed"]
        none? sdata  [error "WAVE data not found"]
        ne? format 1 [error "WAVE data is compressed"]
    ]

    format: make coord! [bps channels 0]
    ; Matches AudioSampleContext in audio.c.
    context copy [
        sample-format: format
        rate: srate
        data: copy slice sdata csize
    ]
]


;-----------------------------------------------------------------------------
; Scene

scene-proto: context [
    enter:
    leave:
    update:
    gl-setup:
        none
]

scene: func [spec] [
    make scene-proto spec
]


;-----------------------------------------------------------------------------
; Math Helpers


hermite-ease-in:  #[2.28 0.0   0.1  0.1  0.0 0.0  1.0 1.0]
hermite-ease-out: #[0.78 3.24  0.1  0.1  0.0 0.0  1.0 1.0]
hermite-ease:     #[1.4  0.0   1.19 0.0  0.0 0.0  1.0 1.0]

vone:   1.0, 1.0, 1.0
vzero:  0.0, 0.0, 0.0
axis-x: 1.0, 0.0, 0.0
axis-y: 0.0, 1.0, 0.0
axis-z: 0.0, 0.0, 1.0

unit-matrix:
#[1.0 0 0 0
  0 1.0 0 0
  0 0 1.0 0
  0 0 0 1.0]

/*-hf- make-matrix
        pos     vec3!
    return: vector!
    Create matrix with position initialized.
*/
make-matrix: func [pos] [poke copy unit-matrix 13 pos]


;eof
