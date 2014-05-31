#ifndef DRAW_PROG_H
#define DRAW_PROG_H
/*
  Boron OpenGL Draw Program
  Copyright 2008-2010 Karl Robillard

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
    GLuint   shaderProg;
    UIndex   shaderResN;
    GLfloat  penX;
    GLfloat  penY;
    Geometry tgeo;      // Textured geometry.
}
DPCompiler;


typedef struct DrawTextState DrawTextState;
struct DrawTextState
{
    TexFont* tf;
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


struct ClientState
{
    uint16_t flags;
    uint16_t attrCount;
    int attr[4];
};

// Client State Array Flags
#define CSA_NORMAL  0x01
#define CSA_COLOR   0x02
#define CSA_UV      0x04


typedef struct
{
    struct ClientState client;
    GLuint samplesQueryId;
    GLuint currentProgram;
    TexFont* font;
}
DPState;


extern DPCompiler* gDPC;

UIndex ur_makeDrawProg( UThread* );
void   ur_markDrawProg( UThread*, UIndex );
void   dprog_destroy( UBuffer* );
void   ur_initDrawState( DPState* );
int    ur_runDrawProg( UThread*, UIndex progN );

DPCompiler* ur_beginDP( DPCompiler* );
int  ur_compileDP( UThread*, const UCell* blkCell, int handleError );
void ur_endDP( UThread*, UBuffer*, DPCompiler* prev );
void ur_setDPSwitch( UThread*, UIndex resN, DPSwitch sid, int n );
void ur_setTransXY( UThread*, UIndex resN, DPSwitch sid, float x, float y );

DPSwitch dp_beginSwitch( DPCompiler*, int count );
void     dp_endCase( DPCompiler*, DPSwitch sid );
void     dp_endSwitch( DPCompiler*, DPSwitch sid, uint16_t n );

DPSwitch dp_beginTransXY( DPCompiler*, float x, float y );
void     dp_endTransXY( DPCompiler* );

void vbo_initTextIndices( uint16_t* ip, int idx, int glyphCount );
/*
int  vbo_drawText( TexFont* tf,
                   GLfloat* tp, GLfloat* vp, int stride,
                   GLfloat x, GLfloat y, GLfloat* newXY,
                   const uint8_t* it, const uint8_t* end );
                   */
void vbo_drawTextInit( DrawTextState* ds, TexFont*, GLfloat x, GLfloat y );
int  vbo_drawText( DrawTextState* ds,
                   GLfloat* tp, GLfloat* vp, int stride,
                   const uint8_t* it, const uint8_t* end );


#endif /*DRAW_PROG_H*/
