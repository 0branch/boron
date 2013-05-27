if none [
    probe 'none-is-true
]
probe either none ['none-is-true] ['none-is-false]
probe empty? none
