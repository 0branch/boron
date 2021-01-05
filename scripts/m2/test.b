#!/usr/bin/boron -s
; Test m2 on various project files.
; Must be run from the boron/scripts/m2/ directory.

odir: %/tmp/m2-test
mdir: current-dir
home: to-file terminate getenv "HOME" '/'

targets: [
    %m2_linux.b
    %m2_macx.b
    %m2_mingw.b
  ; %m2_sun.b     Not updated for Qt 5.
    %m2_visualc.b
]

projects: reduce [
    join home %src/primal
    join home %src/boron
    join home %src/bgl
]

make-dir odir
foreach proj projects [
    change-dir proj
    projbase: next find/last proj '/'
    foreach t targets [
        mf: construct to-string t ["m2" projbase ".b" none]
        execute rejoin [
            {boron -s } mdir {m2 -t } mdir t { -o } odir '/' mf
        ]
    ]
]
