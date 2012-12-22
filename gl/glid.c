/*
  Boron OpenGL ID Collector
  Copyright 2009-2012 Karl Robillard

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


#include <stdint.h>
#include "urlan.h"
#include "os.h"


#ifdef __APPLE__
#include <OpenGL/gl.h>
#elif defined(__ANDROID__)
#include <GLES2/gl2.h>
#define glGenRenderbuffersEXT       glGenRenderbuffers
#define glGenFramebuffersEXT        glGenFramebuffers
#define glDeleteRenderbuffersEXT    glDeleteRenderbuffers
#define glDeleteFramebuffersEXT     glDeleteFramebuffers
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#endif


struct GLIdRecord
{
    UBuffer tex;        // Textures
    UBuffer ren;        // Renderbuffers
    UBuffer fbo;        // Framebuffer Objects
    UBuffer texMark;
    UBuffer renMark;
    UBuffer fboMark;
    int     unsorted;
};

#define TEX_UNSORTED    1
#define REN_UNSORTED    2
#define FBO_UNSORTED    4

static struct GLIdRecord _glid;


void glid_startup()
{
    ur_arrInit( &_glid.tex, sizeof(uint32_t), 64 );
    ur_arrInit( &_glid.ren, sizeof(uint32_t),  0 );
    ur_arrInit( &_glid.fbo, sizeof(uint32_t),  0 );
    ur_binInit( &_glid.texMark, 0 );
    ur_binInit( &_glid.renMark, 0 );
    ur_binInit( &_glid.fboMark, 0 );
    _glid.unsorted = 0;
}


void glid_shutdown()
{
    ur_arrFree( &_glid.tex );
    ur_arrFree( &_glid.ren );
    ur_arrFree( &_glid.fbo );
    ur_binFree( &_glid.texMark );
    ur_binFree( &_glid.renMark );
    ur_binFree( &_glid.fboMark );
}


static void glid_store( UBuffer* arr, GLuint id, int unsortedMask )
{
    uint32_t* end;

    ur_arrReserve( arr, arr->used + 1 );
    end = arr->ptr.u32 + arr->used;

    if( arr->used && (id < end[-1]) )
        _glid.unsorted |= unsortedMask;

    *end = id;
    ++arr->used;
}


#if 0
void glid_storeTexture( GLuint name )
{
    glid_store( &_glid.tex, name, TEX_UNSORTED );
}

void glid_storeRenderbuffer( GLuint name )
{
    glid_store( &_glid.ren, name, REN_UNSORTED );
}

void glid_storeFramebuffer( GLuint name )
{
    glid_store( &_glid.fbo, name, FBO_UNSORTED );
}
#endif


GLuint glid_genTexture()
{
    GLuint name;
    glGenTextures( 1, &name );
    glid_store( &_glid.tex, name, TEX_UNSORTED );
    return name;
}


GLuint glid_genRenderbuffer()
{
    GLuint name;
    glGenRenderbuffersEXT( 1, &name );
    glid_store( &_glid.ren, name, REN_UNSORTED );
    return name;
}


GLuint glid_genFramebuffer()
{
    GLuint name;
    glGenFramebuffersEXT( 1, &name );
    glid_store( &_glid.fbo, name, FBO_UNSORTED );
    return name;
}


#define QS_SWAP(a,b) \
    stmp = ids[a]; \
    ids[a] = ids[b]; \
    ids[b] = stmp

// Quicksort
static void glid_sort( uint32_t* ids, int low, int high )
{
    int i, j;
    uint32_t val;
    uint32_t stmp;

    if( low >= high )
        return;

    val = ids[low];
    i = low;
    j = high+1;
    for(;;)
    {
        do i++; while( i <= high && ids[i] < val );
        do j--; while( ids[j] > val );
        if( i > j )
            break;
        QS_SWAP( i, j );
    }
    QS_SWAP( low, j );
    glid_sort( ids, low, j-1 );
    glid_sort( ids, j+1, high );
}


static void glid_clearMark( UBuffer* mark, int n )
{
    int bn = (n + 7) >> 3;
    ur_binReserve( mark, bn );
    mark->used = bn;
    memSet( mark->ptr.b, 0, bn );

    // Mark excess bits as used for glid_sweep.
    n &= 7;
    if( n )
        mark->ptr.b[ bn - 1 ] = 0xff ^ (0xff >> (8 - n));
}


void glid_gcBegin()
{
    if( _glid.unsorted )
    {
        if( _glid.unsorted & TEX_UNSORTED )
            glid_sort( _glid.tex.ptr.u32, 0, _glid.tex.used - 1 );
        if( _glid.unsorted & REN_UNSORTED )
            glid_sort( _glid.ren.ptr.u32, 0, _glid.ren.used - 1 );
        if( _glid.unsorted & FBO_UNSORTED )
            glid_sort( _glid.fbo.ptr.u32, 0, _glid.fbo.used - 1 );
        _glid.unsorted = 0;
    }

    glid_clearMark( &_glid.texMark, _glid.tex.used );
    glid_clearMark( &_glid.renMark, _glid.ren.used );
    glid_clearMark( &_glid.fboMark, _glid.fbo.used );
}


static void glid_sweep( UBuffer* idArr, UBuffer* markArr,
                       void (*delFunc)(GLsizei, const GLuint*) )
{
#define FREE_ID 0xffffffff
    uint32_t* ids;
    uint32_t* firstFreeId = 0;
    uint8_t* it;
    uint8_t* end;
    int markBits;
    int mask;

    ids = idArr->ptr.u32;
    it  = markArr->ptr.b,
    end = it + markArr->used;

    // Delete unused ids.
    while( it != end )
    {
        markBits = *it++;
        if( markBits != 0xff )
        {
            for( mask = 1; mask < 0x100; mask <<= 1 )
            {
                if( ! (markBits & mask) )
                {
                    //printf( "KR delFunc(1,%d)\n", *ids );
                    delFunc( 1, ids );
                    *ids = FREE_ID;
                    if( ! firstFreeId )
                        firstFreeId = ids;
                }
                ++ids;
            }
        }
        else
        {
            ids += 8;
        }
    }

    // Compact id record.
    if( firstFreeId )
    {
        uint32_t* idEnd;

        ids = firstFreeId + 1;
        idEnd = idArr->ptr.u32 + idArr->used;
        while( ids != idEnd )
        {
            if( *ids != FREE_ID )
                *firstFreeId++ = *ids;
            ++ids;
        }
        idArr->used = firstFreeId - idArr->ptr.u32;
    }
}


void glid_gcSweep()
{
    glid_sweep( &_glid.tex, &_glid.texMark, glDeleteTextures );
    glid_sweep( &_glid.ren, &_glid.renMark, glDeleteRenderbuffersEXT );
    glid_sweep( &_glid.fbo, &_glid.fboMark, glDeleteFramebuffersEXT );
}


/*
  Returns index of id in array or -1 if not found;
*/
static int glid_index( UBuffer* arr, GLuint id )
{
    // Binary search.
    uint32_t sval;
    int mid;
    int low = 0;
    int high = arr->used - 1;

    while( low <= high )
    {
        mid = ((unsigned int) (low + high)) >> 1;
        sval = arr->ptr.u32[ mid ];
        if( sval < id )
            low = mid + 1;
        else if( sval > id )
            high = mid - 1;
        else
            return mid;
    }
    return -1;  // Not found.
}


#define setBit(array,n)      (array[(n)>>3] |= 1<<((n)&7))


void glid_gcMarkTexture( GLuint name )
{
    int i = glid_index( &_glid.tex, name );
    if( i > -1 )
        setBit( _glid.texMark.ptr.b, i );
}


void glid_gcMarkRenderbuffer( GLuint name )
{
    int i = glid_index( &_glid.ren, name );
    if( i > -1 )
        setBit( _glid.renMark.ptr.b, i );
}


void glid_gcMarkFramebuffer( GLuint name )
{
    int i = glid_index( &_glid.fbo, name );
    if( i > -1 )
        setBit( _glid.fboMark.ptr.b, i );
}


/*EOF*/
