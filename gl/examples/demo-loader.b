/*
   Demo Loader

   Press 'r' to call recycle, ESC to quit, F9 to reload script, and F10 to
   take a snapshot.
*/

ifn args [
    print "Specify demo script to run"
    quit
]

display 640,480
clear-color 0.1,0.3,0.8

zoom: 1.0

demo-window: [
    script-widget [
        close: [quit]
        key-down: [
            esc [quit]
            r   [recycle]
            f9  [throw 'reload]
            f10 [
                save-png %snapshot.png display-snapshot
                print "Saved snapshot.png"
            ]
        ]
        mouse-move: [
            ;probe event
            ifn eq? 0 and event/5 mouse-lb [
                view-cam/turntable event/3 event/4
            ]
        ]
        mouse-wheel: [
            zoom: mul zoom pick [1.1 0.90909] gt? event 0
           ;print ["zoom:" zoom]
        ]
        resize: [view-cam/viewport: event]
    ]
]

view-cam: make camera [
    fov:   60.0
    near:   1.0
    far: 9000.0
    viewport: display-area ;20,20,200,200
    orient: make-matrix 0.0, 0.0, 15.0

    azimuth: to-radians 90.0
    elev:    0.0
    dist:   15.0
    focal-pnt: 0.0, 0.0, 0.0

    turntable: func [dx dy | ced] [
        azimuth: add azimuth to-radians mul dx  0.5
        elev: limit add elev to-radians mul dy -0.5  -1.53938 1.53938

       ;print ["azim-elev:" to-degrees azimuth to-degrees elev]

        ced: mul cos elev dist
        orient/13: add focal-pnt/1 mul ced  cos azimuth
        orient/14: add focal-pnt/2 mul dist sin elev
        orient/15: add focal-pnt/3 mul ced  sin azimuth

        look-at orient focal-pnt
    ]
    turntable 0 0
]

demo-exec: func [dl /update | window] [
    window: make widget! demo-window
    forever pick [[
        rclock-delta: to-decimal sub tmp: now rclock
        rclock: tmp

        draw dl
        display-swap
        handle-events window
        sleep 0.002
    ][
        draw dl
        display-swap
        handle-events/wait window
    ]] update
]

rclock-delta: 0.0
rclock: now

forever [
    ex: catch load first args
    ifn eq? 'reload ex [
        throw ex
    ]
    print "Reload"
]
