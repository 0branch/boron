/*
  Copyright 2010-2012 Karl Robillard

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


#include "unset.h"
#include "boron-gl.h"
#include "rfont.h"
#include "shader.h"
#include "quat.h"


extern void binary_copy( UThread*, const UCell* from, UCell* res );
extern void binary_mark( UThread*, UCell* cell );
extern void binary_toShared( UCell* cell );
extern int boron_doVoid( UThread* ut, const UCell* blkC );


//----------------------------------------------------------------------------
// UT_DRAWPROG


int dprog_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        DPCompiler dpc;
        DPCompiler* save;
        UIndex n;
        int ok;

        save = ur_beginDP( &dpc );
        ok = ur_compileDP( ut, from, 0 );
        n = ur_makeDrawProg( ut );
        ur_endDP( ut, ur_buffer( n ), save );

        // Set result last since compile may evaluate parens.
        ur_setId(res, UT_DRAWPROG);
        ur_setSeries(res, n, 0 );
        return ok;
    }
    return ur_error( ut, UR_ERR_TYPE, "draw-prog! make expected block!" );
}


int dprog_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;

    switch( test )
    {
        case UR_COMPARE_SAME:
cmp:
            return (a->series.buf == b->series.buf) ? 1 : 0;

        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_is(a, UT_DRAWPROG) && ur_is(b, UT_DRAWPROG) )
                goto cmp;
            break;
    }
    return 0;
}


void dprog_mark( UThread* ut, UCell* cell )
{
    UIndex n = cell->series.buf;
    if( n > UR_INVALID_BUF )
        ur_markDrawProg( ut, n );
}


//----------------------------------------------------------------------------
// UT_RASTER


/*
  width,height,bytes-per-pixel
*/
int raster_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_COORD) )
    {
        int format = UR_RAST_RGB;
        if( from->coord.len > 2 )
        {
            switch( from->coord.n[2] )
            {
                case 1: format = UR_RAST_GRAY; break;
                case 4: format = UR_RAST_RGBA; break;
            }
        }
        ur_makeRaster( ut, format, from->coord.n[0], from->coord.n[1], res );
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "raster! make expected coord!" );
}


static const UCell*
raster_select( UThread* ut, const UCell* cell, const UCell* sel, UCell* res )
{
    const RasterHead* rh;

    switch( ur_atom(sel) )
    {
        case UR_ATOM_X:
        case UR_ATOM_WIDTH:
            rh = ur_rastHead(cell);
            ur_setId(res, UT_INT);
            ur_int(res) = rh ? rh->width : 0;
            break;

        case UR_ATOM_Y:
        case UR_ATOM_HEIGHT:
            rh = ur_rastHead(cell);
            ur_setId(res, UT_INT);
            ur_int(res) = rh ? rh->height : 0;
            break;

        case UR_ATOM_SIZE:
            rh = ur_rastHead(cell);
            ur_setId(res, UT_COORD);
            res->coord.len = 2;
            if( rh )
            {
                res->coord.n[0] = rh->width;
                res->coord.n[1] = rh->height;
            }
            else
            {
                res->coord.n[0] = 0;
                res->coord.n[1] = 0;
            }
            break;
#if 0
        case UR_ATOM_RECT:          // Can plug directly into camera/viewport.
            ur_setId(res, UT_COORD);
            res->coord.elem[0] = 0;
            res->coord.elem[1] = 0;

            rh = ur_rastHead(cell);
            if( rh )
            {
                res->coord.elem[2] = rh->width;
                res->coord.elem[3] = rh->height;
                res->coord.len = 4;
            }
            else
            {
                res->coord.len = 2;
            }
            break;
#endif
        case UR_ATOM_ELEM:
            ur_setId(res, UT_BINARY);
            ur_setSeries(res, cell->series.buf, sizeof(RasterHead));
            break;

        default:
            ur_error( ut, UR_ERR_SCRIPT,
                      "raster select expected x/y/width/height/size/elem" );
            return 0;
    }
    return res;
}


//----------------------------------------------------------------------------
// UT_TEXTURE


typedef struct
{
    const void* pixels;
    int    width;
    int    height;
    int    mipmap;
    GLenum format;
    GLint  comp;    // internalFormat
    GLint  wrap;
    GLint  min_filter;
    GLint  mag_filter;
}
TextureDef;


// TODO: Should check for ARB_texture_rg at run-time.
#ifdef GL_R8
#define IFORMAT_GRAY  GL_R8
#define FORMAT_GRAY   GL_RED
#else
#define IFORMAT_GRAY  GL_LUMINANCE
#define FORMAT_GRAY   GL_LUMINANCE
#endif


static void _texRast( TextureDef* tex, const RasterHead* rh )
{
    tex->width  = rh->width;
    tex->height = rh->height;
    tex->pixels = rh + 1;

    switch( rh->format )
    {
        case UR_RAST_GRAY:
            tex->comp   = IFORMAT_GRAY;
            tex->format = FORMAT_GRAY;
            break;

        case UR_RAST_RGB:
            tex->comp = tex->format = GL_RGB;
            break;

        case UR_RAST_RGBA:
        default:
            tex->comp = tex->format = GL_RGBA;
            break;
    }

    glPixelStorei( GL_UNPACK_ALIGNMENT, (rh->bytesPerRow & 3) ? 1 : 4 );
}


static void _texCoord( TextureDef* tex, const UCell* cell )
{
    if( ur_is(cell, UT_COORD) )
    {
        tex->width  = cell->coord.n[0];
        tex->height = cell->coord.n[1];
    }
    else
    {
        tex->width  = ur_int(cell);
        tex->height = 1;
    }

    tex->pixels = 0;
    tex->comp   =
    tex->format = GL_RGBA;

    if( cell->coord.len > 2 )
    {
        if( cell->coord.n[2] == 3 )
        {
            tex->comp = tex->format = GL_RGB;
        }
        else if( cell->coord.n[2] == 1 )
        {
            tex->comp   = IFORMAT_GRAY;
            tex->format = FORMAT_GRAY;
        }
    }
}


static int _textureKeyword( UAtom name, TextureDef* def )
{
    switch( name )
    {
        case UR_ATOM_CLAMP:
            def->wrap = GL_CLAMP_TO_EDGE;
            break;

        case UR_ATOM_REPEAT:
            def->wrap = GL_REPEAT;
            break;

        case UR_ATOM_NEAREST:
            def->min_filter = GL_NEAREST;
            def->mag_filter = GL_NEAREST;
            break;

        case UR_ATOM_LINEAR:
            def->min_filter = GL_LINEAR;
            def->mag_filter = GL_LINEAR;
            break;

        case UR_ATOM_MIPMAP:
            def->mipmap = 1;
            break;

        case UR_ATOM_GRAY:
            def->comp   = IFORMAT_GRAY;
            def->format = FORMAT_GRAY;
            break;

        case UR_ATOM_RGB:
            def->comp = def->format = GL_RGB;
            break;

        case UR_ATOM_RGBA:
            def->comp = def->format = GL_RGBA;
            break;

#ifndef GL_ES_VERSION_2_0 
        case UR_ATOM_F32:
            switch( def->comp )
            {
                case IFORMAT_GRAY:
#ifdef GL_R8
                    def->comp = GL_R32F;
#else
                    def->comp = GL_LUMINANCE32F_ARB;
#endif
                    break;
                case GL_RGB:
                    def->comp = GL_RGB32F_ARB;
                    //def.comp = GL_RGB16F_ARB;
                    break;
                case GL_RGBA:
                    def->comp = GL_RGBA32F_ARB;
                    //def.comp = GL_RGBA16F_ARB;
                    break;
            }
            break;
#endif

        default:
            return 0;
    }
    return 1;
}


/*
  texture! raster
  texture! coord   ; 2D Texture
  texture! int     ; 1D Texture
  texture! [
    raster!/coord!/int!/binary!/vector!
    'mipmap 'nearest 'linear 'repeat 'clamp
    'gray 'rgb 'rgba 'f32
  ]
*/
int texture_make( UThread* ut, const UCell* from, UCell* res )
{
    TextureDef def;
    GLuint name;
    GLenum type = GL_UNSIGNED_BYTE;
    UIndex rastN = 0;
    GLenum target = GL_TEXTURE_2D;


    def.mipmap     = 0;
    def.wrap       = GL_REPEAT;
    def.min_filter = GL_NEAREST_MIPMAP_LINEAR;
    def.mag_filter = GL_LINEAR;

    if( ur_is(from, UT_RASTER) )
    {
        rastN = from->series.buf;
        _texRast( &def, ur_rastHead(from) );
        goto build;
    }
    else if( ur_is(from, UT_COORD) )
    {
        _texCoord( &def, from );
        goto build;
    }
#ifndef GL_ES_VERSION_2_0 
    else if( ur_is(from, UT_INT) )
    {
        target = GL_TEXTURE_1D;
        _texCoord( &def, from );
        goto build;
    }
#endif
    else if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;
        const UCell* val;

        def.width = 0;

        ur_blkSlice( ut, &bi, from );
        ur_foreach( bi )
        {
            switch( ur_type(bi.it) )
            {
                case UT_WORD:
                    if( _textureKeyword( ur_atom(bi.it), &def ) )
                        break;
                    val = ur_wordCell( ut, bi.it );
                    if( ! val )
                        return UR_THROW;
                    if( ur_is(val, UT_RASTER) )
                    {
                        rastN = val->series.buf;
                        _texRast( &def, ur_rastHead(val) );
                    }
                    else if( ur_is(val, UT_BINARY) )
                    {
                        def.pixels = ur_bufferSer(val)->ptr.b;
                    }
                    else if( ur_is(val, UT_COORD) )
                    {
                        _texCoord( &def, val );
                    }
#ifndef GL_ES_VERSION_2_0 
                    else if( ur_is(val, UT_INT) )
                    {
                        target = GL_TEXTURE_1D;
                        _texCoord( &def, val );
                    }
#endif
                    else if( ur_is(val, UT_VECTOR) )
                    {
                        const UBuffer* buf = ur_bufferSer(val);
                        if( buf->form == UR_VEC_F32 )
                        {
                            type = GL_FLOAT;
                            def.pixels = ur_bufferSer(val)->ptr.f;
                        }
                        else
                        {
                            return ur_error( ut, UR_ERR_TYPE,
                                    "texture! make only accepts f32 vector!" );
                        }
                    }
                    break;

                case UT_LITWORD:
                    if( ! _textureKeyword( ur_atom(bi.it), &def ) )
                    {
                        return ur_error( ut, UR_ERR_TYPE,
                                         "Invalid texture! keyword" );
                    }
                    break;
#ifndef GL_ES_VERSION_2_0 
                case UT_INT:
                    target = GL_TEXTURE_1D;
                    _texCoord( &def, bi.it );
                    break;
#endif
                case UT_COORD:
                    _texCoord( &def, bi.it );
                    break;

                case UT_BINARY:
                    def.pixels = ur_bufferSer(bi.it)->ptr.b;
            }
        }

        if( def.width )
            goto build;
    }

    return ur_error( ut, UR_ERR_TYPE, "texture! make expected coord!/raster!" );

build:

    name = glid_genTexture();
    glBindTexture( target, name );

#ifndef GL_ES_VERSION_2_0 
    if( target == GL_TEXTURE_2D )
#endif
    {
        glTexImage2D( GL_TEXTURE_2D, 0, def.comp, def.width, def.height,
                      0, def.format, type, def.pixels );
    }
#ifndef GL_ES_VERSION_2_0 
    else
    {
        glTexImage1D( GL_TEXTURE_1D, 0, def.comp, def.width,
                      0, def.format, type, def.pixels );
    }
#endif

    {
        GLenum err = glGetError();
        if( err != GL_NO_ERROR )
        {
            return ur_error( ut, UR_ERR_INTERNAL, gl_errorString( err ) );
        }
    }

    if( def.mipmap )
    {
        //glEnable( target );   // Needed with ATI drivers?
        glGenerateMipmap( target );
    }
    else
    {
        // Ensure that a non-mipmap minifying filter is used.
        // Using a mipmap filter can cause FBO format errors and textures
        // to not appear.
        if( def.min_filter != GL_NEAREST )
            def.min_filter = GL_LINEAR;
    }

    // GL_CLAMP, GL_CLAMP_TO_EDGE, GL_REPEAT.
    glTexParameteri( target, GL_TEXTURE_WRAP_S, def.wrap );
    glTexParameteri( target, GL_TEXTURE_WRAP_T, def.wrap );

    // TODO: Support all GL_TEXTURE_MIN_FILTER types.
    // GL_NEAREST, GL_LINEAR, GL_LINEAR_MIPMAP_NEAREST, etc.
    glTexParameteri( target, GL_TEXTURE_MIN_FILTER, def.min_filter );

    // GL_NEAREST, GL_LINEAR
    glTexParameteri( target, GL_TEXTURE_MAG_FILTER, def.mag_filter );

    ur_setId( res, UT_TEXTURE );
    ur_texRast(res) = rastN;
    ur_texId(res)   = name;

    return UR_OK;
}


static void textureToRaster( UThread* ut, GLenum target, GLuint name,
                             UCell* res )
{
#ifdef GL_ES_VERSION_2_0 
    (void) ut;
    (void) target;
    (void) name;
    ur_setId(res, UT_NONE);
#else
    UBuffer* bin;
    GLint dim[ 2 ];

    glBindTexture( target, name );
    glGetTexLevelParameteriv( target, 0, GL_TEXTURE_WIDTH,  dim );
    glGetTexLevelParameteriv( target, 0, GL_TEXTURE_HEIGHT, dim + 1 );

    bin = ur_makeRaster( ut, UR_RAST_RGB, dim[0], dim[1], res );
    if( bin->ptr.b )
    {
        int bpr = ur_ptr(RasterHead, bin)->bytesPerRow;
        glPixelStorei( GL_PACK_ALIGNMENT, (bpr & 3) ? 1 : 4 );
        glGetTexImage( target, 0, GL_RGB, GL_UNSIGNED_BYTE, ur_rastElem(bin) );
    }
#endif
}


static const UCell*
texture_select( UThread* ut, const UCell* cell, const UCell* sel, UCell* tmp )
{
    if( ur_is(sel, UT_WORD) )
    {
        if( ur_atom(sel) == UR_ATOM_RASTER )
        {
            UIndex ri = ur_texRast(cell);
            if( ri )
            {
                ur_setId(tmp, UT_RASTER);
                ur_setSeries(tmp, ri, 0);
            }
            else
            {
                textureToRaster( ut, GL_TEXTURE_2D, ur_texId(cell), tmp );
            }
            return tmp;
        }
    }
    return raster_select( ut, cell, sel, tmp );
}


static void texture_recycle( UThread* ut, int phase )
{
    // The comparison with guiUT makes sure that we are in the GL thread
    // and that the GL context exists.  OpenGL calls made without a valid
    // context may segfault.

    if( ut != glEnv.guiUT )
        return; 

    switch( phase )
    {
        case UR_RECYCLE_MARK:
        {
            // Clear any unchecked GL error.
#ifdef DEBUG
            GLenum err = glGetError();
            if( err )
                printf( "texture_recycle: glGetError %s\n",
                        gl_errorString( err ) );
#else
            glGetError();
#endif
            glid_gcBegin();
        }
            break;

        case UR_RECYCLE_SWEEP:
            glid_gcSweep();
            break;
    }
}


static void texture_mark( UThread* ut, UCell* cell )
{
    ur_markBuffer( ut, ur_texRast(cell) );
    glid_gcMarkTexture( ur_texId(cell) );
}


//----------------------------------------------------------------------------
// UT_FONT


/*
  ["font.name" 20 "characters" 256,128]
  [raster binary]
  [texture binary]
*/
int rfont_make( UThread* ut, const UCell* from, UCell* res )
{
    RFontConfig cfg;
    UIndex rastN = 0;
    UIndex binN = 0;
    const UCell* tex = 0;

    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;

        cfg.fontFile  = 0;
        cfg.glyphs    = 0;
        cfg.faceIndex = 0;
        cfg.pointSize = 20;
        cfg.texW      = 256;
        cfg.texH      = 256;

        ur_blkSlice( ut, &bi, from );
        ur_foreach( bi )
        {
            switch( ur_type(bi.it) )
            {
                case UT_INT:
                    cfg.pointSize = ur_int(bi.it);
                    break;

                case UT_COORD:
                    cfg.texW = bi.it->coord.n[0];
                    cfg.texH = bi.it->coord.n[1];
                    break;

                case UT_WORD:
                    if( ur_atom(bi.it) == UR_ATOM_FACE )
                    {
                        ++bi.it;
                        if( (bi.it != bi.end) && ur_is(bi.it, UT_INT) )
                        {
                            cfg.faceIndex = ur_int(bi.it);
                        }
                    }
                    break;

                case UT_BINARY:
                    binN = bi.it->series.buf;
                    break;

                case UT_STRING:
                case UT_FILE:
                {
                    char* str = boron_cstr( ut, bi.it, 0 );
                    if( cfg.fontFile )
                        cfg.glyphs = (uint8_t*) str;
                    else
                        cfg.fontFile = str;
                }
                    break;

                case UT_RASTER:
                    rastN = bi.it->series.buf;
                    break;

                case UT_TEXTURE:
                    tex = bi.it;
                    break;
            }
        }
        if( (rastN || tex) && binN )
        {
            UBuffer* bin = ur_buffer( binN );
            txf_swap( (TexFont*) bin->ptr.v );

            if( ! ur_makeRFontBin( ut, res, binN, tex, rastN ) )
                return UR_THROW;
        }
        else if( cfg.fontFile )
        {
            if( ! ur_makeRFont( ut, res, &cfg ) )
                return UR_THROW;
        }
        else
        {
            return ur_error( ut, UR_ERR_SCRIPT,
                    "font! make expected filename or raster! & binary!" );
        }
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "font! make expected binary!/block!" );
}


#if 0
UR_CALL( to_rfont )
{
    UIndex n;
    UCell* res = ur_s_prev(tos);
    if( ur_datatype(tos) == UT_BINARY )
    {
        n = ur_fontTF(res);
        ur_initBinary(res, n, 0);
    }
    else if( ur_datatype(tos) == UT_RASTER )
    {
        n = ur_fontRast(res);
        ur_setId(res, UT_RASTER);
        ur_setSeries(res, n, 0);
    }
    else
    {
        return ur_error( ut, UR_ERR_TYPE,
                     "font! only changes to binary!/raster!" );
    }
    UR_S_DROP;
}
#endif


static const UCell*
rfont_select( UThread* ut, const UCell* cell, const UCell* sel, UCell* res )
{
    if( ur_is(sel, UT_WORD) )
    {
        switch( ur_atom(sel) )
        {
            case UR_ATOM_BINARY:
                ur_setId(res, UT_BINARY);
                ur_setSeries(res, ur_fontTF(cell), 0);
                return res;

            case UR_ATOM_RASTER:
                ur_setId(res, UT_RASTER);
                ur_setSeries(res, ur_fontRast(cell), 0);
                return res;

            case UR_ATOM_TEXTURE:
                ur_setId(res, UT_TEXTURE);
                ur_texId(res)   = ur_fontTexId(cell);
                ur_texRast(res) = ur_fontRast(cell);
                return res;
        }
    }
    ur_error( ut, UR_ERR_SCRIPT,
              "font select expected binary/raster/texture" );
    return 0;
}


static void rfont_mark( UThread* ut, UCell* cell )
{
    UCellRasterFont* fc = (UCellRasterFont*) cell;
    if( fc->rasterN )
        ur_markBuffer( ut, fc->rasterN );
    if( fc->fontN )
        ur_markBuffer( ut, fc->fontN );
    glid_gcMarkTexture( fc->glTexId );
}


//----------------------------------------------------------------------------
// UT_SHADER


static const char* shaderString( UThread* ut, const UCell* strC )
{
    UBuffer* buf = ur_bufferSerM( strC );
    if( ! buf )
        return 0;
    if( ur_strIsUcs2(buf) )
    {
        ur_error( ut, UR_ERR_SCRIPT, "shader string cannot be UCS2" );
        return 0;
    }
    ur_strTermNull( buf );
    return buf->ptr.c;
}


/*
  [
    vertex {...}
    fragment {...}
    default [...]
  ]
*/
int shader_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;
        UAtom cmd = 0;
        UIndex defN = UR_INVALID_BUF;
        const char* vprog = 0;
        const char* fprog = 0;


        ur_blkSlice( ut, &bi, from );
        ur_foreach( bi )
        {
            switch( ur_type(bi.it) )
            {
                case UT_WORD:
                    cmd = ur_atom(bi.it);
                    break;

                case UT_STRING:
                    if( cmd == UR_ATOM_VERTEX )
                    {
                        vprog = shaderString( ut, bi.it );
                        if( ! vprog )
                            return UR_THROW;
                    }
                    else if( cmd == UR_ATOM_FRAGMENT )
                    {
                        fprog = shaderString( ut, bi.it );
                        if( ! fprog )
                            return UR_THROW;
                    }
                    break;

                case UT_BLOCK:
                    if( cmd == UR_ATOM_DEFAULT )
                    {
                        defN = bi.it->series.buf;
                        if( ur_isShared( defN ) )
                        {
                            return ur_error( ut, UR_ERR_SCRIPT,
                                      "shader default block is shared" );
                        }
                    }
                    break;
            }
        }

        if( vprog && fprog )
        {
            if( ! ur_makeShader( ut, vprog, fprog, res ) )
                return UR_THROW;

            if( ur_is(res, UT_CONTEXT) && (defN != UR_INVALID_BUF) )
            {
                UCell blkC;
                ur_bind( ut, ur_buffer(defN), ur_bufferSer(res),
                         UR_BIND_THREAD );
                ur_setId( &blkC, UT_BLOCK );
                ur_setSeries( &blkC, defN, 0 );
                return boron_doVoid( ut, &blkC );
            }
            return UR_OK;
        }
        return ur_error( ut, UR_ERR_SCRIPT,
                  "shader! make expected vertex & fragment programs" );
    }
    return ur_error( ut, UR_ERR_TYPE, "shader! make expected block!" );
}


static void shader_destroy( UBuffer* buf )
{
    //printf( "KR shader_destroy()\n" );
    Shader* sh = (Shader*) buf->ptr.v;
    if( sh )
    {
        destroyShader( sh );
        memFree( sh );
        buf->ptr.v = 0;
    }
}


/*
void shader_mark( UThread* ut, UCell* cell )
{
    UIndex n = cell->series.buf;
    if( n > UR_INVALID_BUF )
        ur_markBuffer( ut, n );
}
*/


//----------------------------------------------------------------------------
// UT_FBO


#ifdef GL_ES_VERSION_2_0 
#define GL_FRAMEBUFFER_COMPLETE_EXT GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT GL_FRAMEBUFFER_UNSUPPORTED
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#endif


const char* _framebufferStatus()
{
    GLenum status;
    status = glCheckFramebufferStatusEXT( GL_FRAMEBUFFER_EXT );
    switch( status )
    {
        case GL_FRAMEBUFFER_COMPLETE_EXT:
            return 0;

        case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
            return "Unsupported framebuffer format";

        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
            return "Framebuffer incomplete, missing attachment";

        case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
            return
            "Framebuffer incomplete, attached images must have same dimensions";
#ifndef GL_ES_VERSION_2_0 
        case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
            return
            "Framebuffer incomplete, attached images must have same format";

        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
            return "Framebuffer incomplete, missing draw buffer";

        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
            return "Framebuffer incomplete, missing read buffer";
#endif
        default:
            assert(0);
            return 0;
    }
}


/*
    coord!/texture!
    TODO: [size 'depth]
    TODO: [size 'shadow] to replace shadowmap?
*/
int fbo_make( UThread* ut, const UCell* from, UCell* res )
{
    GLint dim[2];
    GLuint fbName;
    GLuint depthName = 0;
    GLuint texName;
    const char* err;
    UAtom sel = 0;


    if( ur_is(from, UT_TEXTURE) )
    {
        fbName = glid_genFramebuffer();
        texName = ur_texId(from);

        if( sel == UR_ATOM_DEPTH )
        {
#ifdef GL_ES_VERSION_2_0
            return ur_error( ut, UR_ERR_SCRIPT,
                "make depth framebuffer! cannot get texture! size with GLES2" );
#else
            glBindTexture( GL_TEXTURE_2D, texName );
            glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                                      dim );
            glGetTexLevelParameteriv( GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                                      dim + 1 );
#endif
        }
    }
    else if( ur_is(from, UT_COORD) )
    {
        TextureDef def;

        fbName = glid_genFramebuffer();

        _texCoord( &def, from );

        texName = glid_genTexture();
        glBindTexture( GL_TEXTURE_2D, texName );

        glTexImage2D( GL_TEXTURE_2D, 0, def.comp, def.width, def.height,
                      0, def.format, GL_UNSIGNED_BYTE, 0 );
        /*
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, def.wrap );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, def.wrap );
        */
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

        if( sel == UR_ATOM_DEPTH )
        {
            dim[0] = def.width;
            dim[1] = def.height;
        }
    }
    else
    {
        return ur_error( ut, UR_ERR_TYPE,
                         "make framebuffer! expected texture!/coord!" );
    }

    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fbName );

    glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT,
                               GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                               texName, 0 );

    if( sel == UR_ATOM_DEPTH )
    {
        depthName = glid_genRenderbuffer();
        glBindRenderbufferEXT( GL_RENDERBUFFER_EXT, depthName );
        glRenderbufferStorageEXT( GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT,
                                  dim[0], dim[1] );
        glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT,
                    GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, depthName );
    }

    if( (err = _framebufferStatus()) )
        return ur_error( ut, UR_ERR_INTERNAL, err );

    glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

    ur_setId( res, UT_FBO );
    ur_fboId(res)    = fbName;
    ur_fboRenId(res) = depthName;
    ur_fboTexId(res) = texName;

    return UR_OK;
}


static const UCell*
fbo_select( UThread* ut, const UCell* cell, const UCell* sel, UCell* res )
{
    if( ur_is(sel, UT_WORD) )
    {
        switch( ur_atom(sel) )
        {
        case UR_ATOM_RASTER:
            textureToRaster( ut, GL_TEXTURE_2D, ur_fboTexId(cell), res );
            return res;

        case UR_ATOM_TEXTURE:
            ur_setId(res, UT_TEXTURE);
            ur_texId(res)   = ur_fboTexId(cell);
            ur_texRast(res) = 0;
            return res;
        }
    }
    ur_error( ut, UR_ERR_SCRIPT, "fbo select expected raster/texture" );
    return 0;
}


static void fbo_mark( UThread* ut, UCell* cell )
{
    (void) ut;
    glid_gcMarkFramebuffer( ur_fboId(cell) );
    glid_gcMarkRenderbuffer( ur_fboRenId(cell) );
    glid_gcMarkTexture( ur_fboTexId(cell) );
}


//----------------------------------------------------------------------------
// UT_VBO
/*
  UBuffer is only 12 bytes on 32-bit CPUs so we only have room for two
  GL buffer names.

  UBuffer members:
    type        UT_VBO
    elemSize    Number of GL buffers (1 or 2)
    form        Unused
    flags       Unused
    used        GL buffer 1 (GL_ARRAY_BUFFER)
    ptr.v       GL buffer 2 (GL_ELEMENT_ARRAY_BUFFER)
*/


/**
  \return UBuffer index.
*/
UIndex ur_makeVbo( UThread* ut, GLenum attrUsage, int acount, float* attr,
                   int icount, uint16_t* indices )
{
    GLuint* gbuf;
    UBuffer* buf;
    UIndex bufN;
    int nbuf = 1;

    if( acount && icount )
        ++nbuf;

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );

    memSet( buf, 0, sizeof(UBuffer) );
    buf->type  = UT_VBO;
    vbo_count(buf) = nbuf;

    gbuf = vbo_bufIds(buf);
    glGenBuffers( nbuf, gbuf );

    if( acount )
    {
        glBindBuffer( GL_ARRAY_BUFFER, *gbuf++ );
        glBufferData( GL_ARRAY_BUFFER, sizeof(float) * acount,
                      attr, attrUsage );
    }

    if( icount )
    {
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, *gbuf );
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * icount,
                      indices, GL_STATIC_DRAW );
    }

    return bufN;
}


/*
  int!    - GL_ARRAY_BUFFER of given size
  vector! - GL_ARRAY_BUFFER of given data.
  coord!  - GL_ARRAY_BUFFER and GL_ELEMENT_ARRAY_BUFFER of given sizes.
  [int!/coord!/vector! dynamic | static | stream]
*/
int vbo_make( UThread* ut, const UCell* from, UCell* res )
{
    UIndex n;
    GLenum usage = GL_STATIC_DRAW;

    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;
        ur_blkSlice( ut, &bi, from );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_WORD) )
            {
                switch( ur_atom(bi.it) )
                {
                    case UR_ATOM_DYNAMIC:
                        usage = GL_DYNAMIC_DRAW;
                        break;
                    case UR_ATOM_STATIC:
                        usage = GL_STATIC_DRAW;
                        break;
                    case UR_ATOM_STREAM:
                        usage = GL_STREAM_DRAW;
                        break;
                    default:
                        from = ur_wordCell( ut, bi.it );
                        if( ! from )
                            return UR_THROW;
                        break;
                }
            }
            else
            {
                from = bi.it;
            }
        }
    }

    if( ur_is(from, UT_INT) )
    {
        n = ur_makeVbo( ut, usage, ur_int(from), NULL, 0, NULL );
        goto build;
    }
    else if( ur_is(from, UT_COORD) )
    {
        n = ur_makeVbo( ut, usage, from->coord.n[0], NULL,
                                   from->coord.n[1], NULL );
        goto build;
    }
    else if( ur_is(from, UT_VECTOR) )
    {
        const UBuffer* buf = ur_bufferSer(from);
        if( buf->form == UR_VEC_F32 )
        {
            n = ur_makeVbo( ut, usage, buf->used, buf->ptr.f, 0, NULL );
            goto build;
        }
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "make vbo! make expected int!/coord!/vector!" );

build:

    ur_setId( res, UT_VBO );
    ur_setSeries( res, n, 0 );
    return UR_OK;
}


static void vbo_mark( UThread* ut, UCell* cell )
{
    ur_markBuffer( ut, ur_vboResN(cell) );
}


static void vbo_destroy( UBuffer* buf )
{
    if( vbo_count(buf) )
        glDeleteBuffers( vbo_count(buf), vbo_bufIds(buf) );
}


//----------------------------------------------------------------------------
// UT_QUAT


//extern int vector_pickFloatV( const UBuffer*, UIndex n, float*, int count );

static int quat_make( UThread* ut, const UCell* from, UCell* res )
{
    switch( ur_type(from) )
    {
        case UT_NONE:
            ur_setId(res, UT_QUAT);
            quat_setIdentity( res );
            break;

        case UT_COORD:
            ur_setId(res, UT_QUAT);
            quat_fromEuler( res,
            //quat_fromSpherical( res,
                    degToRad( from->coord.n[0] ),
                    degToRad( from->coord.n[1] ),
                    degToRad( from->coord.n[2] ) );
            break;

        case UT_VEC3:
            ur_setId(res, UT_QUAT);
            quat_fromXYZ( res, from->vec3.xyz );
            break;

        case UT_QUAT:
            *res = *from;
            break;
#if 0
        case UT_VECTOR:
        {
            float n[4];
            int len;
            len = vector_pickFloatV(ur_bufferSer(from), from->series.it, n, 4);
            if( len != 4 )
                return ur_error( ut, UR_ERR_SCRIPT,
                             "make quat! expected vector! with 4 elements" );
            ur_setId(res, UT_QUAT);
            res->vec3.xyz[0] = n[0];
            res->vec3.xyz[1] = n[1];
            res->vec3.xyz[2] = n[2];
            quat_setW( res, n[3] );
        }
            break;
#endif
        default:
            return ur_error( ut, UR_ERR_TYPE,
                    "make quat! expected none!/coord!/vec3!/quat!" );
    }
    return UR_OK;
}


static int quat_compare( UThread* ut, const UCell* a, const UCell* b, int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            // Fall through...

        case UR_COMPARE_SAME:
        {
            const float* pa = a->vec3.xyz;
            const float* pb = b->vec3.xyz;
            if( (pa[0] != pb[0]) || (pa[1] != pb[1]) || (pa[2] != pb[2]) )
                return 0;
            if( (ur_quatW(a) != ur_quatW(b)) ||
                (ur_flags(a, UR_FLAGS_QUAT) !=
                 ur_flags(b, UR_FLAGS_QUAT)) )
                return 0;
            return 1;
        }
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            break;
    }
    return 0;
}


static int quat_operate( UThread* ut, const UCell* a, const UCell* b,
                         UCell* res, int op )
{
    if( ur_is( a, UT_QUAT ) )
    {
        if( op != UR_OP_MUL )
            return unset_operate( ut, a, b, res, op );

        if( ur_is( b, UT_QUAT ) )
        {
            ur_setId( res, UT_QUAT );
            quat_mul( a, b, res );
            return UR_OK;
        }
        else if( ur_is( b, UT_VEC3 ) )
        {
            // Rotate vector.
            UCell conj;

            ur_setId( res, UT_VEC3 );   // w = 0
            res->vec3.xyz[0] = b->vec3.xyz[0];
            res->vec3.xyz[1] = b->vec3.xyz[1];
            res->vec3.xyz[2] = b->vec3.xyz[2];
            ur_normalize( res->vec3.xyz );

            conj.id = a->id;
            conj.vec3.xyz[0] = - a->vec3.xyz[0];
            conj.vec3.xyz[1] = - a->vec3.xyz[1];
            conj.vec3.xyz[2] = - a->vec3.xyz[2];

            quat_mul( res, &conj, res );
            quat_mul( a, res, res );
            return UR_OK;
        }
        else if( ur_is( b, UT_DECIMAL ) )
        {
            double d = ur_decimal(b);
            // Multiply by -1.0 to conjugate (invert) the quaternion.
            res->id = a->id;    // Keep w.
            res->vec3.xyz[0] = a->vec3.xyz[0] * d;
            res->vec3.xyz[1] = a->vec3.xyz[1] * d;
            res->vec3.xyz[2] = a->vec3.xyz[2] * d;
            return UR_OK;
        }
    }
    return ur_error( ut, UR_ERR_TYPE,
            "quat! operator exepected quat! and decimal!/vec3!/quat!" );
}


extern void vec3_toString( UThread*, const UCell*, UBuffer*, int );

static
void quat_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    ur_strAppendCStr( str, "make quat! " );
    vec3_toString( ut, cell, str, depth );
}


static
void quat_toText( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    vec3_toString( ut, cell, str, depth );
    ur_strAppendChar( str, ',' );
    ur_strAppendDouble( str, quat_w(cell) ); 
}


//----------------------------------------------------------------------------
// UT_WIDGET


static int widget_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        if( ! gui_makeWidgets( ut, from, 0, res ) )
            return UR_THROW;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "widget make expected block" );
}


static int widget_compare( UThread* ut, const UCell* a, const UCell* b,
                           int test )
{
    (void) ut;
    switch( test )
    {
        case UR_COMPARE_EQUAL:
        case UR_COMPARE_EQUAL_CASE:
            if( ur_type(a) != ur_type(b) )
                break;
            // Fall through...

        case UR_COMPARE_SAME:
            if( ur_widgetPtr(a) == ur_widgetPtr(b) )
                return 1;
            break;

        case UR_COMPARE_ORDER:
        case UR_COMPARE_ORDER_CASE:
            // Could sort by depth in child hierarchy.
            break;
    }
    return 0;
}


static const UCell*
widget_select( UThread* ut, const UCell* cell, const UCell* sel, UCell* res )
{
    if( ur_is(sel, UT_WORD) )
    {
        GWidget* wp = ur_widgetPtr(cell);
        UAtom atom = ur_atom(sel);
        if( atom == UR_ATOM_PARENT )
        {
            if( wp->parent )
            {
                ur_setId(res, UT_WIDGET);
                ur_widgetPtr(res) = wp->parent;
            }
            else
            {
                ur_setId(res, UT_NONE);
            }
            return res;
        }
        else if( atom == UR_ATOM_ROOT )
        {
            ur_setId(res, UT_WIDGET);
            ur_widgetPtr(res) = gui_root( wp );
            return res;
        }
        return wp->wclass->select( wp, atom, res ) ? res : 0;
    }
    ur_error( ut, UR_ERR_SCRIPT, "widget select expected word!" );
    return 0;
}


static void widget_recycle( UThread* ut, int phase )
{
    if( ut == glEnv.guiUT )
    {
        GWidget** it  = (GWidget**) glEnv.rootWidgets.ptr.v;
        GWidget** end = it + glEnv.rootWidgets.used;
        switch( phase )
        {
            case UR_RECYCLE_MARK:
                while( it != end )
                {
                    (*it)->flags |= GW_RECYCLE;
                    ++it;
                }
                break;

            case UR_RECYCLE_SWEEP:
            {
                GWidget** start  = it;
                GWidget** packIt = 0;
                while( it != end )
                {
                    if( (*it)->flags & GW_RECYCLE )
                    {
                        gui_freeWidget( *it );
                        if( ! packIt )
                            packIt = it;
                    }
                    else if( packIt )
                    {
                        *packIt++ = *it;
                    }
                    ++it;
                }
                if( packIt )
                    glEnv.rootWidgets.used = packIt - start;
            }
                break;
        }
    }
}
 

static void widget_mark( UThread* ut, UCell* cell )
{
    GWidget* wp = ur_widgetPtr(cell);
    if( wp->flags & GW_DESTRUCT )
    {
        ur_setId(cell, UT_NONE);
        ur_widgetPtr(cell) = 0;
    }
    else
    {
        wp = gui_root( wp );
        if( wp && (wp->flags & GW_RECYCLE) )
        {
            GMarkFunc func = wp->wclass->mark;
            if( func )
                func( ut, wp );
            wp->flags &= ~GW_RECYCLE;
        }
    }
}


//----------------------------------------------------------------------------


UDatatype gl_types[] =
{
  {
    "draw-prog!",
    dprog_make,             unset_make,             unset_copy,
    dprog_compare,          unset_operate,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          dprog_mark,             dprog_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "raster!",
    raster_make,            raster_make,            binary_copy,
    unset_compare,          unset_operate,          raster_select,
    unset_toString,         unset_toText,
    unset_recycle,          binary_mark,            ur_binFree,
    unset_markBuf,          binary_toShared,        unset_bind
  },
  {
    "texture!",
    texture_make,           texture_make,           unset_copy,
    unset_compare,          unset_operate,          texture_select,
    unset_toString,         unset_toText,
    texture_recycle,        texture_mark,           unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "font!",
    rfont_make,             rfont_make,             unset_copy,
    unset_compare,          unset_operate,          rfont_select,
    unset_toString,         unset_toText,
    unset_recycle,          rfont_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "shader!",
    shader_make,            shader_make,            unset_copy,
    unset_compare,          unset_operate,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          binary_mark,            shader_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "fbo!",
    fbo_make,               unset_make,             unset_copy,
    unset_compare,          unset_operate,          fbo_select,
    unset_toString,         unset_toText,
    unset_recycle,          fbo_mark,               unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "vbo!",
    vbo_make,               unset_make,             unset_copy,
    unset_compare,          unset_operate,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          vbo_mark,               vbo_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "quat!",
    quat_make,              quat_make,              unset_copy,
    quat_compare,           quat_operate,           unset_select,
    quat_toString,          quat_toText,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "widget!",
    widget_make,            unset_make,             unset_copy,
    widget_compare,         unset_operate,          widget_select,
    unset_toString,         unset_toText,
    widget_recycle,         widget_mark,            unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  }
};


//EOF
