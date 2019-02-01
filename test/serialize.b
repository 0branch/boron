
input: [
    none!/int!/double! 0 -1 2147483647 -2147483648 0xffff0011
    0.0 1.0 -654321.123456 4:59:20.56
    -99.0,0.0,1028.9432 32767,-32768 -1,2,-3,4,-5,6
    ; 01:02:03:45D
    some words set: words:'lit 'words :get :words 
    [1 (2 "two") [3 "three" words]]
    "Tasty treats." #{00112233AABBCCDD}
    #[65495810 -3243 0 1 -1] #[67434.403 -1.0 0.0]  ; .403 -> .40625

    ctx1 ctx2 iter slic bset    ; Bound to values
]

values: context [
    ctxP: context [name: speed: child: none]
    ctx1: make ctxP [name: "Bob" speed: 2]
    ctx2: make ctxP [name: "Tom" speed: 1 child: ctx1]
    iter: next next "**text**"
    slic: slice iter -2
    bset: make bitset! "^-^/ ()[]{}"
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
