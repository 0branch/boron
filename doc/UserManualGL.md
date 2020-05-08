---
title:  Boron-GL User Manual
date:   Version 2.0.2, 2020-03-07
---


Overview
========

Boron-GL extends the Boron scripting language with datatypes and functions
for working with OpenGL 3.3 & ES 3.1.

Features include:

  * Garbage collected GL datatypes.
  * Texture-based fonts loaded from TrueType files.
  * Loading PNG images.
  * Plays audio from WAV and OGG formats.
  * GUI system with automatic layout and styling.
  * Animation system.


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
It is reminiscent of the OpenGL 1.0 display list (now deprecated in OpenGL 3.0)
but operates on modern elements such as shaders and vertex buffers.

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
        'depth
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

Calls [glBindBuffer] and sets pointer offsets using [glVertexAttribPointer].

Currently only a single *buffer* command can be used between one or more
primitive draw commands ([tris], [points], etc.).


Buffer-inst
-----------

Binds an array buffer like [buffer], but also sets the [glVertexAttribDivisor]
to 1 for use in instanced draws.

Currently buffer-inst must follow a [buffer] command.


Camera
------

Sets the camera context.  [glViewport] is called and the `GL_PROJECTION`
and `GL_MODELVIEW` matrices are set.

    camera
        which   context!


Call
----

Runs another draw program.

Calling a none! value does nothing and can be used to disable the display
of an item.

    call
        list    none!/draw-prog!/widget!/get-word!


Clear
-----

Calls [glClear] ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ).


Color
-----

Sets a shader vec4 uniform at layout(location = 1).

    color
        value   int!/double!/coord!/vec3!/get-word!

The following all set the color to red (with alpha 1.0)::

    color 0xff0000
    color 255,0,0
    color 1.0,0.0,0.0

It is recommended to use vec3!.

Grey values (with alpha 1.0) can be set using int!/double!.


Color-Mask
----------

Calls [glColorMask] with all GL_TRUE or GL_FALSE arguments.

    color-mask on/off


Cull
----

Calls [glEnable]/[glDisable] with GL_CULL_FACE.

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


Framebuffer
-----------

Calls [glBindFramebuffer] with target GL_FRAMEBUFFER.  Pass zero to restore rendering to the display.

    framebuffer
        which   fbo! or 0


Image
-----

Display image at 1:1 texel to pixel scale.
An optional X,Y position can be specified.

    image [x,y] texture!
    image x,y,w,h
    image/horiz xmin,xmax,ycenter texture!
    image/vert  ymin,ymax,xcenter texture!


Light
-----

Controls lights.


Lines
-----

Draw line primitives using [glDrawElements] with GL_LINES.

    lines
        elements    int!/vector!


Line-strip
----------

Draw line primitives using [glDrawElements] with GL_LINES.

    line-strip
        elements    int!/vector!


Pop
---

Calls the equivalent of [glPopMatrix].


Points
------

Draw point primitives using [glDrawArrays] or [glDrawElements] with GL_POINTS.

    points
        elements    int!/vector!


Point-Size
----------

    point-size on/off


Point-Sprite
------------

    point-sprite on/off


Push
----

Calls the equivalent of [glPushMatrix] on the modelview stack.


Push-Model
----------

Push (and multiply) a matrix onto the modelview stack and also copy the
original matrix into a shader uniform.

Calls the equivalent of [glPushMatrix], [glMultMatrix] &
[glUniformMatrix4fv][glUniform].

    push-model uniform-name :matrix-variable


Push-Mul
--------

Calls the equivalent of [glPushMatrix] followed by [glMultMatrix] on the
modelview stack.

    push-mul :matrix


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


Tris
----

Draw triangles primitives using [glDrawElements] with GL_TRIANGLES.

    tris
        elements    int!/vector!


Tri-fan
-------

Draw triangles primitives using [glDrawElements] with GL_TRIANGLE_FAN.

    tri-fan
        elements    int!/vector!


Tri-strip
---------

Draw triangles primitives using [glDrawElements] with GL_TRIANGLE_STRIP.

    tri-strip
        elements    int!/vector!


Tris-inst
---------

Draw triangles primitives using [glDrawElementsInstanced].

    tris-inst
        elements    int!/vector!
        count


Quad
----

Draw a single textured rectangle.

    quad
        tex-size    int!/vector!
        uvs         coord!
        area        coord!


Quads
-----

Draw quadrangle primitives (each as two triangles).

    quads
        elements    int!/vector!


Uniform
-------

Set a shader uniform variable using [glUniform] or [glBindTexture] (for
textures).

    uniform
        location    int!/word!
        value       logic!/int!/double!/vec3!/texture!/lit-word!

A word! location is converted to an integer (using glGetUniformLocation) during
compilation.

If value is 'projection or 'modelview then the respective internal matrix
is copied to location.


View-Uniform
------------

Transform a point into view space and assign this to a uniform using
[glUniform3fv][glUniform].

    view-uniform
        location    int!/word!
        point       get-word! that references a vec3!.
        /dir        Treat point as a directional vector.

Using the *dir* option will only transform the point by the upper 3x3 portion
of the view matrix.


[comment]: <> (include widgets.md)


[function reference]: http://urlan.sf.net/boron/doc/func_ref_gl.html
[glBindBuffer]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glBindBuffer.xhtml
[glBindFramebuffer]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glBindFramebuffer.xhtml
[glBindTexture]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glBindTexture.xhtml
[glBlendFunc]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glBlendFunc.xhtml
[glClear]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glClear.xhtml
[glColorMask]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glColorMask.xhtml
[glDepthMask]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glDepthMask.xhtml
[glDisable]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glEnable.xhtml
[glDrawArrays]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glDrawArrays.xhtml
[glDrawElements]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glDrawElements.xhtml
[glDrawElementsInstanced]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glDrawElementsInstanced.xhtml
[glEnable]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glEnable.xhtml
[glPopMatrix]: http://www.opengl.org/sdk/docs/man2/xhtml/glPopMatrix.xml
[glPushMatrix]: http://www.opengl.org/sdk/docs/man2/xhtml/glPushMatrix.xml
[glMultMatrix]: http://www.opengl.org/sdk/docs/man2/xhtml/glMultMatrix.xml
[glUniform]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glUniform.xhtml
[glUseProgram]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glUseProgram.xhtml
[glVertexAttribPointer]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glVertexAttribPointer.xhtml
[glVertexAttribDivisor]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glVertexAttribDivisor.xhtml
[glViewport]: https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glViewport.xhtml
