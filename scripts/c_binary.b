#!/usr/bin/boron

contents-only: ser: false
parse args [some[
    "-c" contents-only:
  | "-s" ser:
  | set file skip
]]

bin: either ser [serialize load file] [read file]

ifn contents-only [
    parse file [some ['/' file: | '.' -1 skip :file break | skip]]
    prin rejoin [
        "#define " file "_len^-" size? bin
        "^/static const unsigned char " file "_data[] = {"
    ]
]

n: 0
foreach c bin [
    if zero? and 7 ++ n [prin "^/  "]
    prin to-hex c
    prin ','
]

prin either contents-only ['^/']["^/};^/"]
