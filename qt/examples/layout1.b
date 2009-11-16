/*
    A simple layout example.
*/

print exec widget [
    vbox [
        grid 3 [
            label "Input Folder" if: line-edit button "..." [print "A"]
            label "Config File"  cf: line-edit button "..." [print "B"]
            label "Log File"     lf: line-edit button "..." [print "C"]
        ]
        spacer
        hbox [
            spacer
            button "OK"     [close yes]
            button "Cancel" [close no ]
        ]
    ]
]
