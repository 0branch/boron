m: make hash-map! [hi [willy wonka] 5 "five" 0:10:0 some-time "key" value]

print "---- print"
probe m
print m

print "---- path"
probe m/hi
probe m/5
;probe m/0:10:0     ; Not a valid path.

print "---- pick"
probe pick m 0:10:0
probe pick m "five"
probe pick m "key"

print "---- poke"
poke m "new-key" 77
poke m "key"     88
print [pick m "new-key" pick m "key"]

print "---- remove"
remove/key m "new-key"
remove/key m "key"
print [pick m "new-key" pick m "key"]
probe values-of m

print "---- similar keys"
s: make hash-map! [
    "game"  0
    "GAME"  1
    %game   2
    game    3
    'game   4
]
print [
    pick s "game"
    pick s "GAME"
    pick s %game
    pick s 'game
    pick s to-lit-word 'game
    pick s to-get-word 'game
]
