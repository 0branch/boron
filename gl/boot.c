  "camera: context [\n"
  "    orient: none\n"
  "    viewport: 0,0,640,480\n"
  "    fov: 65.0\n"
  "    clip: 0.100000001,10.0,0.0\n"
  "]\n"
  "pixmap-camera: make camera [\n"
  "    fov: 'pixels\n"
  "    clip: -10.0,10.0,0.0\n"
  "]\n"
  "orbit-camera: make camera [\n"
  "    orbit: 0.0,0.0,10.0\n"
  "    focal-pnt: 0.0,0.0,0.0\n"
  "]\n"
  "gui-style-proto: context [\n"
  "    widget-sh:\n"
  "    texture:\n"
  "    tex-size:\n"
  "    control-font:\n"
  "    title-font:\n"
  "    edit-font:\n"
  "    list-font:\n"
  "    label:\n"
  "    area:\n"
  "    start-dl:\n"
  "    window-margin:\n"
  "    window:\n"
  "    button-size:\n"
  "    button-up:\n"
  "    button-down:\n"
  "    button-hover:\n"
  "    checkbox-size:\n"
  "    checkbox-up:\n"
  "    checkbox-down:\n"
  "    choice-size:\n"
  "    choice:\n"
  "    choice-item:\n"
  "    menu-margin:\n"
  "    menu-bg:\n"
  "    menu-item-selected:\n"
  "    label-dl:\n"
  "    editor:\n"
  "    editor-active:\n"
  "    editor-cursor:\n"
  "    list-bg:\n"
  "    list-header:\n"
  "    list-item:\n"
  "    list-item-selected:\n"
  "    slider-size:\n"
  "    slider:\n"
  "    slider-groove:\n"
  "    scroll-size:\n"
  "    scroll-bar:\n"
  "    scroll-knob:\n"
  "    none\n"
  "]\n"
  "draw-list: func [blk] [make draw-prog! blk]\n"
  "switch environs/os [\n"
  "    Linux [mouse-lb: 0x100 mouse-mb: 0x200 mouse-rb: 0x400]\n"
  "    [mouse-lb: 1 mouse-mb: 3 mouse-rb: 2]\n"
  "]\n"
  "load-shader: func [file] [make shader! load file]\n"
  "load-texture: func [file /mipmap /clamp | spec] [\n"
  "    spec: file: load-image file\n"
  "    if mipmap [spec: [file 'mipmap]]\n"
  "    if clamp [spec: [file 'linear 'clamp]]\n"
  "    make texture! spec\n"
  "]\n"
  "audio-sample: context [\n"
  "    sample-format:\n"
  "    rate:\n"
  "    data: none\n"
  "]\n"
  "load-wav: func [file /local sdata] [\n"
  "    parse/binary read file [\n"
  "        little-endian\n"
  "        \"RIFF\" u32 \"WAVEfmt \"\n"
  "        csize: u32\n"
  "        format: u16\n"
  "        channels: u16\n"
  "        srate: u32 u32 u16\n"
  "        bps: u16\n"
  "        thru \"data\"\n"
  "        csize: u32\n"
  "        copy sdata csize\n"
  "    ]\n"
  "    case [\n"
  "        none? bps [error \"WAVE header is malformed\"]\n"
  "        none? sdata [error \"WAVE data not found\"]\n"
  "        ne? format 1 [error \"WAVE data is compressed\"]\n"
  "    ]\n"
  "    format: make coord! [bps channels 0]\n"
  "    make audio-sample copy [\n"
  "        sample-format: format\n"
  "        rate: srate\n"
  "        data: sdata\n"
  "    ]\n"
  "]\n"
  "scene-proto: context [\n"
  "    enter:\n"
  "    leave:\n"
  "    update:\n"
  "    gl-setup:\n"
  "    none\n"
  "]\n"
  "scene: func [spec] [\n"
  "    make scene-proto spec\n"
  "]\n"
  "hermite-ease-in: #[2.28 0.0 0.1 0.1 0.0 0.0 1.0 1.0]\n"
  "hermite-ease-out: #[0.78 3.24 0.1 0.1 0.0 0.0 1.0 1.0]\n"
  "hermite-ease: #[1.4 0.0 1.19 0.0 0.0 0.0 1.0 1.0]\n"
  "vone: 1.0,1.0,1.0\n"
  "vzero: 0.0,0.0,0.0\n"
  "axis-x: 1.0,0.0,0.0\n"
  "axis-y: 0.0,1.0,0.0\n"
  "axis-z: 0.0,0.0,1.0\n"
  "unit-matrix:\n"
  "#[1.0 0.0 0.0 0.0 0.0 1.0 0.0 0.0 0.0 0.0 1.0 0.0 0.0 0.0 0.0 1.0]\n"
  "make-matrix: func [pos] [poke copy unit-matrix 13 pos]\n"
