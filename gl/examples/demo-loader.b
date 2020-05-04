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

controls: context [
    con: none
    value:
    vmin: vmax: vinc: 0.0

    set 'demo-controls func [blk /extern con] [con: blk link]

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

    block-adj: func [cmd /extern value vinc] [
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

    number-adj: func [inc /extern value] [
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

_window-spec:
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
            view-cam/zoom pick [0.90909 1.1] gt? event 0
        ]
        resize: [view-cam/viewport: gui-cam/viewport: event]
    ]
]

view-cam: make orbit-camera [
    fov:   60.0
    clip:  1.0, 9000.0
    viewport: display-area
    orient: make-matrix 0.0, 0.0, 15.0

    orbit: 1.570796, 0.0, 15.0
    focal-pnt: 0.0, 0.0, 0.0

    turntable self 0,0

    zoom: func [scale /extern orbit] [
        orbit: poke orbit 3 mul third orbit scale
        turntable self 0,0
    ]
]

gui-style: none

demo-widgets: func [spec /extern gui-style _window-spec] [
    ; Load gui-style only once but always apply any widget changes.
    ifn gui-style [
        gui-style: do %data/style/oxif/gui.b
        gui-style/gl-setup
    ]
    _window-spec: append copy demo-window spec
]

demo-exec: func [dl /update /extern rclock-delta rclock] [
    window: make widget! _window-spec
    resize window display-area
    gui-dl: either gui-style window none

    forever pick [[
        rclock-delta: to-double sub tmp: now rclock
        rclock: tmp

        draw dl
        draw gui-dl
        display-swap
        handle-events window
        sleep 0.002
    ][
        draw dl
        draw gui-dl
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
