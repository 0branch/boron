done: 0
exec widget [
    vbox [
        label "Progress Demo"
        prog-bar: progress 10
        spacer
        hbox [
            spacer
            button "Increment"   [
                set-widget-value prog-bar either equal? done 10 [0] [++ done]
            ]
            button "Quit" [quit]
        ]
    ]
]
