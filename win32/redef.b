n: 1
parse read/text %boron.def [some [
    tok: thru '@' :tok thru '^/' (prin tok print ++ n)
]]
