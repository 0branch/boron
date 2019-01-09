environs: make context! [
  version: 1,99,1
  os: arch: big-endian: none
]

q: :quit  yes: true  no: false
eq?: :equal?
tail?: :empty?
close: :free
context: func [b block!] [make context! b]
charset: func [s char!/string!] [make bitset! s]
error: func [s string! /ghost] [throw make error! s]

join: func [a b] [
  a: either series? a [copy a][to-text a]
  append a reduce b
]

rejoin: func [b block!] [
  if empty? b: reduce b [return b]
  append either series? first b
    [copy first b]
    [to-text first b]
  next b
]

replace: func [series pat rep /all | f size] [
  size: either series? pat [size? pat][1]
  either all [
    f: series
    while [f: find f pat] [f: change/part f rep size]
  ][
    if f: find series pat [change/part f rep size]
  ]
  series
]

split-path: func [path | end] [
  either end: find/last path '/'
    [++ end reduce [slice path end  end]]
    [reduce [none path]]
]

term-dir: func [path] [terminate/dir path '/']

funct: func [spec body] [
  ifn find spec '| [append spec '|]
  func collect/unique/into set-word! body spec body
]
