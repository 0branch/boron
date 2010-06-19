/*
  Copyright 2010 Karl Robillard

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


extern void binary_copy( UThread*, const UCell* from, UCell* res );
extern void binary_mark( UThread*, UCell* cell );
extern void binary_destroy( UBuffer* buf );
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
        UBuffer* buf;
        UIndex n;
        int ok;

        n = ur_makeDrawProg( ut );
        buf = ur_buffer( n );

        ur_setId(res, UT_DRAWPROG);
        ur_setSeries(res, n, 0 );

        save = ur_beginDP( &dpc );
        ok = ur_compileDP( ut, from, 0 );
        ur_endDP( ut, buf, save );
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


static int _rasterBinarySize( int format, int depth, int w, int h )
{
    int size = depth / 8 * w * h;
    if( format == UR_RAST_RGB )
        size *= 3;
    else if( format == UR_RAST_RGBA )
        size *= 4;
    return size + sizeof(RasterHead);
}


/**
  \return Binary index and initialied res.  If binp is non-zero, it is set.
*/
UBuffer* ur_makeRaster( UThread* ut, int format, int w, int h, UCell* res )
{
    UIndex binN;
    UBuffer* bin;
    RasterHead* rh;
    int size;


    size = _rasterBinarySize( format, 8, w, h );
    binN = ur_makeBinary( ut, size );
    bin = ur_buffer( binN );
    bin->used = size;

    rh = (RasterHead*) bin->ptr.v;
    rh->format = format;
    rh->depth  = 8;
    rh->width  = w;
    rh->height = h;

    ur_setId( res, UT_RASTER );
    ur_setSeries( res, binN, 0 );

    return bin;
}


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


static
int raster_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    const RasterHead* rh;

    switch( ur_atom(bi->it) )
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
            return ur_error( ut, UR_ERR_SCRIPT, "Invalid select!" );
    }
    ++bi->it;
    return UR_OK;
}


//----------------------------------------------------------------------------
// UT_TEXTURE


typedef struct
{
    const void* pixels;
    int    width;
    int    height;
    GLenum format;
    GLint  comp;
    GLint  wrap;
    GLint  min_filter;
    GLint  mag_filter;
}
TextureDef;


static void _texRast( TextureDef* tex, const RasterHead* rh )
{
    tex->width  = rh->width;
    tex->height = rh->height;
    tex->pixels = rh + 1;

    switch( rh->format )
    {
        case UR_RAST_GRAY:
            tex->comp   = 1;
            tex->format = GL_LUMINANCE;
            break;

        case UR_RAST_RGB:
            tex->comp   = 3;
            tex->format = GL_RGB;
            break;

        case UR_RAST_RGBA:
        default:
            tex->comp   = 4;
            tex->format = GL_RGBA;
            break;
    }
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
    tex->comp   = 4;
    tex->format = GL_RGBA;

    if( cell->coord.len > 2 )
    {
        if( cell->coord.n[2] == 3 )
        {
            tex->comp   = 3;
            tex->format = GL_RGB;
        }
        else if( cell->coord.n[2] == 1 )
        {
            tex->comp   = 1;
            tex->format = GL_LUMINANCE;
        }
    }
}


/*
  (texture! raster -- tex)
  (texture! coord  -- tex)    ; 2D Texture
  (texture! int    -- tex)    ; 1D Texture
  (texture! [raster!/coord!/int!
             binary!
             'mipmap 'nearest 'linear 'repeat 'clamp
             'gray 'rgb' 'rgba ] -- tex)
*/
int texture_make( UThread* ut, const UCell* from, UCell* res )
{
    TextureDef def;
    GLuint name;
    int mipmap = 0;
    UIndex rastN = 0;
    GLenum target = GL_TEXTURE_2D;


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
    else if( ur_is(from, UT_INT) )
    {
        target = GL_TEXTURE_1D;
        _texCoord( &def, from );
        goto build;
    }
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
                    else if( ur_is(val, UT_INT) )
                    {
                        target = GL_TEXTURE_1D;
                        _texCoord( &def, val );
                    }
                    break;

                case UT_LITWORD:
                    switch( ur_atom( bi.it ) )
                    {
                        case UR_ATOM_CLAMP:
                            def.wrap = GL_CLAMP;
                            break;

                        case UR_ATOM_REPEAT:
                            def.wrap = GL_REPEAT;
                            break;

                        case UR_ATOM_NEAREST:
                            def.min_filter = GL_NEAREST;
                            def.mag_filter = GL_NEAREST;
                            break;

                        case UR_ATOM_LINEAR:
                            def.min_filter = GL_LINEAR;
                            def.mag_filter = GL_LINEAR;
                            break;

                        case UR_ATOM_MIPMAP:
                            mipmap = 1;
                            break;

                        case UR_ATOM_GRAY:
                            def.comp   = 1;
                            def.format = GL_LUMINANCE;
                            break;

                        case UR_ATOM_RGB:
                            def.comp   = 3;
                            def.format = GL_RGB;
                            break;

                        case UR_ATOM_RGBA:
                            def.comp   = 4;
                            def.format = GL_RGBA;
                            break;
                    }
                    break;

                case UT_INT:
                    target = GL_TEXTURE_1D;
                    _texCoord( &def, bi.it );
                    break;

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

    // TODO: Support texture type of GL_FLOAT.

    if( target == GL_TEXTURE_2D )
    {
        if( mipmap )
        {
            gluBuild2DMipmaps( GL_TEXTURE_2D, def.comp, def.width, def.height,
                               def.format, GL_UNSIGNED_BYTE, def.pixels );
        }
        else
        {
            // Ensure that a non-mipmap minifying filter is used.
            // Using a mipmap filter can cause FBO format errors and textures
            // to not appear.
            if( def.min_filter != GL_NEAREST )
                def.min_filter = GL_LINEAR;

            glTexImage2D( GL_TEXTURE_2D, 0, def.comp, def.width, def.height,
                          0, def.format, GL_UNSIGNED_BYTE, def.pixels );
        }
    }
    else
    {
        if( mipmap )
        {
            gluBuild1DMipmaps( GL_TEXTURE_1D, def.comp, def.width,
                               def.format, GL_UNSIGNED_BYTE, def.pixels );
        }
        else
        {
            if( def.min_filter != GL_NEAREST )
                def.min_filter = GL_LINEAR;

            glTexImage1D( GL_TEXTURE_1D, 0, def.comp, def.width,
                          0, def.format, GL_UNSIGNED_BYTE, def.pixels );
        }
    }

    {
        GLenum err = glGetError();
        if( err != GL_NO_ERROR )
        {
            return ur_error( ut, UR_ERR_INTERNAL,
                             (const char*) gluErrorString( err ) );
        }
    }

    // GL_DECAL, GL_MODULATE, GL_REPLACE
    //glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

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
    UBuffer* bin;
    GLint dim[ 2 ];

    glBindTexture( target, name );
    glGetTexLevelParameteriv( target, 0, GL_TEXTURE_WIDTH,  dim );
    glGetTexLevelParameteriv( target, 0, GL_TEXTURE_HEIGHT, dim + 1 );

    bin = ur_makeRaster( ut, UR_RAST_RGB, dim[0], dim[1], res );
    if( bin->ptr.b )
    {
        glGetTexImage( target, 0, GL_RGB, GL_UNSIGNED_BYTE, ur_rastElem(bin) );
    }
}


static
int texture_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    if( ur_is(bi->it, UT_WORD) )
    {
        if( ur_atom(bi->it) == UR_ATOM_RASTER )
        {
            UIndex ri = ur_texRast(cell);
            if( ri )
            {
                ur_setId(res, UT_RASTER);
                ur_setSeries(res, ri, 0);
            }
            else
            {
                textureToRaster( ut, GL_TEXTURE_2D, ur_texId(cell), res );
            }
            ++bi->it;
            return UR_OK;
        }
        return raster_select( ut, cell, bi, res );
    }
    return ur_error( ut, UR_ERR_SCRIPT, "Texture select expected word!" );
}


static void texture_recycle( UThread* ut, int phase )
{
    (void) ut;
    switch( phase )
    {
        case UR_RECYCLE_MARK:
        {
            // Clear any unchecked GL error.
#ifdef DEBUG
            GLenum err = glGetError();
            if( err )
                printf( "recycle_texture - glGetError() returned %d\n", err );
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


static
int rfont_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    if( ur_is(bi->it, UT_WORD) )
    {
        switch( ur_atom(bi->it) )
        {
            case UR_ATOM_BINARY:
                ur_setId(res, UT_BINARY);
                ur_setSeries(res, ur_fontTF(cell), 0);
                break;

            case UR_ATOM_RASTER:
                ur_setId(res, UT_RASTER);
                ur_setSeries(res, ur_fontRast(cell), 0);
                break;

            case UR_ATOM_TEXTURE:
                ur_setId(res, UT_TEXTURE);
                ur_texId(res)   = ur_fontTexId(cell);
                ur_texRast(res) = ur_fontRast(cell);
                break;

            default:
                goto error;
        }
        ++bi->it;
        return UR_OK;
    }

error:

    return ur_error( ut, UR_ERR_SCRIPT,
                     "Font select expected binary, raster, or texture" );
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


#if 0
//----------------------------------------------------------------------------
// UT_PORT
/*
  UBuffer members:
    type        UT_PORT
    elemSize    Unused
    form        UR_PORT_SIMPLE, UR_PORT_EXT
    flags       Unused
    used        Unused
    ptr.v       UPortDevice* or UPortDevice**
*/


int ur_makePort( UThread* ut, UPortDevice* pdev, const UCell* from, UCell* res )
{
    UBuffer* buf;
    UIndex bufN;

    ur_genBuffers( ut, 1, &bufN );
    buf = ur_buffer( bufN );

    buf->type  = UT_PORT;
    buf->form  = UR_PORT_SIMPLE;
    buf->ptr.v = pdev;

    if( ! pdev->open( ut, buf, from ) )
    {
        //buf = ur_buffer( bufN );    // Re-aquire
        buf->ptr.v = 0;
        return UR_THROW;
    }

    ur_setId(res, UT_PORT);
    ur_setSeries(res, bufN, 0);
    return UR_OK;
}


int port_make( UThread* ut, const UCell* from, UCell* res )
{
    UPortDevice* pdev = 0;
    UAtom name = 0;

    switch( ur_type(from) )
    {
        case UT_STRING:
        {
            const char* cp;
            const char* url = boron_cstr( ut, from, 0 );

            for( cp = url; *cp; ++cp )
            {
                if( cp[0] == ':' && cp[1] == '/' && cp[2] == '/' )
                {
                    name = ur_internAtom( ut, url, cp );
                    break;
                }
            }
            if( ! name )
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "make port! expected URL" );
            pdev = portLookup( ut, name );
        }
            break;

        case UT_FILE:
            pdev = &port_file;
            break;

        case UT_BLOCK:
        {
            UBlockIter bi;
            ur_blkSlice( ut, &bi, from );
            if( (bi.it == bi.end) || ! ur_is(bi.it, UT_WORD) )
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "make port! expected device word in block" );
            pdev = portLookup( ut, name = ur_atom(bi.it) );
        }
            break;

        default:
            return ur_error( ut, UR_ERR_TYPE,
                             "make port! expected string!/file!/block!" );
    }

    if( ! pdev )
        return ur_error( ut, UR_ERR_SCRIPT, "Port type %s does not exist",
                         ur_atomCStr(ut, name) );

    return ur_makePort( ut, pdev, from, res );
}


void port_destroy( UBuffer* buf )
{
    if( buf->ptr.v )
    {
        UPortDevice* dev = (buf->form == UR_PORT_SIMPLE) ?
            (UPortDevice*) buf->ptr.v : *((UPortDevice**) buf->ptr.v);
        dev->close( buf );
    }
}
#endif


//----------------------------------------------------------------------------


void ur_arrAppendPtr( UBuffer* arr, void* ptr )
{
    ur_arrReserve( arr, arr->used + 1 );
    ((void**) arr->ptr.v)[ arr->used ] = ptr;
    ++arr->used;
}


static int widget_make( UThread* ut, const UCell* from, UCell* res )
{
    if( ur_is(from, UT_BLOCK) )
    {
        UBlockIter bi;
        GWidgetClass* wclass;
        GWidget* wp = 0;

        ur_blkSlice( ut, &bi, from );
        //ur_foreach( bi )
        while( bi.it != bi.end )
        {
            if( ! ur_is(bi.it, UT_WORD) )
                return ur_error( ut, UR_ERR_TYPE,
                                 "widget make expected widget name" );
            wclass = ur_widgetClass( ur_atom(bi.it) );
            if( ! wclass )
                return ur_error( ut, UR_ERR_SCRIPT, "unknown widget class" );
            wp = wclass->make( ut, &bi );
            if( ! wp )
                return UR_THROW;
        }

        ur_arrAppendPtr( &glEnv.rootWidgets, wp );

        ur_setId(res, UT_WIDGET);
        ur_widgetPtr(res) = wp;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "widget make expected block" );
}


static
int widget_select( UThread* ut, const UCell* cell, UBlockIter* bi, UCell* res )
{
    if( ur_is(bi->it, UT_WORD) )
    {
        GWidget* wp = ur_widgetPtr(cell);
        UAtom atom = ur_atom(bi->it);
        ++bi->it;
        return wp->wclass->select( wp, atom, res );
    }
    return ur_error( ut, UR_ERR_SCRIPT, "widget select expected word!" );
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
    wp = gui_root( wp );
    if( wp )
    {
        GMarkFunc func = wp->wclass->mark;
        if( func )
            func( ut, wp );
        wp->flags &= ~GW_RECYCLE;
    }
}


//----------------------------------------------------------------------------


UDatatype gl_types[] =
{
  {
    "draw-prog!",
    dprog_make,             unset_make,             unset_copy,
    dprog_compare,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          dprog_mark,             dprog_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "raster!",
    raster_make,            raster_make,            binary_copy,
    unset_compare,          raster_select,
    unset_toString,         unset_toText,
    unset_recycle,          binary_mark,            binary_destroy,
    unset_markBuf,          binary_toShared,        unset_bind
  },
  {
    "texture!",
    texture_make,           texture_make,           unset_copy,
    unset_compare,          texture_select,
    unset_toString,         unset_toText,
    texture_recycle,        texture_mark,           unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "font!",
    rfont_make,             rfont_make,             unset_copy,
    unset_compare,          rfont_select,
    unset_toString,         unset_toText,
    unset_recycle,          rfont_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "shader!",
    shader_make,            shader_make,            unset_copy,
    unset_compare,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          binary_mark,            shader_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "fbo!",
    unset_make,             unset_make,             unset_copy,
    unset_compare,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "vbo!",
    unset_make,             unset_make,             unset_copy,
    unset_compare,          unset_select,
    unset_toString,         unset_toText,
    unset_recycle,          unset_mark,             unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  },
  {
    "widget!",
    widget_make,            unset_make,             unset_copy,
    unset_compare,          widget_select,
    unset_toString,         unset_toText,
    widget_recycle,         widget_mark,            unset_destroy,
    unset_markBuf,          unset_toShared,         unset_bind
  }
};


//EOF
