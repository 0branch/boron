tp: thread/port [
    while [true] [
        val: read port
        prin "read: " probe val
        if block? val [print do val]
        if eq? val 'bye [break]
    ]
    print "Thread exit"
]

send: func [val] [
    write tp val
    sleep 0.2
]

str: "Here is some text."

send 1
send true
send str
send str
send [1 2 3]
send [add 4 5]
send 'bye
