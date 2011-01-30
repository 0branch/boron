; Boron-GL Data


camera: context [
    orient:   none          ; matrix
    position: none
    viewport: 0,0,640,480   ; x,y,width,height
    fov:  65.0
    near:  0.1
    far:  10.0
]

ortho-cam: make camera [
    fov: 'ortho
    near: -100.0
    far:   100.0
]


; Must match order of ContextIndexStyle in gui.h
gui-style-proto: context [
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
    start-dl:
    window-margin:
    window:
    button-size:
    button-up:
    button-down:
    button-hover:
    checkbox-size:
    checkbox-up:
    checkbox-down:
    label-dl:
    editor:
    editor-active:
    editor-cursor:
    list-header:
    list-item:
    list-item-selected:
        none
]


animation: context [
    value: none
    curve: none
    period: 1.0
    time:   0.0
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

recal-curve: func [orig a b | nc] [
    nc: copy orig
    forall nc [
        poke nc 2 lerp a b second nc
        ++ nc
    ]
    head nc
]


draw-list: func [blk] [make draw-prog! blk]


;-----------------------------------------------------------------------------
; Input


switch environs/os [
    linux [mouse-lb: 0x100 mouse-mb: 0x200 mouse-rb: 0x400]
          [mouse-lb: 1     mouse-mb: 3     mouse-rb: 2]
]


;-----------------------------------------------------------------------------
; gx_init() calls layout to install the user layout rules.


/*
layout: func [custom] [
    layout: custom

    [spec | tok firstw assign parent stack make-widget box widget rules]
    [
        [] clear :stack

        [
            widget! parent tok make
                assign ift (dup assign set none :assign)
                firstw iff (dup :firstw)
        ]
        proc :make-widget   ; ( -- widget)

        [tok: (stack tok last append make-widget push)] :box
        [tok: (make-widget drop)] :widget

        spec
        [
            [
                :tok
                'hbox      block!                 box
              | 'vbox      block!                 box
              | 'window    string! block! block!  box
              | 'expand                           widget
              | 'button    string! block!         widget
              | 'checkbox  string! block!         widget
              | 'label     string!                widget
              | 'line-edit word!/string!          widget
              | 'list      block!  block!         widget
              | 't-widget  block!                 widget
              | get-word!  (tok/1 word! to :assign)
              ; >> Custom widget parse rules go here <<
            ]
            parse.some

            stack empty? ift break
            stack pop :parent
            stack pop
        ] forever

        firstw
    ]

    ; Append user rules.
    layout if-some (over tail -3 skip first swap append.cat drop)

    ; Make the function with the new rules in place and replace this installer.
    func :layout    ; (block -- widget)
]
*/


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


load-shader: func [file] [make shader! load file]

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

; Returns audio-sample context.
load-wav: func [file | csize format channels srate bps sdata] [
    parse read file [
        little-endian
        "RIFF" u32 "WAVEfmt "
        csize:    u32
        format:   u16
        channels: u16
        srate:    u32 u32 u16
        bps:      u16
        "data"
        csize:    u32
        copy sdata csize
    ]
    if ne? format 1 [
        error "WAV data is compressed"
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

make-matrix: func [pos] [poke copy unit-matrix 13 pos]


;eof
