; Boron-GL Item View Test


display/title 640,480 "Item View Test"
clear-color 128,128,128
ortho-cam: copy ortho-cam       ; Copy from shared storage.

; gui-style: do %data/style/oxif.b

;/*
load-png0: :load-png
load-png: func [file] [
    if find file %oxif.png [
        return blit load-png0 file
                    load-png0 %data/image/color_numbers.png
                    0,128,256,128
    ]
    load-png0 file
]
gui-style: do %data/style/oxif.b
load-png: :load-png0
;*/


/*
s1: copy gui-style/widget-sh
s1/cmap: load-png %data/image/color_numbers.png
ortho-cam/viewport: 0,0,512,512
f1: make fbo! gui-style/texture
draw draw-list [
    camera ortho-cam
    shader s1
    framebuffer f1
    image 0,0
    framebuffer 0
]
s1: f1: none
*/


obj-proto: context [
    class:
    name:
    desc:
    mass:
        none
]

items: []
foreach [c m n d] [
    ship   200  "Voyager"          "A refit was made in 35088."
    base  1040  "Central Command"  "The base of Janus sector.^/Ready to serve."
    ship   160  "Arjura"           "Science vessel deployed^/to Taurus sector."
][
    append items make obj-proto [
        class: c
        name: n
        mass: m
        desc: d
   ]
]

item-layout: [
    300,90 [           ; Item size: width,height vbox hbox grid
        image 0,30,32,32 gui-style/tex-size (
            select [
                ship   0,320, 63,383
                base 128,320,191,383
            ] item/class
        )
        font gui-style/list-font
        text 50,50 "Name:"  text 110,50 item/name
        text 50,30 "Mass:"  text 110,30 item/mass
        text  0,10 item/desc
    ]
]

wd-text: "Some Text"
s1: none

gui: make widget! [
    root [
        resize: [
          ; move/center win
                ortho-cam/viewport: event
        ]
        close:  [quit]
        key-down: [
            esc [quit]
        ]
    ]
    win: window "Some Widgets" none [
        label "Test Label"
        option: checkbox "An Option" [print option/value]

        hbox [
            label "Entry:" 
            line-edit wd-text 20
        ]

        s1: slider 1,100 50 [print s1/value]
            slider 1,100 25

        res-view: item-view items item-layout
        /*
            ; When using automatic layout, the first item at construction
            ; time is template for all item layout.
            vbox [
                hbox [
                    image (
                        select item/class [
                            ship 0,0,0,0
                            base 0,0,0,0
                        ]
                    )
                    grid 2,2 [
                        text "Name:" text item/name
                        text "Mass:" text item/mass
                    ]
                ]
                text item/desc
            ]
        */

        expand 16
        hbox [
            expand
            button "OK"     [print wd-text print s1/value print res-view/value]
            button "Cancel" [quit]
        ]
    ]
]


test-dl: draw-list [
    clear
    camera ortho-cam
    call :gui

    /*
    camera view-cam
    blend/off cull depth-mask depth-test
    box -1 1
    */
]


resize gui display-area
forever [
    draw test-dl
    display-swap
    handle-events/wait gui
]


;eof
