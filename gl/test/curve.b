; Curve

c1: [
    0.0    0,  0,  0
    0.2  255,128,  0
    0.4  255,255,255
    0.5    0,255,255
    1.0   40,128,  0
]

times: [-0.1 0.0 0.1 0.8 1.0 1.1]

foreach t times [
    probe curve-at c1 t
]

foreach t times [
    probe lerp 10.0 50.0 t
]
