; Boron-GL Data


camera: context [
    orient:   none          ; matrix
    viewport: 0,0,640,480   ; x,y,width,height
    fov:  65.0
    clip: 0.1,10.0
]

ortho-cam: make camera [
    fov: 'pixels            ; 'pixels to match viewport or left,right vec3!
    clip: -10.0,10.0
]

orbit-cam: make camera [
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
    menu-margin:
    menu-bg:
    menu-item-selected:
    label-dl:
    editor:
    editor-active:
    editor-cursor:
    list-header:
    list-item:
    list-item-selected:
    slider-size:
    slider:
    slider-groove:
    scroll-size:
    scroll-bar:
    scroll-knob:
        none
]


animation: context [
    value: none
    curve: none
    scale: 1.0
    time:  0.0
    behavior: 1
]

ease-in: [
    0.00 0.0
    0.15 0.003375
    0.30 0.027000
    0.50 0.125000
    0.70 0.343000
    0.85 0.614125
    1.00 1.0
]

ease-out: [
    0.00 0.0
    0.15 0.385875
    0.30 0.657000
    0.50 0.875000
    0.70 0.973000
    0.85 0.996625
    1.00 1.0
]

ease-in-out: [
    0.00 0.0
    0.10 0.004
    0.20 0.032
    0.30 0.108
    0.40 0.256
    0.50 0.500
    0.60 0.744
    0.70 0.892
    0.80 0.968
    0.90 0.996
    1.00 1.0
]


/*-hf- recal-curve
        curve   block!
        a       decimal!/coord!/vec3!
        b       decimal!/coord!/vec3!
   return: Recalibrated copy of curve.

   A and B must be of the same type.
*/
recal-curve: func [orig a b] [
    nc: copy orig
    forall nc [
        poke nc 2 lerp a b second nc
        ++ nc
    ]
    head nc
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
        file    PNG filename
        /mipmap
        /clamp
    return: texture!
    group: io
*/
load-texture: func [file /mipmap /clamp | spec] [
    spec: file: load-png file
    if mipmap [spec: [file 'mipmap]]
    if clamp  [spec: [file 'linear 'clamp]]
    make texture! spec
]


;-----------------------------------------------------------------------------


audio-sample: context [
    sample-format:
    rate:
    data: none
]

/*-hf- load-wav
        file
    return: Audio-sample context.
    group: audio, io
*/
load-wav: func [file /local sdata] [
    parse/binary read file [
        little-endian
        "RIFF" u32 "WAVEfmt "
        csize:    u32
        format:   u16
        channels: u16
        srate:    u32 u32 u16
        bps:      u16
        thru "data"
        csize:    u32
        copy sdata csize
    ]
    case [
        none? bps    [error "WAVE header is malformed"]
        none? sdata  [error "WAVE data not found"]
        ne? format 1 [error "WAVE data is compressed"]
    ]

    format: make coord! [bps channels 0]
    make audio-sample copy [
        sample-format: format
        rate: srate
        data: sdata
    ]
]


;-----------------------------------------------------------------------------
; Math Helpers


/*
axis-x: 1.0, 0.0, 0.0
axis-y: 0.0, 1.0, 0.0
axis-z: 0.0, 0.0, 1.0
*/

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
