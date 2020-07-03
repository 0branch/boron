; Boron-GL Item View Test


display/title 640,480 "Item View Test"
clear-color 128,128,128
ortho-cam: copy ortho-cam       ; Copy from shared storage.

; gui-style: do %data/style/oxif.b

;/*
load-img0: :load-image
load-image: func [file] [
    if find file %oxif-comp.png [
        return blit load-img0 file
                    load-img0 %data/image/color_numbers.png
                    0,128,256,128
    ]
    load-img0 file
]
gui-style: do %data/style/oxif.b
load-image: :load-img0
;*/


/*
s1: copy gui-style/widget-sh
s1/cmap: load-image %data/image/color_numbers.png
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
    ship   160  "Arjura"           "Science vessel deployed to Taurus^/sector."
    ship   160  "Tessel"           "Science vessel 2."
][
    append items make obj-proto [
        class: c
        name: n
        mass: m
        desc: d
   ]
]

; Item-view binds layout block to gui-style.
item-layout: [
    300,90 [           ; Item size: width,height vbox hbox grid
        image 8,50,32,32 tex-size (
            select [
                ship   0,320, 63,383
                base 128,320,191,383
            ] item/class
        )
        font list-font
        text 50,70 "Name:"  text 110,70 item/name
        text 50,50 "Mass:"  text 110,50 item/mass
        color 31    ; gray
        text  8,30 item/desc
    ][
        ; Selected background
        color 63
        image area tex-size 2,438,9,445 ; sm-box
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
            slider 0,4 2

        res-view: item-view items item-layout [probe res-view/value/name]
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
            button "OK" [
                print wd-text
                print s1/value
                print res-view/value/name
            ]
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
