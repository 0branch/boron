#ifndef DRAW_PROG_H
#define DRAW_PROG_H
/*
  Boron OpenGL Draw Program
  Copyright 2008-2020 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "geo.h"


#define DP_MAX_ARGS    5    // Maximum arguments per instruction.
#define DP_MAX_VBUF    3

typedef struct
{
    GLenum usage;
    UIndex keyN;
    UIndex vecN;
}
DPBuffer;


typedef struct
{
    GLuint  shaderProg;
    GLuint  currentVAO;
    GLuint  guiVAO;
    GLuint  shapeVAO;
    GLuint  userVAO;
}
DPCompileContext;


typedef struct
{
    DPCompileContext* ccon;
    DPCompileContext ccStorage;
    UBuffer  ref;
    UBuffer  inst;
    UBuffer  blocks;
    UBuffer  primRec;
    int      opCount;
    int      argCount;
    int      refCount;
    int      blkCount;
    int16_t  vbufCount;
    int16_t  _pad;
    uint32_t uid;       // Unique Id
    uint32_t ops;
    uint32_t args[ 4 * DP_MAX_ARGS ];
    DPBuffer vbuf[ DP_MAX_VBUF ];
    UIndex   fontN;       // TexFont binary
    uint32_t indexCount;
    GLuint   indexBuf;
    UIndex   shaderResN;
    GLfloat  penX;
    GLfloat  penY;
    Geometry tgeo;      // Textured geometry.
    Geometry sgeo;      // Shape geometry with uvs & normals.
}
DPCompiler;


typedef struct DrawTextState DrawTextState;
struct DrawTextState
{
    const TexFont* tf;
    const uint8_t* (*lowChar)( DrawTextState*, const uint8_t*,
                                               const uint8_t* ); 
    TexFontGlyph* prev;
    GLfloat x;
    GLfloat y;
    GLfloat marginL;
    GLfloat marginR;
    int emitTris;
};


typedef uint16_t    DPSwitch;


typedef struct
{
    GLuint samplesQueryId;
    GLuint currentProgram;
    TexFont* font;
}
DPState;


// Hardcoded shader uniform locations (requires GLES 3.1).
enum DPUniformLocation
{
    ULOC_TRANSFORM,         // layout(location = 0) uniform mat4 transform;
    ULOC_COLOR,             // layout(location = 1) uniform vec4 baseColor;
    ULOC_MODELVIEW          // layout(location = 2) uniform mat4 modelView;
};


// Hardcoded vertex shader input locations for common attributes.
enum DPAttribLocation
{
    ALOC_VERTEX,            // layout(location = 0) in vec3 position;
    ALOC_TEXTURE,           // layout(location = 1) in vec2 uv;
    ALOC_COLOR,             // layout(location = 2) in vec3 color;
    ALOC_NORMAL             // layout(location = 3) in vec3 normal;
};


extern DPCompiler* gDPC;

UIndex  ur_makeDrawProg( UThread* );
void    ur_markDrawProg( UThread*, UIndex );
void    dprog_init( UBuffer* );
void    dprog_destroy( UBuffer* );
void    ur_initDrawState( DPState* );
UStatus ur_runDrawProg( UThread*, UIndex progN );

void*   ur_compileDrawProg( UThread*, const UCell* blkCell,
                            UIndex replaceBufN );
DPCompiler* ur_beginDP( DPCompiler* );
UStatus ur_compileDP( UThread*, const UCell* blkCell );
void    ur_endDP( UThread*, UBuffer*, DPCompiler* prev );
void ur_setDPSwitch( UThread*, UIndex resN, DPSwitch sid, int n );
void ur_setTransXY( UThread*, UIndex resN, DPSwitch sid, float x, float y );

DPSwitch dp_beginSwitch( DPCompiler*, int count );
void     dp_endCase( DPCompiler*, DPSwitch sid );
void     dp_endSwitch( DPCompiler*, DPSwitch sid, uint16_t n );

DPSwitch dp_beginTransXY( DPCompiler*, float x, float y );
void     dp_endTransXY( DPCompiler* );

void     dp_drawText( DPCompiler*, const TexFont*,
                      const uint8_t* it, const uint8_t* end );

void vbo_initTextIndices( uint16_t* ip, int idx, int glyphCount );
/*
int  vbo_drawText( TexFont* tf,
                   GLfloat* tp, GLfloat* vp, int stride,
                   GLfloat x, GLfloat y, GLfloat* newXY,
                   const uint8_t* it, const uint8_t* end );
                   */
void vbo_drawTextInit( DrawTextState*, const TexFont*, GLfloat x, GLfloat y );
int  vbo_drawText( DrawTextState* ds,
                   GLfloat* tp, GLfloat* vp, int stride,
                   const uint8_t* it, const uint8_t* end );


#endif /*DRAW_PROG_H*/
