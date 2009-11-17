print "---- split-path"
probe split-path %path/to/file
probe split-path %some-file
probe split-path %C:/windows/file
;probe split-path %C:\windows\file


print "---- replace"
s: "comma,separated,words,"
probe replace     copy s ',' ' '
probe replace/all copy s ',' ' '
probe replace/all copy s ',' "-->"
