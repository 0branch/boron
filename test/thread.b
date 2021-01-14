print "---- basic"
tp: thread/port [
    while [true] [
        val: read thread-port
        prin "read: " probe val
        if block? val [print do val]
        if eq? val 'bye [break]
    ]
    print "Thread exit"
]

send: func [val] [
    write tp val
    sleep 0.1
]

str: "Here is some text."

send 1
send true
send str
send str        ; Will be cleared by previous send.
send [1 2 3]
send [add 4 5]
send 'bye


print "---- join on close"
tp: thread/port {{
    while [string? val: read thread-port] [
        print join "Echo " val
    ]
    print "Thread auto-exit"
}}
send "apple"
send "ball"
close tp
