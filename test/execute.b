cmd-args: ""
c: to-int 'a'
n: 1
loop 20 [
    append cmd-args rejoin [" -" to-char ++ c ++ n]
]

out: ""
err: ""
execute/out/err join "../boron execute_child " cmd-args out err
probe out
probe err
