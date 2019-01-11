print "---- int! & string!"
foreach [item n name][
    cookies 30 "Joe Smith"
    cake 102 "Sally M. Longshanks"
    tea 5 "Fred"
][
    print format ["  | " 11 6 12 " |"] [item n name]
]

print "---- pad"
foreach [file h m s][
    %some/file 1 22 3
    %some/other/file 0 4 45
][
    print format [pad '.' 20 pad 0 -2 ':' -2 ':' -2] [file h m s]
]

print "---- coord! & non-block values"
foreach [v][
    'a' 0xfedc 'more-than-four
][
    print format [" value(" 2,4 ");"] v
]
