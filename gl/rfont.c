/*
  Boron OpenGL Raster Font
  Copyright 2005-2010 Karl Robillard

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


#include "TexFont.h"
#include "os.h"
#include "boron-gl.h"
#include "rfont.h"
#include "glid.h"


static void buildFontTexture( RasterHead* img, GLuint id )
{
    GLenum format;
    GLint  comp;

    switch( img->format )
    {
        case UR_RAST_GRAY:
            //format = GL_LUMINANCE;
            //comp   = 1;
            format = GL_LUMINANCE_ALPHA;
            comp   = GL_LUMINANCE_ALPHA;
            break;

        case UR_RAST_RGB:
            format = GL_RGB;
            comp   = 3;
            break;

        case UR_RAST_RGBA:
            format = GL_RGBA;
            comp   = 4;
            break;

        default:
            return;
    }

    glBindTexture( GL_TEXTURE_2D, id );

    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    if( format == GL_LUMINANCE_ALPHA )
    {
        int i;
        int tsize = img->width * img->height;
        GLubyte* buf = (GLubyte*) memAlloc( 2 * tsize );
        GLubyte* src = (GLubyte*) (img + 1);
        GLubyte* dst = buf;

        for( i = 0; i < tsize; i++ )
        {
            *dst++ = *src;
            *dst++ = *src++;
        }

        glTexImage2D( GL_TEXTURE_2D, 0, comp, img->width, img->height, 0,
                      format, GL_UNSIGNED_BYTE, buf );

        memFree( buf );
    }
    else
    {
        glTexImage2D( GL_TEXTURE_2D, 0, comp, img->width, img->height, 0,
                      format, GL_UNSIGNED_BYTE, img + 1 );
    }


    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}


#if 0
void destroyFont( Resource* res )
{
    if( fr->glTexId )
        glDeleteTextures( 1, &fr->glTexId );
}
#endif


/*
  Initialize RFont cell and build texture if needed.

  \param binN   UIndex of binary holding TexFont.
  \param tex    Pointer to texture cell.  If zero, then a texture will be built.
  \param rastN  May be zero.

  \return zero if error thrown.
*/
int ur_makeRFontBin( UThread* ut, UCell* res, UIndex binN,
                     const UCell* tex, UIndex rastN )
{
    GLuint texId;
    UBuffer* bin;

    if( tex )
    {
        texId = ur_texId(tex);
        rastN = ur_texRast(tex);
    }
    else if( rastN )
    {
        texId = glid_genTexture();
        bin = ur_buffer( rastN );
        buildFontTexture( (RasterHead*) bin->ptr.v, texId );
    }
    else
    {
        texId = 0;
    }

    // Should ditch raster once we have the texture?
    // Not if we want to save it later.

    ur_setId( res, UT_FONT );
    ur_fontRast(res)  = rastN;
    ur_fontTF(res)    = binN;
    ur_fontTexId(res) = texId;

    return 1;
}


static uint8_t _defaultCodes[] =
    " ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890abcdefghijklmnopqrstuvwxyz"
    "?.;,!*:\"/+-|'@#$%^&<>()[]{}_";

/*
  Returns zero if error thrown.
*/
int ur_makeRFont( UThread* ut, UCell* res, RFontConfig* cfg )
{
    UIndex binN;
    UIndex hold;
    UBuffer* bin;
    TexFontImage fbm;
    TexFont* tf;
    int ok;


    if( cfg->pointSize < 1 )
        cfg->pointSize = 20;
    else if( cfg->pointSize > 100 )
        cfg->pointSize = 100;

    bin = ur_makeRaster( ut, UR_RAST_GRAY, cfg->texW, cfg->texH, res );
    if( ! bin->ptr.b )
    {
        ur_error( ut, UR_ERR_SCRIPT, "font raster is empty" );
        return 0;
    }
    hold = ur_hold( res->series.buf );

    fbm.pixels = ur_rastElem(bin);
    fbm.width  = cfg->texW;
    fbm.height = cfg->texH;
    fbm.pitch  = fbm.width;

    tf = txf_build( cfg->fontFile, cfg->glyphs ? cfg->glyphs : _defaultCodes,
                    &fbm, cfg->pointSize, 1, 1 );
    if( tf )
    {
        // Move TexFont to garbage collected binary.
        binN = ur_makeBinary( ut, txf_byteSize(tf) );
        bin = ur_buffer( binN );
        bin->used = ur_avail(bin);
        memCpy( bin->ptr.v, tf, bin->used );
        free( tf );

        ok = ur_makeRFontBin( ut, res, binN, 0, res->series.buf );
    }
    else
    {
        ur_error( ut, UR_ERR_ACCESS, "Font build failed" );
        ok = 0;
    }

    ur_release( hold );
    return ok;
}


/*EOF*/
