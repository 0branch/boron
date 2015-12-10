---
title:  Boron-GL User Manual
date:   Version 0.2.4, 2014-01-05
---


Overview
========

Boron-GL extends the Boron scripting language with datatypes and functions
for working with OpenGL 2.1.

Features include:

  * Garbage collected GL datatypes.
  * Texture-based fonts loaded from TrueType files.
  * Loading PNG images.
  * Plays audio from WAV and OGG formats.


About This Document
-------------------

This manual is largely incomplete.

There is a separate [function reference] available online at
<http://urlan.sourceforge.net/boron>.


Running Scripts
===============


Shell Invocation
----------------

The first line of a script may be a UNIX shell sha-bang (``#!``) command.

    #!/usr/bin/boron-gl


Command Line Usage
------------------

Usage:

    boron-gl [options] [script] [arguments]


### Command Line Options

----------  ------------------------
-a          Disable audio
-e "*exp*"  Evaluate expression
-h          Show help and exit
-p          Disable prompt and exit on exception
-s          Disable security
----------  ------------------------


### Command Line Arguments

If the interpreter is invoked with a script then the *args* word will be set
to either a block of strings, or *none* if no script arguments were given.

So this command:

    boron-gl -e "probe args" file1 -p 2

Will print this:

    ["file1" "-p" "2"]



GL Datatypes
============

Type Name                 Description
------------------------  -----------------------------
[draw-prog!](#draw-prog)  Compiled draw program
[raster!](#raster)        Pixel image in main memory
[texture!](#texture)      OpenGL texture
[font!](#font)            Texture-based font
[shader!](#shader)        OpenGL shader
[fbo!](#fbo)              OpenGL frame buffer object
[vbo!](#vbo)              OpenGL vertex buffer object
[quat!](#quat)            Quaternion
[widget!](#widget)        Widget
------------------------  -----------------------------


Draw-Prog!
----------

A draw program is a list of display operations compiled to byte-code.
It is reminiscent of the GL display list (now deprecated in OpenGL 3.0).

**NOTE**: The terms *draw program* and *draw list* are used interchangeably.


Raster!
-------


Texture!
--------

The simplest way to make textures is to use the *load-texture* function.

    load-texture %skin.png

Full specification:

    make texture! [
        raster!/coord!/int!
        binary!
        'mipmap 'nearest 'linear 'repeat 'clamp
        'gray 'rgb' 'rgba
    ]


Font!
-----

A texture-based font.

    make font! [%font-file.ttf 20]
    make font! [%font-file.ttf face 1 20 "characters" 256,128]
    make font! [raster! binary!]
    make font! [texture! binary!]


Shader!
-------

    make shader! [
        vertex {...}
        fragment {...}
        default []
    ]


Fbo!
----

A framebuffer object is a render target.


Vbo!
----

Vertex buffers hold arrays of geometry vertex attributes.


Quat!
-----

A unit-length quaternion.

Multiply a quat! by -1.0 to conjugate (invert) it.
Multiplying a quat! by a vec3! will return a transformed vec3!.

Examples quaternions::

    to-quat none     ; Identity
    to-quat 10,0,240 ; From euler x,y,z angles


Widget!
-------



Draw Program Instructions
=========================

In addition to the following instructions, any paren! value will
be evaluated as Boron code at that point in the draw list.

Some instructions accept a get-word! argument.  When this is used, the word
value is read each time program is run rather than using a fixed value.


Box
---

Draws a box given minimum and maximum extents.
Internally, a vertex buffer with normals & texture coordinates is created.

    box
        min   int!/decimal!/vec3!
        max   int!/decimal!/vec3!


Blend
-----

Sets the blending mode using [glBlendFunc].

    blend
        mode    on/off/add/burn/trans

Mode   Function
-----  ----------------------
on     glEnable( GL_BLEND )
off    glDisable( GL_BLEND )
add    glBlendFunc( GL_SRC_ALPHA, GL_ONE )
burn   glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA )
trans  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA )
-----  ----------------------


Buffer
------

Calls [glBindBuffer] and sets pointer offsets using [glVertexPointer][],
[glNormalPointer], etc.


Camera
------

Sets the camera context.  [glViewport] is called and the `GL_PROJECTION`
and `GL_MODELVIEW` matrices are set.

    camera
        which   context!


Call
----

Calling a none! value does nothing and can be used to disable the display
of an item.

    call
        list    none!/draw-prog!/widget!/get-word!


Clear
-----

Calls [glClear] ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ).


Color
-----

Sets the `gl_Color` for shaders.

    color
        value   int!/coord!/vec3!/get-word!

The following all set the color to red::

    color 0xff0000
    color 255,0,0
    color 1.0,0.0,0.0


Color-Mask
----------

Calls [glColorMask] with all GL_TRUE or GL_FALSE arguments.

    color-mask on/off


Cull
----

Calls glEnable/glDisable with GL_CULL_FACE.

    cull on/off


Depth-Mask
----------

Calls [glDepthMask] with GL_TRUE/GL_FALSE.

    depth-mask on/off


Font
----

Sets the font for [text][Text] instructions.  This only provides the glyph metrics
needed to generate quads; it does not actually emit any data into the compiled
draw-prog.  The [shader][Shader] instruction must be used to specify the texture.

    font
        which   font!


Image
-----

Display image at 1:1 texel to pixel scale.
An optional X,Y position can be specified.

    image texture!
    image coord!/vec3! texture!


Light
-----

Controls lights.


Pop
---

Calls [glPopMatrix].


Point-Size
----------

    point-size on/off


Point-Sprite
------------

    point-sprite on/off


Push
----

Calls [glPushMatrix].


Rotate
------

Rotate around axis or by quaternion.

    rotate x/y/z decimal!
    rotate x/y/z get-word!
    rotate get-word!


Scale
-----

    scale
        value   decimal!/get-word!


Shader
------

Calls [glUseProgram], binds and enables any textures used,
and calls [glUniform] for any variables.

    shader
        which   shader!


Sphere
------

Draws a sphere.
Internally, a vertex buffer with normals & texture coordinates is created.

    sphere
        radius          int!/decimal!
        slices,stacks   coord!

Text
----

Draws quads for each character.

    text x,y string!
    text/center/right rect [x,y] string!

### Text Center

Using the *center* option centers the string horizontally and vertically within
the *rect* argument.  To center only horizontally use a width (fourth value)
of zero.


Translate
---------

Translate the model view matrix.

    translate
        distance    vec3!/get-word!


Uniform
-------


Framebuffer
-----------

Calls [glBindFramebuffer] with target GL_FRAMEBUFFER.  Pass zero to restore rendering to the display.

    framebuffer
        which   fbo! or 0


[comment]: <> (include widgets.md)


[function reference]: http://urlan.sf.net/boron/doc/func_ref_gl.html
[glBindBuffer]: http://www.opengl.org/sdk/docs/man2/xhtml/glBindBuffer.xml
[glBindFramebuffer]: http://www.opengl.org/sdk/docs/man3/xhtml/glBindFramebuffer.xml
[glBlendFunc]: http://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml
[glClear]: http://www.opengl.org/sdk/docs/man2/xhtml/glClear.xml
[glColorMask]: http://www.opengl.org/sdk/docs/man2/xhtml/glColorMask.xml
[glDepthMask]: http://www.opengl.org/sdk/docs/man2/xhtml/glDepthMask.xml
[glNormalPointer]: http://www.opengl.org/sdk/docs/man2/xhtml/glNormalPointer.xml
[glPopMatrix]: http://www.opengl.org/sdk/docs/man2/xhtml/glPopMatrix.xml
[glPushMatrix]: http://www.opengl.org/sdk/docs/man2/xhtml/glPushMatrix.xml
[glUniform]: http://www.opengl.org/sdk/docs/man2/xhtml/glUniform.xml
[glUseProgram]: http://www.opengl.org/sdk/docs/man2/xhtml/glUseProgram.xml
[glVertexPointer]: http://www.opengl.org/sdk/docs/man2/xhtml/glVertexPointer.xml
[glViewport]: http://www.opengl.org/sdk/docs/man2/xhtml/glViewport.xml
