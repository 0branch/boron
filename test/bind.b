pref: context [
    size: 0,0
    path: none
    auto-save: false
]

print "---- bind/secure"
foreach blk [
    [size: 100,200 path: %/tmp/save auto-save: true]
    [size: quit]
    [new-word: 0]
][
    probe v: try [
        do bind/secure blk pref
    ]
    ifn error? v [probe pref]
]
