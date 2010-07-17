
input: [
    none!/int!/decimal! 0 -1 2147483647 -2147483648
    some words
    [1 (2 "two") [3 "three" words]]
    "Hello" #{00112233AABBCCDD}
    ctx1 ctx2 iter slic
]

values: context [
    ctxP: context [name: speed: child: none]
    ctx1: make ctxP [name: "Bob" speed: 2]
    ctx2: make ctxP [name: "Tom" speed: 1 child: ctx1]
    iter: next next "**text**"
    slic: slice iter -2
]
bind input values
append input reduce skip tail input -4


bin: serialize input
probe size? bin
probe bin

out: unserialize bin

input-str: mold input
out-str: mold out
if eq? input-str out-str [
    print size? input-str
    print input-str
    print out-str
]

;print size? compress bin
;print size? compress out-str
