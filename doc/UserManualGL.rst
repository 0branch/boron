==============================
    Boron-GL User Manual
==============================

:Version:   0.1.11
:Date:      |date|

.. sectnum::
.. contents::


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

There is a separate `function reference`_ available online at
http://urlan.sourceforge.net/boron.



Running Scripts
===============


Shell Invocation
----------------

The first line of a script may be a UNIX shell sha-bang (``#!``) command.

::

    #!/usr/bin/boron-gl


Command Line Usage
------------------

Usage::

    boron-gl [options] [script] [arguments]


Command Line Options
~~~~~~~~~~~~~~~~~~~~

==========  ========================
-a          Disable audio
-e "*exp*"  Evaluate expression
-h          Show help and exit
-s          Disable security
==========  ========================


Command Line Arguments
~~~~~~~~~~~~~~~~~~~~~~

If the interpreter is invoked with a script then the *args* word will be set
to either a block of strings, or *none* if no script arguments were given.

So this command::

    boron-gl -e "probe args" file1 -p 2

Will print this::

    ["file1" "-p" "2"]



GL Datatypes
============

=============  =============================
Type Name      Description
=============  =============================
`draw-prog!`_  Compiled draw program
`raster!`_     Pixel image in main memory
`texture!`_    OpenGL texture
`font!`_       Texture-based font
`shader!`_     OpenGL shader
`fbo!`_        OpenGL frame buffer object
`vbo!`_        OpenGL vertex buffer object
`quat!`_       Quaternion
`widget!`_     Widget
=============  =============================


Draw-Prog!
----------

A draw program is a list of display operations compiled to byte-code.
It is reminiscent of the GL display list (now deprecated in OpenGL 3.0).

.. note::
    The terms "draw program" and "draw list" are used interchangeably.


Raster!
-------


Texture!
--------

The simplest way to make textures is to use the *load-texture* function.
::

    load-texture %skin.png

Full specification::

    make texture! [
        raster!/coord!/int!
        binary!
        'mipmap 'nearest 'linear 'repeat 'clamp
        'gray 'rgb' 'rgba
    ]


Font!
-----

A texture-based font.
::

    make font! [%font-file.ttf 20]
    make font! [%font-file.ttf 20 "characters" 256,128]
    make font! [raster! binary!]
    make font! [texture! binary!]


Shader!
-------
::

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
::

    box vec3! vec3!


Blend
-----

Sets the blending mode using glBlendFunc_.
::

    blend on/off/add/burn/trans

=====  ======================
Mode   Function
=====  ======================
on     glEnable( GL_BLEND )
off    glDisable( GL_BLEND )
add    glBlendFunc( GL_SRC_ALPHA, GL_ONE )
burn   glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA )
trans  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA )
=====  ======================


Buffer
------

Calls glBindBuffer_ and sets pointer offsets using glVertexPointer_,
glNormalPointer_, etc.


Camera
------

Sets the camera context.  glViewport_ is called and the ``GL_PROJECTION``
and ``GL_MODELVIEW`` matrices are set.
::

    camera context!


Call
----

Calling a none! value does nothing and can be used to disable the display
of an item.
::

    call none!/draw-prog!/widget!
    call get-word!


Clear
-----

Calls glClear_ ( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ).


Color
-----

Sets the ``gl_Color`` for shaders.
::

    color int!/coord!/vec3!
    color get-word!

The following all set the color to red::

    color 0xff0000
    color 255,0,0
    color 1.0,0.0,0.0


Color-Mask
----------

Calls glColorMask_ with all GL_TRUE or GL_FALSE arguments.
::

    color-mask on/off


Cull
----

Calls glEnable/glDisable with GL_CULL_FACE.
::

    cull on/off


Depth-Mask
----------

Calls glDepthMask_ with GL_TRUE/GL_FALSE.
::

    depth-mask on/off


Font
----

Sets the font for `text`_ instructions.  This only provides the glyph metrics
needed to generate quads; it does not actually emit any data into the compiled
draw-prog.  The `shader`_ instruction must be used to specify the texture.
::

    font font!


Image
-----

Display image at 1:1 texel to pixel scale.
An optional X,Y position can be specified.
::

    image texture!
    image coord!/vec3! texture!


Light
-----

Controls lights.


Pop
---

Calls glPopMatrix_.


Point-Size
----------
::

    point-size on/off


Point-Sprite
------------
::

    point-sprite on/off


Push
----

Calls glPushMatrix_.


Rotate
------

Rotate around axis or by quaternion.
::

    rotate x/y/z decimal!
    rotate x/y/z get-word!
    rotate get-word!


Scale
-----
::

    scale decimal!
    scale get-word!


Shader
------

Calls glUseProgram_, binds and enables any textures used,
and calls glUniform_ for any variables.
::

    shader shader!


Sphere
------

Draws a sphere.
Internally, a vertex buffer with normals & texture coordinates is created.
::

    sphere radius slices,stacks


Text
----

Draws quads for each character.
::

    text x,y string!
    text/center/right rect [x,y] string!


Translate
---------
::

    translate get-word!


Uniform
-------


Framebuffer
-----------

Calls glBindFramebuffer().



.. |date| date::
.. _`function reference`: http://urlan.sf.net/boron/doc/func_ref_gl.html
.. _glBindBuffer: http://www.opengl.org/sdk/docs/man/xhtml/glBindBuffer.xml
.. _glBlendFunc: http://www.opengl.org/sdk/docs/man/xhtml/glBlendFunc.xml
.. _glClear: http://www.opengl.org/sdk/docs/man/xhtml/glClear.xml
.. _glColorMask: http://www.opengl.org/sdk/docs/man/xhtml/glColorMask.xml
.. _glDepthMask: http://www.opengl.org/sdk/docs/man/xhtml/glDepthMask.xml
.. _glNormalPointer: http://www.opengl.org/sdk/docs/man/xhtml/glNormalPointer.xml
.. _glPopMatrix: http://www.opengl.org/sdk/docs/man/xhtml/glPopMatrix.xml
.. _glPushMatrix: http://www.opengl.org/sdk/docs/man/xhtml/glPushMatrix.xml
.. _glUniform: http://www.opengl.org/sdk/docs/man/xhtml/glUniform.xml
.. _glUseProgram: http://www.opengl.org/sdk/docs/man/xhtml/glUseProgram.xml
.. _glVertexPointer: http://www.opengl.org/sdk/docs/man/xhtml/glVertexPointer.xml
.. _glViewport: http://www.opengl.org/sdk/docs/man/xhtml/glViewport.xml

