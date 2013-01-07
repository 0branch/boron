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

controls: context [
    con: none
    value:
    vmin: vmax: vinc: 0.0

    set 'demo-controls func [blk] [con: blk link]

    prev-con: does [if con [con: skip/wrap con -3 link]]
    next-con: does [if con [con: skip/wrap con  3 link]]
    inc: dec: min: max: none

    link: does [
        value: do third con
        tmp: second con
        set [inc dec min max] either block? tmp [
            ifn vinc: find tmp value [
                vinc: tmp
            ]
            block-funcs
        ][
            vmin: first  tmp
            vmax: second tmp
            vinc: third  tmp
            number-funcs
        ]
        print [first con value]
    ]

    assign: does [
        set third con value
        print [first con value]
    ]

    block-adj: func [cmd] [
        if con [
            value: first vinc: do cmd
            assign
        ]
    ]
    block-funcs: reduce [
        does [block-adj [skip/wrap vinc  1]]
        does [block-adj [skip/wrap vinc -1]]
        does [block-adj [head second con]]
        does [block-adj [prev tail second con]]
    ]

    number-adj: func [inc] [
        value: limit add value inc vmin vmax
        assign
    ]
    number-funcs: reduce [
        does [if con [number-adj vinc]]
        does [if con [number-adj negate vinc]]
        does [if con [value: vmin assign]]
        does [if con [value: vmax assign]]
    ]

    report: does [
        if con [
            print "Controls:"
            foreach [name range path] head con [
                print ["  " name do path]
            ]
        ]
    ]

    keys: [
        left  [dec]
        right [inc]
        up    [prev-con]
        down  [next-con]
        home  [min]
        end   [max]
        c     [report]
    ]
]

demo-window: [
    root [
        close: [quit]
        key-down: append copy controls/keys [
            esc  [quit]
            r    [recycle]
            f9   [throw 'reload]
            f10  [
                save-png %snapshot.png display-snapshot
                print "Saved snapshot.png"
            ]
        ]
        mouse-move: [
            ;probe event
            ifn eq? 0 and event/5 mouse-lb [
                turntable view-cam mul 0.5,-0.5 slice event 2,2
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
    viewport: display-area
    orient: make-matrix 0.0, 0.0, 15.0

    orbit: 1.570796, 0.0, 15.0
    focal-pnt: 0.0, 0.0, 0.0

    turntable self 0,0
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
