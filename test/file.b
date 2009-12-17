print "---- make file"
probe make file! "Some File"
probe make file! 'my-file
probe f: %filename-v12.ext
print f
probe f: %"file before block"[]
print f


print "---- to-file"
probe to-file "Some File"
probe to-file 'my-file
probe join to-file "../" %src


print "---- special characters"
probe %"$(QTDIR)/lib"


print "---- file as series"
f: %my-file.ext
probe next f
probe find f "file"
probe find f %file
probe append %my-file- 23
probe rejoin [%file- 10 %.ext]
