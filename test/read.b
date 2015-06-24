print "---- file read"
f: %data-104
buf: "init^/"
probe read/append f buf
probe read/into f buf
probe read/append/part f buf 4

print "---- eof returns none"
fp: open f
str: ""
len: []
while [read/part/into fp 20 str] [
    probe str
    append len size? str
]
probe len
close fp
