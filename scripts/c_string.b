; Convert Boron data to C string.

replace-quotes: func [str] [
    replace/all copy str '"' {\"}
]

forall args [
    str: to-string load first args
    str: copy trim/indent slice str 1,-1    ; Remove first & last bracket.
    append str '^/'
    parse str [some[
        tok: to '^/' :tok skip (
            ifn empty? tok [prin {  "} prin replace-quotes tok print {\n"}]
        )
    ]]
]
