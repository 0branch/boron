#ifndef BORON_GL_H
#define BORON_GL_H
/*
  Boron OpenGL Module
  Copyright 2005-2011 Karl Robillard

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


#include <GL/glv.h>
#include "boron.h"
#include "gui.h"
#include "TexFont.h"


enum BoronGLDataType
{
    UT_DRAWPROG = UT_BORON_COUNT,
    UT_RASTER,
    UT_TEXTURE,
    UT_FONT,
    UT_SHADER,
    UT_FBO,
    UT_VBO,
    UT_QUAT,
    UT_WIDGET,

    UT_GL_COUNT
};


//----------------------------------------------------------------------------


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t glListPool;
    UIndex   blkN;
    UIndex   _pad;
    GLuint   glListId;
}
UCellDrawList;


//----------------------------------------------------------------------------


enum URasterFormat
{
    UR_RAST_GRAY = 1,
    UR_RAST_RGB,
    UR_RAST_RGBA
};


typedef struct
{
    uint8_t  format;
    uint8_t  depth;
    uint16_t width;
    uint16_t height;
    uint8_t  _pad[2];
    // Element data follows.
}
RasterHead;


//----------------------------------------------------------------------------


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t _pad;
    UIndex   rasterN;    // Raster binary
    UIndex   fontN;      // TexFont binary
    GLuint   glTexId;
}
UCellRasterFont;


typedef UCellRasterFont     UCellTexture;


typedef struct
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t _pad;
    GLuint   glFboId;
    GLuint   glRenId;
    GLuint   glTexId;
}
UCellFramebuffer;


//----------------------------------------------------------------------------


enum CameraContext
{
    CAM_CTX_ORIENT = 0,
    CAM_CTX_VIEWPORT,
    CAM_CTX_FOV,
    CAM_CTX_NEAR,
    CAM_CTX_FAR,
    CAM_CTX_COUNT,

    CAM_CTX_ORBIT = CAM_CTX_COUNT,
    CAM_CTX_FOCAL_PNT,
    CAM_CTX_ORBIT_COUNT
};


struct GLEnv
{
    GLView* view;
    UAtomEntry* drawOpTable;
    GWidget* eventWidget;
    UThread* guiUT;
    UCell*   guiStyle;          // For GWidget layout & render.
    UBuffer tmpBin;
    UBuffer tmpStr;
    UBuffer widgetClasses;
    UBuffer rootWidgets;
    GLint   maxTextureUnits;
    int     mouseDeltaX;
    int     mouseDeltaY;
    int     prevMouseX;
    int     prevMouseY;
    char    guiThrow;
};


extern struct GLEnv glEnv;

extern UThread* boron_makeEnvGL( UDatatype**, unsigned int dtCount );
extern void boron_freeEnvGL( UThread* );

void ur_markBlkN( UThread* ut, UIndex blkN );

UBuffer* ur_makeRaster( UThread*, int format, int w, int h, UCell* res );
int      ur_rasterElementSize( const RasterHead* );
void     ur_rasterBlit( const RasterHead* src,  uint16_t* srcRect,
                        RasterHead* dest, uint16_t* destRect );

TexFont* ur_texFontV( UThread*, const UCell* );

UIndex ur_makeVbo( UThread*, GLenum attrUsage, int acount, float* attr,
                   int icount, uint16_t* indices );


#define ur_rasterElements(rh)       (((char*) rh) + sizeof(RasterHead))
#define ur_rasterBytesPerRow(rh)    (rh->width * ur_rasterElementSize(rh))

#define ur_rastHead(c)      ((const RasterHead*) ur_bufferSer(c)->ptr.v)
#define ur_rastHeadM(c)     ((RasterHead*) ur_bufferSer(c)->ptr.v)
#define ur_rastElem(bin)    ur_rasterElements((bin)->ptr.b)

#define ur_fontRast(c)      ((UCellRasterFont*) (c))->rasterN
#define ur_fontTF(c)        ((UCellRasterFont*) (c))->fontN
#define ur_fontTexId(c)     ((UCellRasterFont*) (c))->glTexId

#define ur_texRast(c)       ((UCellTexture*) (c))->rasterN
#define ur_texId(c)         ((UCellTexture*) (c))->glTexId

#define ur_fboId(c)         ((UCellFramebuffer*) (c))->glFboId
#define ur_fboRenId(c)      ((UCellFramebuffer*) (c))->glRenId
#define ur_fboTexId(c)      ((UCellFramebuffer*) (c))->glTexId

#define ur_vboResN(c)       (c)->series.buf

// vbo! UBuffer members
#define vbo_count(buf)      (buf)->elemSize
#define vbo_bufIds(buf)     ((GLuint*) &(buf)->used)


#define UR_GUI_THROW \
    glv_setEventHandler(glEnv.view, 0); \
    glEnv.guiThrow = 1


#endif /*BORON_GL_H*/
