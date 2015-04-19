print "---- file read"
f: %data-104
buf: "init^/"
probe read/append f buf
probe read/into f buf
probe read/append/part f buf 4
