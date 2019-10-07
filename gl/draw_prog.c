/*
  Boron OpenGL Draw Program
  Copyright 2008-2011 Karl Robillard

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


#include <stdarg.h>
#include "glh.h"
#include "boron-gl.h"
#include "urlan_atoms.h"
#include "gl_atoms.h"
#include "os.h"
#include "math3d.h"
#include "draw_ops.h"
#include "draw_prog.h"
#include "shader.h"
#include "geo.h"
#include "quat.h"

#define ES_UPDATE_MATRIX \
    if( es_matrixUsed && es_matrixMod ) \
        es_updateUniformMatrix();


// Hardcoded shader uniform locations (requires GLES 3.1).
enum DPUniformLocation
{
    ULOC_TRANSFORM,         // layout(location = 0) uniform mat4 transform;
    ULOC_COLOR              // layout(location = 1) uniform vec4 baseColor;
};


// Hardcoded vertex shader input locations for common attibutes.
enum DPAttribLocation
{
    ALOC_VERTEX,            // layout(location = 0) in vec3 position;
    ALOC_TEXTURE,           // layout(location = 1) in vec2 uv;
    ALOC_COLOR,             // layout(location = 2) in vec3 color;
    ALOC_NORMAL             // layout(location = 3) in vec3 normal;
};


enum DPOpcode
{
    DP_EMPTY,
    DP_NOP,
    DP_END,
    DP_JMP,                 // offset
    DP_SWITCH,              // switch,N offset1 ... offsetN
    DP_RUN_PROGRAM,         // resN
    DP_RUN_PROGRAM_WORD,    // blkN index
    DP_EVAL_BLOCK,          // blkN
    DP_RENDER_WIDGET,       // wid
    DP_CLEAR,
    DP_CAMERA,              // ctxValN
    DP_SHADER,              // glprog
    DP_SHADER_CTX,          // glprog ctxValN
    DP_UNIFORM_1I,          // loc val
    DP_UNIFORM_1F,          // loc val
    DP_UNIFORM_3F,          // loc f0 f1 f2
    DP_BIND_TEXTURE,        // texUnit gltex 
    DP_BIND_ARRAY,          // glbuffer
    DP_BIND_ELEMENTS,       // glbuffer
    DP_VERTEX_OFFSET,       // stride-offset
    DP_VERTEX_OFFSET_4,     // stride-offset
    DP_NORMAL_OFFSET,       // stride-offset
    DP_COLOR_OFFSET,        // stride-offset
    DP_COLOR_OFFSET_4,      // stride-offset
    DP_UV_OFFSET,           // stride-offset
    DP_ATTR_OFFSET,         // location-size stride-offset
    DP_DRAW_ARR_POINTS,     // count
    DP_DRAW_POINTS,         // count
    DP_DRAW_LINES,          // count
    DP_DRAW_LINE_STRIP,     // count
    DP_DRAW_TRIS,           // count
    DP_DRAW_TRI_STRIP,      // count
    DP_DRAW_TRI_FAN,        // count
    DP_DRAW_QUADS,          // count
    DP_DRAW_POINTS_I,       // count offset
    DP_DRAW_LINES_I,        // count offset
    DP_DRAW_LINE_STRIP_I,   // count offset
    DP_DRAW_TRIS_I,         // count offset
    DP_DRAW_TRI_STRIP_I,    // count offset
    DP_DRAW_TRI_FAN_I,      // count offset
    DP_DRAW_QUADS_I,        // count offset
    DP_DEPTH_ON,
    DP_DEPTH_OFF,
    DP_BLEND_ON,
    DP_BLEND_OFF,
    DP_BLEND_ADD,
    DP_BLEND_BURN,
    DP_BLEND_TRANS,
    DP_CULL_ON,
    DP_CULL_OFF,
    DP_COLOR_MASK_ON,
    DP_COLOR_MASK_OFF,
    DP_DEPTH_MASK_ON,
    DP_DEPTH_MASK_OFF,
    DP_POINT_SIZE_ON,
    DP_POINT_SIZE_OFF,
    DP_POINT_SPRITE_ON,
    DP_POINT_SPRITE_OFF,
    DP_COLOR3,              // color
    DP_COLOR4,              // color
    DP_COLOR_WORD,          // blkN index
    DP_PUSH,
    DP_POP,
    DP_TRANSLATE,           // x y z
    DP_TRANSLATE_WORD,      // blkN index
    DP_TRANSLATE_XY,        // x y
    DP_ROTATE_X,            // angle
    DP_ROTATE_Y,            // angle
    DP_ROTATE_Z,            // angle
    DP_ROTATE_X_WORD,       // blkN index
    DP_ROTATE_Y_WORD,       // blkN index
    DP_ROTATE_Z_WORD,       // blkN index
    DP_ROTATE_WORD,         // blkN index
    DP_SCALE_WORD,          // blkN index
    DP_SCALE_1,             // float
    DP_FRAMEBUFFER,         // fbo
    DP_FRAMEBUFFER_TEX_WORD,// blkN index attach
    DP_SHADOW_BEGIN,        // fbo
    DP_SHADOW_END,
    DP_SAMPLES_QUERY,
    DP_SAMPLES_BEGIN,
    DP_SAMPLES_END,         // blkN index
    DP_LIGHT,               // blkN
    DP_READ_PIXELS,         // blkN index pos dim
    DP_FONT,                // blkN
    DP_TEXT_WORD,           // blkN index aoff x y (then DP_DRAW_TRIS_I)
    DP_86,
    DP_87,
    DP_88,
    DP_89,
    DP_90
};


enum DPResourceType
{
    RES_GL_BUFFERS,
    RES_BINARY,
    RES_BLOCK,
  //RES_TEXTURE,
    RES_SHADER,
    RES_VBO,
    RES_DRAW_PROG
};


typedef struct
{
    uint8_t  type;
    uint8_t  count;
    uint16_t _pad;
}
DPResEntry;


typedef struct
{
    uint32_t  uid;
    uint16_t  resCount;
    uint16_t  progIndex;
    /*
    for resCount
        DPResEntry entry;
        int32_t    names[ entry.count ]     // Holds GLuint or UIndex
    uint32_t program[]
    */
}
DPHeader;


typedef union
{
    float f;
    uint32_t i;
}
Number;


#define MAX_DYNAMIC_TEXT_LEN    20

#define dp_byteCode(ph) ((uint32_t*) (((char*) ph) + ph->progIndex))

#define flushOps(opcode) \
    while( emit->opCount ) \
        emitOp( opcode )


/*--------------------------------------------------------------------------*/
/*
  UBuffer members:
    type        UT_DRAWPROG
    elemSize    Unused
    form        Unused
    flags       0, UR_DRAWPROG_HIDDEN
    used        Number of bytes used
    ptr.b       Program
    ptr.i[-1]   Number of bytes available
*/


UIndex ur_makeDrawProg( UThread* ut /*, UBuffer** bp*/ )
{
    UIndex bufN;
    UBuffer* buf;

    ur_genBuffers( ut, 1, &bufN );

    buf = ur_buffer( bufN );
    *((uint32_t*) buf) = 0;
    buf->type  = UT_DRAWPROG;
    buf->used  = 0;
    buf->ptr.b = 0;

    //if( bp )
    //    *bp = buf;
    return bufN;
}


extern void block_markBuf( UThread*, UBuffer* );

void ur_markBlkN( UThread* ut, UIndex blkN )
{
    if( blkN > UR_INVALID_BUF )    // Also acts as (! ur_isShared(n))
    {
        if( ur_markBuffer( ut, blkN ) )
            block_markBuf( ut, ur_buffer(blkN) );
    }
}


void ur_markDrawProg( UThread* ut, UIndex n )
{
    DPHeader* ph;

    if( ! ur_markBuffer( ut, n ) )
        return;

    ph = (DPHeader*) ur_buffer( n )->ptr.v;
    if( ph )
    {
        UIndex* idx;
        UIndex* end;
        DPResEntry* it = (DPResEntry*) (ph + 1);
        int count = ph->resCount;

        while( count-- )
        {
            switch( it->type )
            {
                case RES_GL_BUFFERS:
                    break;

                case RES_BINARY:
                case RES_SHADER:
                case RES_VBO:
                    idx = (UIndex*) (it + 1);
                    end = idx + it->count;
                    while( idx != end )
                        ur_markBuffer( ut, *idx++ );
                    break;

                case RES_BLOCK:
                    idx = (UIndex*) (it + 1);
                    end = idx + it->count;
                    while( idx != end )
                        ur_markBlkN( ut, *idx++ );
                    break;

              //case RES_TEXTURE:
              //    break;

                case RES_DRAW_PROG:
                    idx = (UIndex*) (it + 1);
                    end = idx + it->count;
                    while( idx != end )
                        ur_markDrawProg( ut, *idx++ );
                    break;
            }
            it += it->count + 1;
        }
    }
}


void dprog_destroy( UBuffer* buf )
{
    DPHeader* ph = (DPHeader*) buf->ptr.v;
    if( ph )
    {
        DPResEntry* it = (DPResEntry*) (ph + 1);
        int count = ph->resCount;

        while( count-- )
        {
            switch( it->type )
            {
                case RES_GL_BUFFERS:
                    //printf( "KR glDeleteBuffers\n" );
                    glDeleteBuffers( it->count, (GLuint*) (it + 1) );
                    break;
            }
            it += it->count;
        }

        memFree( ph );
        buf->ptr.v = 0;
    }
}


#define emitOp(op)      emitDPInst(emit, op, 0)
#define emitOp1(op,a)   emitDPInst(emit, op, 1, a)
#define emitOp2(op,a,b) emitDPInst(emit, op, 2, a, b)


static inline uint32_t* dp_addArgs( DPCompiler* emit, int count )
{
    uint32_t* pc;
    UBuffer* arr = &emit->inst;

    ur_arrReserve( arr, arr->used + count );
    pc = arr->ptr.u32 + arr->used;
    arr->used += count;

    return pc;
}


static void emitDPInst( DPCompiler* emit, int opcode, int argc, ... )
{
    emit->ops = (emit->ops >> 8) | (opcode << 24);

    if( argc )
    {
        va_list args;
        uint32_t* pc;

        pc = emit->args + emit->argCount;
        emit->argCount += argc;

        va_start( args, argc );
        while( argc-- )
            *pc++ = va_arg( args, uint32_t );
        va_end( args );
    }

    if( emit->opCount == 3 )
    {
        uint32_t* pc;
        uint32_t* arg;
        uint32_t* end;
        int esize;

        esize = emit->argCount + 1;
        pc = dp_addArgs( emit, esize );

        end = pc + esize;
        arg = emit->args;

        *pc++ = emit->ops;
        while( pc != end )
            *pc++ = *arg++;

        emit->opCount  = 0;
        emit->argCount = 0;
    }
    else
    {
        ++emit->opCount;
    }
}


/*
  Add extra arguments following emitDPInst().
  Caller must not exceed DP_MAX_ARGS.
*/
static void emitDPArg( DPCompiler* emit, uint32_t arg )
{
    if( emit->opCount )
        emit->args[ emit->argCount++ ] = arg;
    else
        *dp_addArgs( emit, 1 ) = arg;
}


/*
  Return offset in bytecode of next argument.
*/
static int nextArgPosition( DPCompiler* emit )
{
    return emit->inst.used + 1 + emit->argCount;
}


static void refBlock( DPCompiler* emit, UIndex n )
{
    UBuffer* arr = &emit->blocks;

    // No need to reference frozen globals.
    if( ur_isShared( n ) )
        return;

    // Return if reference already exists.
    {
        UIndex* it  = arr->ptr.i;
        UIndex* end = it + arr->used;
        while( it != end )
        {
            if( *it == n )
                return;
            ++it;
        }
    }

    ur_arrReserve( arr, arr->used + 1 );
    arr->ptr.i[ arr->used ] = n;
    ++arr->used;
}


static void dp_addRef( DPCompiler* emit, int type, int count,
                       const GLuint* ids )
{
    DPResEntry* re;
    GLuint* ip;
    GLuint* end;
    UBuffer* arr = &emit->ref;
    int size;

    size = arr->used + sizeof(DPResEntry) + (sizeof(GLuint) * count);
    ur_binReserve( arr, size );
    re = (DPResEntry*) (arr->ptr.b + arr->used);
    arr->used = size;

    re->type  = type;
    re->count = count;
    re->_pad  = 0;

    ++emit->refCount;

    ip  = (GLuint*) (re + 1);
    end = ip + count;
    while( ip != end )
        *ip++ = *ids++;
}


#define STRIDE_OFFSET(str,off)  ((off << 8) | str)

/*
  Set offsets for interleaved array.

  blk rules:
     word!         <- size 3
     word! int!    <- size 1 to 4

  Returns zero (and throws error) if fails.
*/
static int emitBufferOffsets( UThread* ut, DPCompiler* emit, UBuffer* blk )
{
    UCell* it;
    UCell* end;
    GLsizei stride = 0;
    int size;
    int offset = 0;
    UAtom atom;

    it  = blk->ptr.cell;
    end = it + blk->used;
    while( it != end )
    {
        if( ur_is(it, UT_WORD) )
        {
            ++it;
            if( (it != end) && ur_is(it, UT_INT) )
            {
                stride += ur_int(it);
                ++it;
            }
            else
                stride += 3;
        }
        else
        {
            return ur_error( ut, UR_ERR_SCRIPT,
                        "Buffer attribute must be word!" );
        }
    }
    stride *= sizeof(GL_FLOAT);

#define B_STRIDE_OFFSET  STRIDE_OFFSET(stride, offset)

    it = blk->ptr.cell;
    while( it != end )
    {
        if( ur_is(it, UT_WORD) )
        {
            atom = ur_atom(it);
            ++it;
            if( (it != end) && ur_is(it, UT_INT) )
            {
                size = ur_int(it);
                ++it;
            }
            else
                size = 3;

            switch( atom )
            {
            case UR_ATOM_MINUS:
                // Ignore values; unused attribute.
                break;

            case UR_ATOM_COLOR:
                //glColorPointer( 3, GL_FLOAT, stride, offset );
                emitOp1( (size == 4) ? DP_COLOR_OFFSET_4 : DP_COLOR_OFFSET,
                         B_STRIDE_OFFSET );
                break;

            case UR_ATOM_NORMAL:
                //glNormalPointer( GL_FLOAT, stride, offset );
                emitOp1( DP_NORMAL_OFFSET, B_STRIDE_OFFSET );
                break;

            case UR_ATOM_TEXTURE:
                //glTexCoordPointer( 2, GL_FLOAT, stride, offset );
                emitOp1( DP_UV_OFFSET, B_STRIDE_OFFSET );
                break;

            case UR_ATOM_VERTEX:
                //glVertexPointer( 3, GL_FLOAT, stride, offset );
                emitOp1( (size == 4) ? DP_VERTEX_OFFSET_4 : DP_VERTEX_OFFSET,
                         B_STRIDE_OFFSET );
                break;

            default:
            {
                // LIMIT: Shader attribute names must be lower-case.
                const char* name = ur_atomCStr(ut, atom);
                int loc = glGetAttribLocation( emit->shaderProg, name );
                if( loc == -1 )
                {
                    return ur_error( ut, UR_ERR_SCRIPT,
                                     "Shader attrib '%s not found", name );
                }

                emitOp2( DP_ATTR_OFFSET, (loc << 8) | size, B_STRIDE_OFFSET );
            }
                break;
            }

            offset += size * sizeof(GL_FLOAT);
        }
    }
    return 1;
}


typedef struct
{
    int dataType;       // UT_INT or UT_VECTOR
    UIndex n;
}
DPPrimRecord;


static void dp_recordPrim( DPCompiler* emit, int dt, UIndex n )
{
    DPPrimRecord* pr;
    UBuffer* arr = &emit->primRec;

    ur_arrExpand1( DPPrimRecord, arr, pr );

    pr->dataType = dt;
    pr->n = n;
}


/*
  \param primOpcode    DP_DRAW_POINTS to DP_DRAW_QUADS.
  \param elemCount     Number of contiguous vertices to draw.
                       Zero if idxVec set.
  \param idxVec        Vertices to draw.

  \return zero (and throws error) if fails.
*/
static int genPrimitives( UThread* ut, DPCompiler* emit, int primOpcode,
                          int elemCount, const UCell* idxVec )
{
    GLuint buf[ DP_MAX_VBUF + 1 ];
    DPBuffer* vbuf;
    int bufCount;
    int drawArrPoints;
    int makeIndexBuf;
    int i = 0;


    drawArrPoints = (primOpcode == DP_DRAW_POINTS) && elemCount;
    makeIndexBuf = ! drawArrPoints && ! emit->indexBuf;

    bufCount = emit->vbufCount;
    if( makeIndexBuf )
        ++bufCount;

    if( bufCount )
    {
        glGenBuffers( bufCount, buf );
        dp_addRef( emit, RES_GL_BUFFERS, bufCount, buf );

        vbuf = emit->vbuf;
        for( i = 0; i < emit->vbufCount; ++i, ++vbuf )
        {
            UBuffer* arr = ur_buffer( vbuf->vecN );

            glBindBuffer( GL_ARRAY_BUFFER, buf[i] );
            glBufferData( GL_ARRAY_BUFFER, sizeof(float) * arr->used,
                          arr->ptr.f, vbuf->usage );

            emitOp1( DP_BIND_ARRAY, buf[i] );
            if( ! emitBufferOffsets( ut, emit, ur_buffer( vbuf->keyN ) ) )
                return 0;
        }
        emit->vbufCount = 0;
    }

    if( makeIndexBuf )
    {
        emit->indexBuf = buf[i];
        emitOp1( DP_BIND_ELEMENTS, buf[i] );
    }
    else if( drawArrPoints )
    {
        emitOp1( DP_DRAW_ARR_POINTS, elemCount );
        return 1;
    }

    if( idxVec )
    {
        elemCount = ur_bufferSer( idxVec )->used;
    }

    if( elemCount > 0 )
    {
        // Record vector!/int! info for use later by genIndices().
        // Reading the primitive indices from a vector! here would require
        // an extra copy of the data (to store until genIndices).

        if( idxVec )
            dp_recordPrim( emit, UT_VECTOR, idxVec->series.buf );
        else
            dp_recordPrim( emit, UT_INT, elemCount );

        if( emit->indexCount )
        {
            emitOp2( primOpcode + (DP_DRAW_POINTS_I - DP_DRAW_POINTS),
                     elemCount, emit->indexCount * sizeof(uint16_t) );
        }
        else
        {
            emitOp1( primOpcode, elemCount );
        }
        emit->indexCount += elemCount;
    }

    return 1;
}


/*
static void bufferReset( DPCompiler* emit )
{
    emit->tgeo.used = 0;
    emit->vbufCount = 0;
    emit->indexCount = 0;
    emit->indexBuf = 0;
}
*/


static void _genIndices2( UThread* ut, DPCompiler* emit, uint16_t* dst )
{
    UBuffer* iarr;
    uint32_t* it;
    uint32_t* end;
    DPPrimRecord* pr;
    DPPrimRecord* pend;
    int offset = 0;

    pr   = (DPPrimRecord*) emit->primRec.ptr.v;
    pend = pr + emit->primRec.used;
    while( pr != pend )
    {
        if( pr->dataType == UT_VECTOR )
        {
            // Convert vector! int32_t to GL_UNSIGNED_SHORT.
            iarr = ur_buffer( pr->n );
            it  = iarr->ptr.u32;
            end = it + iarr->used;
            while( it != end )
                *dst++ = *it++;

            offset += iarr->used;
        }
        else
        {
            int idx = offset;
            offset += pr->n;
            while( idx != offset )
                *dst++ = idx++;
        }
        ++pr;
    }
}


static void genIndices( UThread* ut, DPCompiler* emit )
{
    if( emit->indexCount && emit->indexBuf )
    {
        uint16_t* dst;

        int len = sizeof(uint16_t) * emit->indexCount;
        dst = malloc( len );
        if( dst )
        {
            _genIndices2( ut, emit, dst );

            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, emit->indexBuf );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER, len, dst, GL_STATIC_DRAW );
            free( dst );
        }
    }
}


// Allow only normal, block-bound words so we can emit just 2 numbers.
static UIndex _wordBlock( const UCell* cell )
{
    int bind = ur_binding( cell );
    if( bind == UR_BIND_THREAD || bind == UR_BIND_ENV )
        return cell->word.ctx;
    return UR_INVALID_BUF;
}


static int emitWordOp( DPCompiler* emit, const UCell* cell, int opcode )
{
    if( ur_is(cell, UT_WORD) || ur_is(cell, UT_GETWORD) )
    {
        UIndex blkN = _wordBlock( cell );
        if( blkN != UR_INVALID_BUF )
        {
            refBlock( emit, blkN );
            emitOp2( opcode, blkN, cell->word.index );
            return 1;
        }
    }
    return 0;
}


/*--------------------------------------------------------------------------*/


static void emitGeoBind( const Geometry* geo, DPCompiler* emit )
{
    int stride = geo->attrSize * sizeof(GL_FLOAT);

    dp_addRef( emit, RES_GL_BUFFERS, 2, geo->buf );

    emitOp1( DP_BIND_ARRAY, geo->buf[0] );

#define G_STRIDE_OFFSET(off)    STRIDE_OFFSET(stride,off*sizeof(GL_FLOAT))

    if( geo->colorOff > -1 )
        emitOp1( DP_COLOR_OFFSET, G_STRIDE_OFFSET(geo->colorOff) );

    if( geo->normOff > -1 )
        emitOp1( DP_NORMAL_OFFSET, G_STRIDE_OFFSET(geo->normOff) );

    if( geo->uvOff > -1 )
        emitOp1( DP_UV_OFFSET, G_STRIDE_OFFSET(geo->uvOff) );

    if( geo->vertOff > -1 )
        emitOp1( DP_VERTEX_OFFSET, G_STRIDE_OFFSET(geo->vertOff) );

    emitOp1( DP_BIND_ELEMENTS, geo->buf[1] );
}


static void emitGeoPrim( const Primitives* it, DPCompiler* emit )
{
    int opcode;

    switch( it->mode )
    {
        case GL_POINTS:
            opcode = DP_DRAW_POINTS;
            break;
        case GL_LINES:
            opcode = DP_DRAW_LINES;
            break;
        case GL_LINE_STRIP:
            opcode = DP_DRAW_LINE_STRIP;
            break;
        case GL_TRIANGLES:
            opcode = DP_DRAW_TRIS;
            break;
        case GL_TRIANGLE_STRIP:
            opcode = DP_DRAW_TRI_STRIP;
            break;
        case GL_TRIANGLE_FAN:
            opcode = DP_DRAW_TRI_FAN;
            break;
        default:
            return;
    }

    if( it->offset )
    {
        emitOp2( opcode + (DP_DRAW_POINTS_I - DP_DRAW_POINTS),
                 it->count, it->offset * sizeof(uint16_t) );
    }
    else
    {
        emitOp1( opcode, it->count );
    }
}


static void emitGeoPrims( const Geometry* geo, DPCompiler* emit )
{
    Primitives* it  = (Primitives*) geo->prim.ptr.v;
    Primitives* end = it + geo->prim.used;
    while( it != end )
    {
        emitGeoPrim( it, emit );
        ++it;
    }
}


static void compileGeo( const Geometry* geo, DPCompiler* emit )
{
    geo_bufferData( geo );
    emitGeoBind( geo, emit );
    emitGeoPrims( geo, emit );
}


/*--------------------------------------------------------------------------*/


/*
void vbo_setAttrComp( float* attr, int count, int stride, float value )
{
    float* end = attr + (count * stride);
    while( attr != end )
    {
        *attr = value;
        attr += stride;
    }
}
*/


void vbo_initTextIndices( uint16_t* ip, int idx, int glyphCount )
{
    while( glyphCount-- )
    {
        *ip++ = idx;
        *ip++ = idx + 1;
        *ip++ = idx + 2;
        *ip++ = idx;
        *ip++ = idx + 2;
        *ip++ = idx + 3;

        idx += 4;
    }
}


static const uint8_t* drawTextReturn( DrawTextState* ds, const uint8_t* it,
                                      const uint8_t* end )
{
    (void) end;

    if( *it == '\n' )
    {
        ds->x = ds->marginL;
        ds->y -= txf_lineSpacing( ds->tf );
        ds->prev = 0;
    }
    else if( *it == '\t' )
    {
        TexFontGlyph* tgi = txf_glyph( ds->tf, ' ' );
        if( tgi )
        {
            float tabWidth = (float) (tgi->advance * 4);
            ds->x += tabWidth - fmodf(ds->x - ds->marginL, tabWidth);
        }
        ds->prev = 0;
    }
    return it + 1;
}


void vbo_drawTextInit( DrawTextState* ds, TexFont* tf, GLfloat x, GLfloat y )
{
    ds->tf      = tf;
    ds->lowChar = drawTextReturn;
    ds->prev    = 0;
    ds->x       = x;
    ds->y       = y;
    ds->marginL = x;
    ds->marginR = 90000.0f;
    ds->emitTris = 0;
}


/*
  Returns number of glyphs drawn.
*/
int vbo_drawText( DrawTextState* ds,
                  GLfloat* tp, GLfloat* vp, int stride,
                  const uint8_t* it, const uint8_t* end )
{
    //const uint8_t* wordStart = it;
    GLfloat gw, gh;
    GLfloat gx, gy;
    GLfloat min_s, max_s;
    GLfloat min_t, max_t;
    int xkern;
    int drawn = 0;
    TexFontGlyph* tgi;
    GLfloat texW = (GLfloat) ds->tf->tex_width;
    GLfloat texH = (GLfloat) ds->tf->tex_height;


    while( it != end )
    {
        /*
        if( ds->x > ds->marginR )
        {
            ds->x = ds->marginL;
            ds->y -= txf_lineSpacing( ds->tf );
            ds->prev = 0;
        }
        */

        if( *it > ' ' )
        {
            tgi = txf_glyph( ds->tf, *it++ );
            if( tgi )
            {
                gw = (GLfloat) tgi->width;
                gh = (GLfloat) tgi->height;
                gx = (GLfloat) tgi->x;
                gy = (GLfloat) tgi->y;

                min_s = gx / texW;
                max_s = (gx + gw) / texW;
#if 0
                min_t = 1.0f - (gy / texH);
                max_t = 1.0f - ((gy + gh) / texH);
#else
                min_t = gy / texH;
                max_t = (gy - gh) / texH;
#endif

                xkern = ds->prev ? txf_kerning( ds->tf, ds->prev, tgi ) : 0;

                gx = ds->x + (GLfloat) (tgi->xoffset + xkern);
                ds->x += (GLfloat) (tgi->advance + xkern);
                gy = ds->y + (GLfloat) tgi->yoffset;

#define VB_GLYPH_ATTR(s,t,x,y) \
    tp[0] = s; \
    tp[1] = t; \
    tp += stride; \
    vp[0] = x; \
    vp[1] = y; \
    vp[2] = 0.0f; \
    vp += stride

                VB_GLYPH_ATTR( min_s, min_t, gx,      gy );
                VB_GLYPH_ATTR( max_s, min_t, gx + gw, gy );
                VB_GLYPH_ATTR( max_s, max_t, gx + gw, gy + gh );

                if( ds->emitTris )
                {
                    VB_GLYPH_ATTR( max_s, max_t, gx + gw, gy + gh );
                    VB_GLYPH_ATTR( min_s, max_t, gx,      gy + gh );
                    VB_GLYPH_ATTR( min_s, min_t, gx,      gy );
                }
                else
                {
                    VB_GLYPH_ATTR( min_s, max_t, gx,      gy + gh );
                }

                ++drawn;
            }
            ds->prev = tgi;
        }
        else if( *it == ' ' )
        {
            //wordStart = it + 1;
            tgi = txf_glyph( ds->tf, ' ' );
            if( tgi )
            {
#if 0
                xkern = prev ? txf_kerning( ds->tf, prev, tgi ) : 0;
                x += (GLfloat) (prev->advance + xkern);
#else
                ds->x += (GLfloat) tgi->advance;
#endif
            }
            ds->prev = tgi;
            ++it;
        }
        else
        {
            it = ds->lowChar( ds, it, end );
            if( ! it )
                break;
        }
    }

    return drawn;
}


/*
   Draws triangles at the current pen position, where penY is the baseline
   of the first line of text.
   The pen is moved to the end of text.
*/
void dp_drawText( DPCompiler* emit, TexFont* tf,
                  const uint8_t* it, const uint8_t* end )
{
    Primitives prim;
    DrawTextState ds;
    float* attr;
    Geometry* geo = &emit->tgeo;
    int cc;
    int startIndex;
    int len = end - it;


    if( (len < 1) || (geo->uvOff < 0) )
        return;

    //geo_primBegin( geo, GL_TRIANGLES );
    prim.mode   = GL_TRIANGLES;
    prim.offset = geo->idx.used;

    startIndex = geo_vertCount( geo );

    ur_arrReserve( &geo->attr, geo->attr.used + (len * 4 * geo->attrSize) );
    attr = geo->attr.ptr.f + geo->attr.used;

    vbo_drawTextInit( &ds, tf, emit->penX, emit->penY );

    cc = vbo_drawText( &ds,
                       attr + geo->uvOff,
                       attr + geo->vertOff, geo->attrSize,
                       it, end );
    if( cc )
    {
        emit->penX = ds.x;
        emit->penY = ds.y;

        // Zero vertices Z.
        //vbo_setAttrComp(attr + geo->vertOff + 2, cc * 4, geo->attrSize, 0.0f);

        geo->attr.used += cc * 4 * geo->attrSize;

        vbo_initTextIndices( geo_addIndices(geo, cc * 6), startIndex, cc );

        //geo_primEnd( geo );
        prim.count = geo->idx.used - prim.offset;
        emitGeoPrim( &prim, emit );
    }
}


static void dp_emitTextWord( DPCompiler* emit, const UCell* wordC )
{
    //float* attr;
    Geometry* geo = &emit->tgeo;
    int indexOffset = geo->idx.used;
    int attrOffset = geo->attr.used;
    int startIndex = geo_vertCount( geo );
    int maxLen = MAX_DYNAMIC_TEXT_LEN;
    const uint32_t* pp;

    geo->flags |= GEOM_DYN_ATTR;

    geo_addVerts( geo, maxLen * 4 );
    //vbo_setAttrComp(attr + geo->vertOff + 2, maxLen * 4, geo->attrSize, 0.0f);
    vbo_initTextIndices( geo_addIndices(geo, maxLen * 6), startIndex, maxLen );

    emitOp1( DP_FONT, emit->fontN );
    emitWordOp( emit, wordC, DP_TEXT_WORD );
    emitDPArg( emit, attrOffset * sizeof(GLfloat) );
    pp = (const uint32_t*) &emit->penX;
    emitDPArg( emit, pp[0] );
    emitDPArg( emit, pp[1] );
    emitOp2( DP_DRAW_TRIS_I, 0, indexOffset * sizeof(uint16_t) );
}


/*
   Sets bi->it and bi->end to string data.
   The bi->buf member is not set.
   Uses glEnv.tmpStr.
*/
void dp_toString( UThread* ut, const UCell* cell, UBinaryIter* bi )
{
    UBuffer* str;

    if( ur_isStringType( ur_type(cell) ) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, cell );
        if( si.buf->form == UR_ENC_LATIN1 )
        {
            bi->it  = si.buf->ptr.b + si.it;
            bi->end = si.buf->ptr.b + si.end;
            return;
        }
        str = &glEnv.tmpStr;
        str->used = 0;
        ur_strAppend( str, si.buf, si.it, si.end );
    }
    else
    {
        str = &glEnv.tmpStr;
        str->used = 0;
        ur_toStr( ut, cell, str, 0 );
    }
    bi->it  = str->ptr.b;
    bi->end = str->ptr.b + str->used;
}


void dp_drawTextCell( DPCompiler* emit, UThread* ut, const UCell* cell,
                      UAtom sel, const UCell* area )
{
    UBinaryIter bi;
    TexFont* tf = (TexFont*) ur_buffer(emit->fontN)->ptr.v; 

    dp_toString( ut, cell, &bi );

    if( sel && area )
    {
        const int16_t* rect = area->coord.n;
        int size[2];

        txf_pixelSize( tf, bi.it, bi.end, size );
        //printf( "KR text size: %d,%d h: %d\n", size[0], size[1], rect[3] );

        if( sel == UR_ATOM_CENTER )
            emit->penX += rect[0] + ((rect[2] - size[0]) >> 1);
        else if( sel == UR_ATOM_RIGHT )
            emit->penX += rect[0] + rect[2] - size[0];
        else
            emit->penX += rect[0];

        if( rect[3] )
        {
            // Center vertically.
            if( size[1] > tf->max_ascent )
                size[1] = -size[1] + tf->max_ascent;
            emit->penY += rect[1] + ((rect[3] - size[1]) >> 1);
        }
    }

    dp_drawText( emit, tf, bi.it, bi.end );
}


static void _sphere( DPCompiler* emit, float radius, int slices, int stacks,
                     const char* attrib, int inside )
{
    Geometry geo;

    geo_init( &geo, attrib, 0, 0, 0 );
    geo_sphere( &geo, radius, slices, stacks, inside );
    compileGeo( &geo, emit );
    geo_free( &geo );
}


static void _box( DPCompiler* emit, const float* minv, const float* maxv,
                  const char* attrib, int inside )
{
    Geometry geo;

    geo_init( &geo, attrib, 0, 0, 0 );
    geo_box( &geo, minv, maxv, inside );
    compileGeo( &geo, emit );
    geo_free( &geo );
}


/*--------------------------------------------------------------------------*/


/*
  Returns switch id.
*/
// LIMIT: Switch Id limits draw-prog to 65k 32-bit words.
DPSwitch dp_beginSwitch( DPCompiler* emit, int count )
{
    union
    {
        uint32_t u32;
        uint16_t u16[2];
    }
    fw;
    uint16_t* sp;
    DPSwitch sid;
    int i;
    int wcount;
    int jmp;

    fw.u16[0] = 1;
    fw.u16[1] = count;

    emitOp1( DP_SWITCH, fw.u32 );
    flushOps( DP_NOP );

    sid = emit->inst.used - 1;

    // Initialize jump table to first case.
    wcount = (count + 1) / 2;
    sp = (uint16_t*) dp_addArgs( emit, wcount );
    jmp = wcount + 1;
    for( i = 0; i < count; ++i )
        *sp++ = jmp;
    if( count & 1 )
        *sp = 0xffff;   // Setting unused entry to stand out in a hex dump.

    return sid;
}


void dp_endCase( DPCompiler* emit, DPSwitch sid )
{
    uint16_t* sp = (uint16_t*) (emit->inst.ptr.u32 + sid);
    uint32_t n = sp[0];

    if( n < sp[1] )
    {
        emitOp1( DP_JMP, 1 );   // Proper offset filled in by dp_closeSwitch().
        flushOps( DP_NOP );

        // Re-acquire emit->inst.ptr (may have changed).
        sp = (uint16_t*) (emit->inst.ptr.u32 + sid);

        ++n;
        sp[n + 1] = emit->inst.used - sid;
        sp[0] = n;
    }
}


void dp_endSwitch( DPCompiler* emit, DPSwitch sid, uint16_t caseN )
{
    uint32_t* pc;
    uint16_t* sp;
    uint16_t* ti;
    uint32_t* jmpPos;
    uint32_t delta;
    uint32_t count;
    uint32_t switchLen;

    flushOps( DP_NOP );

    pc = emit->inst.ptr.u32 + sid;
    switchLen = emit->inst.used - sid;
    sp = (uint16_t*) pc;
    ti = sp + 3;
    count = sp[1] - 1;

    // Fix jump offsets.
    while( count-- )
    {
        delta = *ti++;
        jmpPos = pc + delta;
        jmpPos[-1] = switchLen - delta + 1;
    }

    if( caseN < sp[1] )
        sp[0] = caseN + 2;
}


/*
  \param sid    Switch id returned from dp_beginSwitch().
  \param caseN  Which switch to take.  Zero based.
*/
void dp_setSwitch( DPHeader* dp, DPSwitch sid, uint16_t caseN )
{
    uint16_t* sp = (uint16_t*) (dp_byteCode(dp) + sid);
    if( caseN < sp[1] )
        sp[0] = caseN + 2;
}


/*--------------------------------------------------------------------------*/


DPSwitch dp_beginTransXY( DPCompiler* emit, float x, float y )
{
    Number nx, ny;
    DPSwitch sid;

    emitOp( DP_PUSH );
    sid = nextArgPosition( emit );
    nx.f = x;
    ny.f = y;
    emitOp2( DP_TRANSLATE_XY, nx.i, ny.i );

    return sid;
}


void dp_endTransXY( DPCompiler* emit )
{
    emitOp( DP_POP );
}


void ur_setTransXY( UThread* ut, UIndex resN, DPSwitch sid, float x, float y )
{
    DPHeader* dp = (DPHeader*) ur_buffer(resN)->ptr.v;
    if( dp )
    {
        float* sp = (float*) (dp_byteCode(dp) + sid);
        sp[0] = x;
        sp[1] = y;
        //dp_setTransXY( dp, sid, x, y );
    }
}


/*--------------------------------------------------------------------------*/


static void dp_init( DPCompiler* gpc )
{
    memSet( gpc, 0, sizeof(DPCompiler) );

    ur_binInit( &gpc->ref, 8 );
    ur_arrInit( &gpc->inst,    sizeof(uint32_t),    32 );
    ur_arrInit( &gpc->blocks,  sizeof(UIndex),      20 );
    ur_arrInit( &gpc->primRec, sizeof(DPPrimRecord), 8 );
}


static void dp_free( DPCompiler* gpc )
{
    ur_binFree( &gpc->ref );
    ur_arrFree( &gpc->inst );
    ur_arrFree( &gpc->blocks );
    ur_arrFree( &gpc->primRec );

    geo_free( &gpc->tgeo );
}


void dp_tgeoInit( DPCompiler* emit )
{
    UBuffer* attr = &emit->tgeo.attr;
    if( ur_testAvail(attr) )
        return;
    geo_init( &emit->tgeo, "tv", 32, 32, 8 );
    emitGeoBind( &emit->tgeo, emit );
}


static void dp_tgeoFlushPrims( DPCompiler* emit )
{
    emitGeoPrims( &emit->tgeo, emit );
    emit->tgeo.prim.used = 0;
}


static int cellToColorUB( const UCell* pc, uint8_t* col )
{
    if( ur_is(pc, UT_INT) )
    {
        col[0] = (ur_int(pc) >> 16) & 0xff;
        col[1] = (ur_int(pc) >>  8) & 0xff;
        col[2] =  ur_int(pc) & 0xff;
        return 3;
    }
    else if( ur_is(pc, UT_COORD) )
    {
        col[0] = pc->coord.n[0];
        col[1] = pc->coord.n[1];
        col[2] = pc->coord.n[2];
        if( pc->coord.len > 3 )
        {
            col[3] = pc->coord.n[3];
            return 4;
        }
        return 3;
    }
    else if( ur_is(pc, UT_VEC3) )
    {
        col[0] = (uint8_t) (pc->vec3.xyz[0] * 255.0);
        col[1] = (uint8_t) (pc->vec3.xyz[1] * 255.0);
        col[2] = (uint8_t) (pc->vec3.xyz[2] * 255.0);
        return 3;
    }
    return 0;
}


static void cellToVec3( const UCell* cell, float* v3 )
{
    if( ur_is(cell, UT_VEC3) )
    {
        v3[0] = cell->vec3.xyz[0];
        v3[1] = cell->vec3.xyz[1];
        v3[2] = cell->vec3.xyz[2];
    }
    else
    {
        float n;
        if( ur_is(cell, UT_DOUBLE) )
            n = (float) ur_double(cell);
        else if( ur_is(cell, UT_INT) )
            n = (float) ur_int(cell);
        else
            n = 0.0f;
        v3[0] = v3[1] = v3[2] = n;
    }
}


static uint8_t _primOp[] =
{
    /*
    GL_POINTS,    GL_LINES,          GL_LINE_STRIP,
    GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN,
    GL_QUADS,     GL_QUAD_STRIP
    */
    DP_DRAW_POINTS, DP_DRAW_LINES,     DP_DRAW_LINE_STRIP,
    DP_DRAW_TRIS,   DP_DRAW_TRI_STRIP, DP_DRAW_TRI_FAN,
    DP_DRAW_QUADS,  0
};


#if 0
#define PC_VALUE(val) \
    if( ur_is(pc, UT_WORD) ) { \
        if( ! (val = ur_wordCell(ut, pc)) ) \
            goto error; \
    } \
    else val = pc;
#else
#define PC_VALUE(val) \
    if( ! (val = dp_value(ut,pc)) ) \
        goto error;

// Returns NULL if not found.
static const UCell* dp_value( UThread* ut, const UCell* cell )
{
    if( ur_is(cell, UT_WORD) )
        return ur_wordCell(ut, cell);
    if( ur_is(cell, UT_PAREN) )
    {
        UCell* res = ut->stack.ptr.cell + ut->stack.used - 1;
        return boron_doBlock(ut, cell, res) ? res : NULL;
    }
    return cell;
}
#endif


#define INC_PC \
    if( ++pc == pend ) \
        goto end_of_block;

#define INC_PC_VALUE(val) \
    INC_PC \
    PC_VALUE(val)

#define scriptError(msg) \
    ur_error( ut, UR_ERR_SCRIPT, msg ); \
    goto error

#define typeError(msg) \
    ur_error( ut, UR_ERR_TYPE, msg ); \
    goto error

#define SA_VERT    0x01
#define SA_NORM    0x02
#define SA_COLOR   0x04
#define SA_UV      0x08

#define refContext(cell) \
    refBlock( emit, cell->series.buf )

#define refShader(resN) \
    dp_addRef( emit, RES_SHADER, 1, (GLuint*) &(resN) )

#define refVBO(resN) \
    dp_addRef( emit, RES_VBO, 1, (GLuint*) &(resN) )

#define refDrawProg(resN) \
    dp_addRef( emit, RES_DRAW_PROG, 1, (GLuint*) &(resN) )


/*
  \return UR_OK/UR_THROW.
*/
int dp_compile( DPCompiler* emit, UThread* ut, UIndex blkN )
{
    const UCell* val;
    const UCell* pc;
    const UCell* pend;
    const UBuffer* blk;
    int opcode;
    UAtom option;

    blk  = ur_buffer( blkN );
    pc   = blk->ptr.cell;
    pend = blk->ptr.cell + blk->used;

    while( pc != pend )
    {
        if( ur_is(pc, UT_WORD) )
        {
            option = 0;
            opcode = ur_atomsSearch(glEnv.drawOpTable, DOP_COUNT, ur_atom(pc));
comp_op:
            switch( opcode )
            {
            case DOP_NOP:
                emitOp( DP_NOP );
                break;

            case DOP_CALL:
                INC_PC
                if( ur_is(pc, UT_GETWORD) )
                {
                    emitWordOp( emit, pc, DP_RUN_PROGRAM_WORD );
                }
                else
                {
                    PC_VALUE(val)
                    if( ur_is(val, UT_DRAWPROG) )
                    {
                        refDrawProg( val->series.buf );
                        emitOp1( DP_RUN_PROGRAM, val->series.buf );
                    }
#if 0
                    else if( ur_is(val, UT_WIDGET) )
                    {
                        //refWidget( ur_int(val) );
                        emitOp1( DP_RENDER_WIDGET, ur_int(val) );
                    }
#endif
                    else
                    {
                        typeError( "call expected draw-prog!" );
                    }
                }
                break;

            case DOP_IMAGE:                 // image [x,y] texture
            {
                GLfloat x, y;
                GLfloat w, h;

                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_COORD) )
                {
                    x = (GLfloat) val->coord.n[0];
                    y = (GLfloat) val->coord.n[1];
                    goto image_next;
                }
                else if( ur_is(val, UT_VEC3) )
                {
                    x = val->vec3.xyz[0];
                    y = val->vec3.xyz[1];
image_next:
                    INC_PC
                    PC_VALUE(val)
                }
                else
                {
                    x = y = 0.0f;
                }

                if( ur_is(val, UT_TEXTURE) )
                {
                    if( ur_texRast(val) )
                    {
                        RasterHead* rh = (RasterHead*)
                                        ur_buffer( ur_texRast(val) )->ptr.v;
                        w = rh->width;
                        h = rh->height;
                    }
                    else
                    {
                        // glGetTexLevelParameterfv requires GLES 3.1
                        //scriptError
                        //    ( "image cannot get texture! size with GLES2" );
                        glBindTexture( GL_TEXTURE_2D, ur_texId(val) );
                        glGetTexLevelParameterfv( GL_TEXTURE_2D, 0,
                                                  GL_TEXTURE_WIDTH, &w );
                        glGetTexLevelParameterfv( GL_TEXTURE_2D, 0,
                                                  GL_TEXTURE_HEIGHT, &h );
                    }

                    dp_tgeoInit( emit );
                    geo_image( &emit->tgeo, x, y, x + w, y + h );
                    dp_tgeoFlushPrims( emit );
                }
                else
                {
                    typeError( "image expected texture!" );
                }
            }
                break;

            case DOP_COLOR:
                INC_PC
                if( ur_is(pc, UT_GETWORD) )
                {
                    emitWordOp( emit, pc, DP_COLOR_WORD );
                }
                else
                {
                    // NOTE: An active shader is not available when compiling
                    // widget style fragments, so explicit uniform location
                    // is used (ULOC_COLOR).

                    uint32_t color = 0;
                    int cop;

                    PC_VALUE(val)
                    cop = cellToColorUB( val, (uint8_t*) &color );
                    if( cop == 3 )
                        cop = DP_COLOR3;
                    else if( cop == 4 )
                        cop = DP_COLOR4;
                    else
                    {
                        typeError( "color expected int!/coord!/vec3!" );
                    }
                    emitOp1( cop, color );
                }
                break;
#if 0
            case DOP_COLORS:
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_VECTOR) )
                {
                    emit->cbufN = val->series.buf;
                  //emit->attrMod |= SA_COLOR;
                }
                break;

            case DOP_VERTS:
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_VECTOR) )
                {
                    emit->vbufN = val->series.buf;
                  //emit->attrMod |= SA_VERT;
                    emit->attrN = 0;
                }
                break;

            case DOP_NORMALS:
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_VECTOR) )
                {
                    emit->nbufN = val->series.buf;
                  //emit->attrMod |= SA_NORM;
                }
                break;

            case DOP_UVS:
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_VECTOR) )
                {
                    emit->ubufN = val->series.buf;
                  //emit->attrMod |= SA_UV;
                }
                break;
#endif
/*
            case DOP_ATTRIB:
                break;
*/
            case DOP_POINTS:            // prim vector!/int!
            case DOP_LINES:
            case DOP_LINE_STRIP:
            case DOP_TRIS:
            case DOP_TRI_STRIP:
            case DOP_TRI_FAN:
            case DOP_QUADS:
            //case DOP_QUAD_STRIP:
            {
                int dpOp = _primOp[ opcode - DOP_POINTS ];
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_VECTOR) )
                {
                    const UBuffer* vec = ur_bufferSer(val);
                    if( vec->form != UR_VEC_I32 )
                        goto bad_prim;
                    if( ! genPrimitives( ut, emit, dpOp, 0, val ) )
                        goto error;
                }
                else if( ur_is(val, UT_INT) )
                {
                    if( ! genPrimitives( ut, emit, dpOp, ur_int(val), 0 ) )
                        goto error;
                }
                else
                {
bad_prim:
                    ur_error( ut, UR_ERR_SCRIPT, "%s expected vector!/int!",
                              ur_atomCStr( ut, ur_atom(--pc) ) );
                    goto error;
                }
            }
                break;

            case DOP_SPHERE:                 // sphere radius silces,stacks
            {
                float radius; 
                int slices, stacks;

                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_DOUBLE) )
                    radius = (float) ur_double(val);
                else if( ur_is(val, UT_INT) )
                    radius = (float) ur_int(val);
                else
                    goto bad_sphere;
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_COORD) )
                {
                    slices = val->coord.n[0];
                    stacks = val->coord.n[1];
                }
                else
                {
bad_sphere:
                    scriptError( "sphere expected radius slices,stacks" );
                }
                _sphere( emit, radius, slices, stacks, "ntv", 0 );
            }
                break;

            case DOP_BOX:                    // box min max
            {
                float box[6];

                INC_PC
                cellToVec3( pc, box );
                INC_PC
                cellToVec3( pc, box + 3 );

                _box( emit, box, box + 3, "ntv", option );
            }
                break;

            case DOP_QUAD:          // quad tex-size uvs area
                                    // quad tex-size uvs,corner area (skin)
            {
                const UCell* ts;    // Texture size
                const UCell* tc;    // Texture coordinates
                int skin;

                INC_PC
                PC_VALUE(ts)
                if( ! ur_is(ts, UT_COORD) )
                {
bad_quad:
                    typeError( "quad expected coord!" );
                }
                INC_PC
                PC_VALUE(tc);
                if( ! ur_is(tc, UT_COORD) )
                    goto bad_quad;
                skin = tc->coord.len > 4;
                INC_PC
                PC_VALUE(val);
                if( ! ur_is(val, UT_COORD) )
                    goto bad_quad;

                dp_tgeoInit( emit );
                geo_quadSkin( &emit->tgeo, val->coord.n,
                              ts->coord.n, tc->coord.n, skin );
                dp_tgeoFlushPrims( emit );
            }
                break;

            case DOP_CLEAR:
                emitOp( DP_CLEAR );
                break;

            case DOP_CAMERA:
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_CONTEXT) )
                {
                    refContext( val );
                    emitOp1( DP_CAMERA, val->series.buf );
                }
                else
                {
                    typeError( "camera expected context!" );
                }
                break;

            case DOP_LIGHT:
                INC_PC
                if( ! ur_is(pc, UT_BLOCK) )
                {
                    typeError( "light expected block!" );
                }
                refBlock( emit, pc->series.buf );
                emitOp1( DP_LIGHT, pc->series.buf );
                break;

            case DOP_PUSH:
                emitOp( DP_PUSH );
                break;

            case DOP_POP:
                emitOp( DP_POP );
                break;

            case DOP_TRANSLATE:
                INC_PC_VALUE(val)
                if( ur_is(val, UT_VEC3) )
                {
                    const uint32_t* vp = (const uint32_t*) &val->vec3.xyz;
                    emitDPInst( emit, DP_TRANSLATE, 3, vp[0], vp[1], vp[2] );
                }
                else if( ! emitWordOp( emit, val, DP_TRANSLATE_WORD ) )
                {
                    typeError( "translate expected vec3!/get-word!" );
                }
                break;

            case DOP_ROTATE:    // rotate axis word! angle double!/int!
                                // rotate :quat
                INC_PC
                if( ur_is(pc, UT_GETWORD) )
                {
                    emitWordOp( emit, pc, DP_ROTATE_WORD );
                    break;
                }
                opcode = ur_atom(pc);
                if( opcode == UR_ATOM_Y )
                    opcode = DP_ROTATE_Y;
                else if( opcode == UR_ATOM_Z )
                    opcode = DP_ROTATE_Z;
                else
                    opcode = DP_ROTATE_X;

                INC_PC
                if( ur_is(pc, UT_DOUBLE) )
                {
                    Number n;
                    n.f = ur_double(pc);
                    emitOp1( opcode, n.i );
                }
                else
                {
                    emitWordOp( emit, pc,
                                opcode + (DP_ROTATE_X_WORD - DP_ROTATE_X) );
                }
                break;

            case DOP_SCALE:
                INC_PC
                if( ur_is(pc, UT_DOUBLE) )
                {
                    Number n;
                    n.f = ur_double(pc);
                    emitOp1( DP_SCALE_1, n.i );
                }
                else
                {
                    emitWordOp( emit, pc, DP_SCALE_WORD );
                }
                break;

            case DOP_FONT:
                INC_PC_VALUE(val)
                if( ur_is(val, UT_FONT) )
                {
                    //glBindTexture( GL_TEXTURE_2D, ur_fontTexId(val) );
                    emit->fontN = ur_fontTF(val);
                }
                else
                {
                    typeError( "font expected font!" );
                }
                break;

            case DOP_TEXT:          // text [x,y] string!
                                    // text/center/right rect [x,y] string!
            {
                const UCell* area = 0;

                INC_PC_VALUE(val)
                if( option )
                {
                    if( ! ur_is(val, UT_COORD) || (val->coord.len < 4) )
                    {
                        typeError( "text expected area coord!" );
                    }
                    area = val;
                    emit->penX = 0.0f;
                    emit->penY = 0.0f;
                    INC_PC_VALUE(val)
                }

                if( ur_is(val, UT_COORD) )
                {
                    emit->penX = (GLfloat) val->coord.n[0];
                    emit->penY = (GLfloat) val->coord.n[1];
                    INC_PC_VALUE(val)
                }
                else if( ur_is(val, UT_VEC3) )
                {
                    emit->penX = val->vec3.xyz[0];
                    emit->penY = val->vec3.xyz[1];
                    INC_PC_VALUE(val)
                }

                if( emit->fontN )
                {
                    dp_tgeoInit( emit );
                    if( ur_is(val, UT_GETWORD) )
                        dp_emitTextWord( emit, val );
                    else
                        dp_drawTextCell( emit, ut, val, option, area ); 
                }
                else
                {
                    scriptError( "font is unset for text" );
                }
            }
                break;

            case DOP_SHADER:
            {
                INC_PC_VALUE(val)
#if 0
                // Don't allow old fixed functionality pipeline.
                // This allows us to use 0 as the currentProgram "unset" state.
                if( ur_is(val, UT_NONE) )
                {
                    emitOp1( DP_SHADER, 0 );
                    emit->shaderProg = 0;
                }
                else
#endif
                {
                    const UBuffer* blk;
                    const Shader* sh;

                    sh = shaderContext( ut, val, &blk );
                    if( ! sh )
                    {
                        scriptError( "invalid shader" );
                    }
                    if( blk )
                    {
                        emit->shaderResN = blk->ptr.cell[0].series.buf;
                        refContext( val );
                        emitOp2( DP_SHADER_CTX, sh->program, val->series.buf );
                    }
                    else
                    {
                        emit->shaderResN = val->series.buf;
                        refShader( val->series.buf );
                        emitOp1( DP_SHADER, sh->program );
                    }
                    emit->shaderProg = sh->program;
                }
            }
                break;

            case DOP_UNIFORM:       // uniform name value
            {
                GLint loc;
                UAtom name;

                INC_PC
                if( ! ur_is(pc, UT_WORD) )
                {
                    typeError( "uniform expected word! name" );
                }
                name = ur_atom(pc);
                INC_PC_VALUE(val)
                if( ur_is(val, UT_TEXTURE) && emit->shaderProg )
                {
                    Shader* sh = (Shader*) ur_buffer( emit->shaderResN )->ptr.v;
                    loc = shaderTextureUnit( sh, name );
                    if( loc > -1 )
                    {
                        emitOp2( DP_BIND_TEXTURE, GL_TEXTURE0 + loc,
                                 ur_texId(val) );
                    }
                }
                else
                {
                    loc = glGetUniformLocation( emit->shaderProg,
                                                ur_atomCStr(ut, name) );
                    if( loc == -1 )
                    {
                        ur_error( ut, UR_ERR_SCRIPT,
                                  "uniform \"%s\" not found in program %d",
                                  ur_atomCStr(ut, name), emit->shaderProg );
                        goto error;
                    }
                    switch( ur_type(val) )
                    {
                        case UT_LOGIC:
                            emitOp2( DP_UNIFORM_1I, loc, ur_logic(val) );
                            break;

                        case UT_INT:
                            emitOp2( DP_UNIFORM_1I, loc, ur_int(val) );
                            break;

                        case UT_DOUBLE:
                        {
                            Number n;
                            n.f = ur_double(val);
                            emitOp2( DP_UNIFORM_1F, loc, n.i );
                        }
                            break;

                        case UT_VEC3:
                        {
                            const uint32_t* vp =
                                (const uint32_t*) &val->vec3.xyz;
                            emitDPInst( emit, DP_UNIFORM_3F, 4, loc,
                                        vp[0], vp[1], vp[2] );
                        }
                            break;
                    }
                }
            }
                break;

            case DOP_FRAMEBUFFER:   // framebuffer!
                INC_PC
                PC_VALUE(val)
                emitOp1( DP_FRAMEBUFFER, ur_is(val, UT_FBO) ?
                                            ur_fboId(val) : 0 );
                break;

            case DOP_FRAMEBUFFER_TEX:   // attach int!/word!  name get-word!
            {
                GLenum attach = 0;
                INC_PC
                if( ur_is(pc, UT_INT) )
                {
                    attach = GL_COLOR_ATTACHMENT0 + ur_int(pc);
                }
                else if( ur_is(pc, UT_WORD ) )
                {
                    if( ur_atom(pc) == UR_ATOM_COLOR )
                        attach = GL_COLOR_ATTACHMENT0;
                    else if( ur_atom(pc) == UR_ATOM_DEPTH )
                        attach = GL_DEPTH_ATTACHMENT;
                }
                if( ! attach )
                {
                    typeError( "framebuffer-tex expected"
                               " int!/color/depth attachment" );
                }
                INC_PC
                if( ! emitWordOp( emit, pc, DP_FRAMEBUFFER_TEX_WORD ) )
                {
                    typeError( "framebuffer-tex expected texture word!" );
                }
                emitDPArg( emit, attach );
            }
                break;

            case DOP_SHADOW_BEGIN:
                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_FBO) )
                {
                    emitOp1( DP_SHADOW_BEGIN, ur_fboId(val) );
                }
                else
                {
                    typeError( "shadow-begin expected fbo!" );
                }
                break;

            case DOP_SHADOW_END:
                emitOp( DP_SHADOW_END );
                break;

            case DOP_SAMPLES_QUERY:     // result word! dlist block!
            {
                INC_PC
                if( ! ur_is(pc, UT_WORD) )
                {
samples_err:
                    typeError( "samples-query expected word! block!" );
                }
                val = pc;
                INC_PC
                if( ! ur_is(pc, UT_BLOCK) )
                    goto samples_err;

                emitOp( DP_SAMPLES_QUERY );
                if( dp_compile( emit, ut, pc->series.buf ) != UR_OK )
                    goto error;
                emitWordOp( emit, val, DP_SAMPLES_END );
            }
                break;

            case DOP_SAMPLES_BEGIN:
                emitOp( DP_SAMPLES_BEGIN );
                break;

            case DOP_BUFFER:        // buffer attr-block vector!/vertex-buffer!
            {
                DPBuffer* vbuf;
                UIndex keyN;

                INC_PC
                PC_VALUE(val)
                if( ! ur_is(val, UT_BLOCK) )
                {
                    typeError( "buffer expected attribute block!" );
                }
                keyN = val->series.buf;

                INC_PC
                PC_VALUE(val)
                if( ur_is(val, UT_VECTOR) )
                {
                    if( emit->vbufCount == DP_MAX_VBUF )
                    {
                        scriptError( "Vertex buffer limit exceeded" );
                    }
                    vbuf = emit->vbuf + emit->vbufCount;
                    ++emit->vbufCount;

                    switch( option )
                    {
                        case UR_ATOM_STREAM:
                            vbuf->usage = GL_STREAM_DRAW;
                            break;
                        case UR_ATOM_DYNAMIC:
                            vbuf->usage = GL_DYNAMIC_DRAW;
                            break;
                        default:
                            vbuf->usage = GL_STATIC_DRAW;
                            break;
                    }
                    vbuf->keyN = keyN;
                    vbuf->vecN = val->series.buf;
                }
                else if( ur_is(val, UT_VBO) )
                {
                    UBuffer* res = ur_buffer( ur_vboResN(val) );
                    GLuint* buf = vbo_bufIds(res);
                    if( vbo_count(res) )
                    {
                        refVBO( ur_vboResN(val) );
                        emitOp1( DP_BIND_ARRAY, buf[0] );
                        if( ! emitBufferOffsets( ut, emit, ur_buffer(keyN) ) )
                            goto error;
                    }
                }
                else
                {
                    typeError( "buffer expected vector!/vertex-buffer!" );
                }
            }
                break;

            case DOP_DEPTH_TEST:
                INC_PC
                emitOp( (ur_atom(pc) == UR_ATOM_OFF) ?
                        DP_DEPTH_OFF : DP_DEPTH_ON );
                break;

            case DOP_BLEND:
            {
                int op;
                INC_PC
                switch( ur_atom(pc) )
                {
                    case UR_ATOM_ON:    op = DP_BLEND_ON;    break;
                    case UR_ATOM_OFF:   op = DP_BLEND_OFF;   break;
                    case UR_ATOM_ADD:   op = DP_BLEND_ADD;   break;
                    case UR_ATOM_BURN:  op = DP_BLEND_BURN;  break;
                    case UR_ATOM_TRANS: op = DP_BLEND_TRANS; break;
                    default:
                        typeError( "blend expected word!"
                                   " (on off add burn trans)" );
                }
                emitOp( op );
            }
                break;

            case DOP_CULL:
                INC_PC
                emitOp( (ur_atom(pc) == UR_ATOM_OFF) ?
                        DP_CULL_OFF : DP_CULL_ON );
                break;

            case DOP_COLOR_MASK:
                INC_PC
                emitOp( (ur_atom(pc) == UR_ATOM_OFF) ?
                        DP_COLOR_MASK_OFF : DP_COLOR_MASK_ON );
                break;

            case DOP_DEPTH_MASK:
                INC_PC
                emitOp( (ur_atom(pc) == UR_ATOM_OFF) ?
                        DP_DEPTH_MASK_OFF : DP_DEPTH_MASK_ON );
                break;

            case DOP_POINT_SIZE:
                INC_PC
                emitOp( (ur_atom(pc) == UR_ATOM_OFF) ?
                        DP_POINT_SIZE_OFF : DP_POINT_SIZE_ON );
                break;

            case DOP_POINT_SPRITE:
                INC_PC
                emitOp( (ur_atom(pc) == UR_ATOM_OFF) ?
                        DP_POINT_SPRITE_OFF : DP_POINT_SPRITE_ON );
                break;

            case DOP_READ_PIXELS:
            {
                const UCell* rect;
                const uint32_t* pp;
                INC_PC
                if( ! ur_is(pc, UT_COORD) )
                {
                    scriptError( "read-pixels expected rect" );
                }
                rect = pc;
                INC_PC
                if( ! emitWordOp( emit, pc, DP_READ_PIXELS ) )
                {
                    typeError( "read-pixels expected word!" );
                }
                pp = (const uint32_t*) rect->coord.n;
                emitDPArg( emit, pp[0] );
                emitDPArg( emit, pp[1] );
            }
                break;

            default:
                goto bad_inst;
            }
        }
        else if( ur_is(pc, UT_PATH) )
        {
            const UBuffer* blk = ur_bufferSer(pc);
            const UCell* pit = blk->ptr.cell;

            if( blk->used != 2 )
                goto bad_inst;

            option = ur_atom(pit + 1);
            opcode = ur_atomsSearch(glEnv.drawOpTable, DOP_COUNT, ur_atom(pit));
            goto comp_op;
        }
        else if( ur_is(pc, UT_PAREN) )
        {
            refBlock( emit, pc->series.buf );
            emitOp1( DP_EVAL_BLOCK, pc->series.buf );
        }
        else
        {
bad_inst:
            scriptError( "Invalid draw-prog instruction" );
        }
        ++pc;
    }
    return UR_OK;

end_of_block:

    return ur_error( ut, UR_ERR_SCRIPT, "unexpected end of draw-prog" );

error:

    ur_appendTrace( ut, blkN, pc - blk->ptr.cell );
    return UR_THROW;
}


/*
*/
DPHeader* dp_finish( UThread* ut, DPCompiler* emit )
{
    DPHeader* ph;

    // Terminate program.
    emitOp( DP_END );
    flushOps( DP_END );

    // Add a single DPResEntry for all block references.
    if( emit->blocks.used )
    {
        dp_addRef( emit, RES_BLOCK, emit->blocks.used,
                   (GLuint*) emit->blocks.ptr.i );
    }

    genIndices( ut, emit );

    geo_bufferData( &emit->tgeo );

    // Put header, resource entries, and program bytecode into a single
    // UBuffer.
    ph = (DPHeader*) memAlloc( sizeof(DPHeader) +
                               emit->ref.used +
                               sizeof(uint32_t) * emit->inst.used );
    if( ph )
    {
        ph->uid       = emit->uid;
        ph->resCount  = emit->refCount;
        ph->progIndex = sizeof(DPHeader) + emit->ref.used;

        if( emit->inst.used )
        {
            memCpy( ph + 1, emit->ref.ptr.v, emit->ref.used );

            memCpy( ((char*) ph) + ph->progIndex, emit->inst.ptr.v,
                    sizeof(uint32_t) * emit->inst.used );
        }
    }

    return ph;
}


/*--------------------------------------------------------------------------*/


DPCompiler* gDPC = 0;


/*
  \return previous compiler.
*/
DPCompiler* ur_beginDP( DPCompiler* dpc /*, UIndex dpResN*/ )
{
    static uint32_t serial = 1;
    DPCompiler* save = gDPC;

    dp_init( dpc );
    dpc->uid = serial++;
    gDPC = dpc;

    return save;
}


/*
  \return UR_OK/UR_THROW
*/
int ur_compileDP( UThread* ut, const UCell* blkCell, int handleError )
{
    int ok;
    assert( gDPC );
    ok = dp_compile( gDPC, ut, blkCell->series.buf );
    if( (ok == UR_THROW) && handleError )
    {
        UBuffer str;
        const UCell* ex = ur_exception( ut );

        ur_strInit( &str, UR_ENC_UTF8, 0 );
        ur_toStr( ut, ex, &str, 0 );
        ur_strTermNull( &str );
        fputs( str.ptr.c, stderr );
        ur_strFree( &str );

        //UR_S_DROP;
        //UR_CALL_OP = 0;
    }
    return ok;
}


void ur_endDP( UThread* ut, UBuffer* res, DPCompiler* prev )
{
    assert( gDPC );

    dprog_destroy( res );
    res->ptr.v = dp_finish( ut, gDPC );

    dp_free( gDPC );
    gDPC = prev;
}


void ur_setDPSwitch( UThread* ut, UIndex resN, DPSwitch sid, int n )
{
    DPHeader* dp = (DPHeader*) ur_buffer(resN)->ptr.v;
    if( dp /*&& (dp->uid == uid)*/ )
        dp_setSwitch( dp, sid, n );
}


/*--------------------------------------------------------------------------*/



static void disableVertexAttrib( struct ClientState* cs )
{
    int f = cs->flags;

    // Always leaving ALOC_VERTEX enabled.

    if( f & CSA_NORMAL )
        glDisableVertexAttribArray( ALOC_NORMAL );
    if( f & CSA_COLOR )
        glDisableVertexAttribArray( ALOC_COLOR );
    if( f & CSA_UV )
        glDisableVertexAttribArray( ALOC_TEXTURE );

    for( f = 0; f < cs->attrCount; ++f )
        glDisableVertexAttribArray( cs->attr[ f ] );
}


#if 0
#define REPORT(str)             printf(str)
#define REPORT_1(str,v1)        printf(str,v1)
#define REPORT_2(str,v1,v2)     printf(str,v1,v2)
#else
#define REPORT(str)
#define REPORT_1(str,v1)
#define REPORT_2(str,v1,v2)
#endif


void ur_initDrawState( DPState* state )
{
    state->client.flags =
    state->client.attrCount = 0;

    state->samplesQueryId = 0;
    state->currentProgram = 0;
    state->font = 0;
}


/*--------------------------------------------------------------------------*/


/*
double number_d( const UCell* cell )
{
    if( ur_is(cell, UT_DOUBLE) )
        return ur_double(cell);
    if( ur_is(cell, UT_INT) )
        return (double) ur_int(cell);
    return 0.0;
}
*/


//static GLfloat* _cameraMatrix = 0;

void dop_camera( UThread* ut, UIndex ctxValBlk )
{
    GLfloat mat[16];
    double w, h;
    double zNear, zFar;
    const UCell* val;
    const UBuffer* blk = ur_bufferE( ctxValBlk );
    const UCell* ctxCells = blk->ptr.cell;

    val = ctxCells + CAM_CTX_VIEWPORT;
    if( ur_is(val, UT_COORD) && (val->coord.len > 3) )
    {
        // Coord elements: x, y, width, height.

        glViewport( val->coord.n[0], val->coord.n[1],
                    val->coord.n[2], val->coord.n[3] );

        w = (double) val->coord.n[2];
        h = (double) val->coord.n[3];

        val = ctxCells + CAM_CTX_CLIP;
        if( ur_is(val, UT_VEC3) )
        {
            zNear = val->vec3.xyz[0];
            zFar  = val->vec3.xyz[1];
        }
        else
        {
            zNear =  0.1;
            zFar  = 10.0;
        }

        glMatrixMode( GL_PROJECTION );

        val = ctxCells + CAM_CTX_FOV;
        if( ur_is(val, UT_DOUBLE) )
        {
            ur_perspective( mat, ur_double(val), w / h, zNear, zFar );
            glLoadMatrixf( mat );
        }
        else if( ur_is(val, UT_WORD) )
        {
            // 'pixels sets projection to show viewport pixels at a 1:1 ratio.
            glLoadIdentity();
            glOrtho( 0, w, 0, h, zNear, zFar );
        }
        else if( ur_is(val, UT_VEC3) )
        {
            // The FOV vec3 x,y are the orthographic left & right.
            // Bottom & top are calculated from this and the viewport aspect.
            double left  = val->vec3.xyz[0];
            double right = val->vec3.xyz[1];
            h = ((right - left) * h / w) * 0.5;
            glLoadIdentity();
            glOrtho( left, right, -h, h, zNear, zFar );
        }

        glMatrixMode( GL_MODELVIEW );

        val = ctxCells + CAM_CTX_ORIENT;
        if( ur_is(val, UT_VECTOR) )
        {
            const UBuffer* arr = ur_bufferSer(val);
            if( (arr->form == UR_VEC_F32) && (arr->used == 16) )
            {
                // glLoadTransposeMatrixf() could be used in GL 2.0?
                //_cameraMatrix = arr->ptr.f;
                ur_matrixInverse( mat, arr->ptr.f );
                glLoadMatrixf( mat );
                return;
            }
        }
        glLoadIdentity();
    }
}


#ifndef GL_ES_VERSION_2_0
static void _lightv3( GLenum light, GLenum pname, const UCell* cell, GLfloat w )
{
    GLfloat pos[4];

    pos[0] = cell->vec3.xyz[0];
    pos[1] = cell->vec3.xyz[1];
    pos[2] = cell->vec3.xyz[2];
    pos[3] = w;

    //printf( "KR light %d %d %g\n", light, pname, w );
    glLightfv( light, pname, pos ); 
}


static void _lightEn( GLenum light, const UCell* cell )
{
    if( ur_logic(cell) )
        glEnable( light );
    else
        glDisable( light );
}


void dop_light( UThread* ut, const UCell* val, int light )
{
    if( ur_is(val, UT_LOGIC) )
    {
        _lightEn( light, val );
    }
    else if( ur_is(val, UT_VEC3) )
    {
        _lightv3( light, GL_POSITION, val, 0.0f );
    }
    else if( ur_is(val, UT_BLOCK) )
    {
        UBlockIt bi;
        GLenum name = GL_POSITION;
        GLfloat w = 0.0f;

        ur_blockIt( ut, &bi, val );
        ur_foreach( bi )
        {
            if( ur_isWordType( ur_type(bi.it) ) )
            {
                switch( ur_atom(bi.it) )
                {
                    case UR_ATOM_AMBIENT: name = GL_AMBIENT;  w = 1.0f; break;
                    case UR_ATOM_DIFFUSE: name = GL_DIFFUSE;  w = 1.0f; break;
                    case UR_ATOM_POS:     name = GL_POSITION; w = 0.0f; break;

                    default:
                        val = ur_wordCell( ut, bi.it );
                        if( val )
                        {
                            if( ur_is(val, UT_LOGIC) )
                                _lightEn( light, val );
                            else if( ur_is(val, UT_VEC3) )
                                _lightv3( light, name, val, w );
                        }
                        break;
                }
            }
            else if( ur_is(bi.it, UT_VEC3) )
            {
                _lightv3( light, name, bi.it, w );
            }
            else if( ur_is(bi.it, UT_LOGIC) )
            {
                _lightEn( light, bi.it );
            }
        }
    }
}
#endif


#ifndef GL_ES_VERSION_2_0
//#define SHADOW_BACK_FACES   1

static GLfloat lightViewMatrix[ 16 ];
static GLfloat lightProjMatrix[ 16 ];


/*
   Camera must be set up for light before calling shadow-begin.
*/
void dop_shadow_begin( GLuint fbo )
{
    // Save matrices for shadow-end.
    glGetFloatv( GL_MODELVIEW_MATRIX, lightViewMatrix );
    glGetFloatv( GL_PROJECTION_MATRIX, lightProjMatrix );


    glDepthMask( GL_TRUE );
    glEnable( GL_DEPTH_TEST );
    glDisable( GL_BLEND );
    glDisable( GL_LIGHTING );
    //gr_disableTexture();


    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    glClear( GL_DEPTH_BUFFER_BIT );

#ifdef SHADOW_BACK_FACES
    glEnable( GL_CULL_FACE );
    glCullFace( GL_FRONT );
#else
    glEnable( GL_POLYGON_OFFSET_FILL );
    //glPolygonOffset( polygon_offset_scale, polygon_offset_bias );
    //glPolygonOffset( 2.0f, 10.0f );
    glPolygonOffset( 2.0f, 4.0f );
#endif
}


// Transform from -1..1 to 0..1
const GLfloat bias[] =
{
    0.5, 0.0, 0.0, 0.0, 
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0
};


void dop_shadow_end()
{
#ifdef SHADOW_BACK_FACES
    glCullFace( GL_BACK );
#else
    glDisable( GL_POLYGON_OFFSET_FILL );
#endif
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );


    glActiveTexture( GL_TEXTURE1 );
    glMatrixMode( GL_TEXTURE );

#if 1
    glLoadMatrixf( bias );
#else
    glLoadIdentity();
#endif
    glMultMatrixf( lightProjMatrix );
    glMultMatrixf( lightViewMatrix );
    //glMultMatrixf( _cameraMatrix );

    glMatrixMode( GL_MODELVIEW );
    glActiveTexture( GL_TEXTURE0 );
}
#endif


/*--------------------------------------------------------------------------*/


/*
  \param ds     Parent draw program state.  May be zero.
  \param n      Draw program buffer index.

  \return UR_OK/UR_THROW.
*/
int ur_runDrawProg2( UThread* ut, DPState* ds, UIndex n )
{
    DPHeader* ph;
    const UBuffer* blk;
    UCell* val;
    uint32_t* pc;
    uint32_t ops = 0;

#define PC_WORD \
    blk = ur_buffer( *pc++ ); \
    val = blk->ptr.cell + *pc++

    blk = ur_buffer( n );
    if( blk->flags )    // UR_DRAWPROG_HIDDEN
        return UR_OK;
    ph = (DPHeader*) blk->ptr.v;
    if( ! ph )
        return UR_OK;
    pc = dp_byteCode(ph); 
    ops = *pc++;

    REPORT_2( "\ndraw_prog %d %d\n", n, *pc );

    while( 1 )
    {
dispatch:
        switch( (uint8_t) ops )
        {
        case DP_EMPTY:
            ops = *pc++;
            goto dispatch;

        case DP_NOP:
            n = 0;
            break;

        case DP_END:
            REPORT( "END\n" );
            return UR_OK;

        case DP_JMP:
            REPORT_1( "JMP %d\n", *((int*) pc) );
            pc += *((int*) pc);
            ops = *pc++;
            goto dispatch;

        case DP_SWITCH:
        {
            uint16_t* sp = (uint16_t*) pc;
            REPORT_2( "SWITCH %ld %d\n", pc - dp_byteCode(ph), *sp );
            pc += sp[ *sp ];
            ops = *pc++;
        }
            goto dispatch;

        case DP_RUN_PROGRAM:
            REPORT_1( "RUN_PROGRAM %d\n", pc[0] );
            {
            if( ! ur_runDrawProg2( ut, ds, *pc++ ) )
                return UR_THROW;
            }
            break;

        case DP_RUN_PROGRAM_WORD:
        {
            PC_WORD;
            if( ur_is(val, UT_DRAWPROG) )
            {
                if( ! ur_runDrawProg2( ut, ds, ur_drawProgN(val) ) )
                    return UR_THROW;
            }
            else if( ur_is(val, UT_WIDGET) )
            {
                GWidget* wp = ur_widgetPtr(val);
                // NOTE: Draw programs may be compiled in GWidget render.
                wp->wclass->render( wp );
            }
        }
            break;

        case DP_EVAL_BLOCK:
        {
            UCell* res = ut->stack.ptr.cell + ut->stack.used - 1;
            UCell tmp;
            ur_initSeries(&tmp, UT_BLOCK, *pc++);

            if( ! boron_doBlock( ut, &tmp, res ) )
                return UR_THROW;
        }
            break;
#if 0
        case DP_RENDER_WIDGET:
            // NOTE: Compilation of draw lists will happen in gui_render().
            gui_render( &gxEnv.gui, *pc++ );
            break;
#endif
        case DP_CLEAR:
            glDepthMask( GL_TRUE );
            glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
            glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
            break;

        case DP_CAMERA:
            dop_camera( ut, *pc++ );
            break;

        case DP_SHADER:
        {
            GLuint prog = *pc++;
            if( ds->currentProgram != prog )
            {
                ds->currentProgram = prog;
                glUseProgram( prog );
            }
        }
            break;

        case DP_SHADER_CTX:
        {
            UCell cc;
            const UBuffer* blk;
            const Shader* sh;
            GLuint prog = *pc++;

            if( ds->currentProgram != prog )
            {
                ds->currentProgram = prog;
                glUseProgram( prog );
            }

            ur_setId( &cc, UT_CONTEXT );
            cc.series.buf = *pc++;
            sh = shaderContext( ut, &cc, &blk );
            if( sh && blk )
                setShaderUniforms( sh, blk );
        }
            break;

        case DP_UNIFORM_1I:
            REPORT_2( "UNIFORM_1I %d %d\n", pc[0], pc[1] );
            glUniform1i( pc[0], pc[1] );
            pc += 2;
            break;

        case DP_UNIFORM_1F:
            REPORT_2( "UNIFORM_1F %d %f\n", pc[0], *((GLfloat*) (pc+1)) );
            glUniform1f( pc[0], *((GLfloat*) (pc+1)) );
            pc += 2;
            break;

        case DP_UNIFORM_3F:
            REPORT_2( "UNIFORM_3F %d %f ...\n", pc[0], *((GLfloat*) (pc+1)) );
            glUniform3fv( pc[0], 1, (GLfloat*) (pc+1) );
            pc += 4;
            break;

        case DP_BIND_TEXTURE:
            REPORT_2( "BIND_TEXTURE %d %d\n", pc[0], pc[1] );
            glActiveTexture( pc[0] /*GL_TEXTURE0*/ );
            glBindTexture( GL_TEXTURE_2D, pc[1] );
            pc += 2;
            break;

        case DP_BIND_ARRAY:
            REPORT_1( "BIND_ARRAY %d\n", *pc );
#if 1
            if( ds->client.flags )
            {
                disableVertexAttrib( &ds->client );
                ds->client.flags = 0;
                ds->client.attrCount = 0;
            }
#endif
            glBindBuffer( GL_ARRAY_BUFFER, *pc++ );
            break;

        case DP_BIND_ELEMENTS:
            REPORT_1( "BIND_ELEMENTS %d\n", *pc );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, *pc++ );
            break;

        case DP_VERTEX_OFFSET:
        {
            uint32_t stof = *pc++;
            REPORT_1( " VERTEX_OFFSET %08x\n", stof );
            glVertexAttribPointer( ALOC_VERTEX, 3, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_VERTEX_OFFSET_4:
        {
            uint32_t stof = *pc++;
            REPORT_1( " VERTEX_OFFSET_4 %08x\n", stof );
            glVertexAttribPointer( ALOC_VERTEX, 3, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_NORMAL_OFFSET:
        {
            uint32_t stof = *pc++;
            REPORT_1( " NORMAL_OFFSET %08x\n", stof );
            glEnableVertexAttribArray( ALOC_NORMAL );
            ds->client.flags |= CSA_NORMAL;
            glVertexAttribPointer( ALOC_NORMAL, 3, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_COLOR_OFFSET:
        {
            uint32_t stof = *pc++;
            REPORT_1( " COLOR_OFFSET %08x\n", stof );
            glEnableVertexAttribArray( ALOC_COLOR );
            ds->client.flags |= CSA_COLOR;
            glVertexAttribPointer( ALOC_COLOR, 3, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_COLOR_OFFSET_4:
        {
            uint32_t stof = *pc++;
            REPORT_1( " COLOR_OFFSET_4 %08x\n", stof );
            glEnableVertexAttribArray( ALOC_COLOR );
            ds->client.flags |= CSA_COLOR;
            glVertexAttribPointer( ALOC_COLOR, 4, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_UV_OFFSET:
        {
            uint32_t stof = *pc++;
            REPORT_1( " UV_OFFSET %08x\n", stof );
            glEnableVertexAttribArray( ALOC_TEXTURE );
            ds->client.flags |= CSA_UV;
            glVertexAttribPointer( ALOC_TEXTURE, 2, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_ATTR_OFFSET:
        {
            int size;
            uint32_t loc  = *pc++;
            uint32_t stof = *pc++;
            REPORT_2( " ATTR_OFFSET %08x %08x\n", loc, stof );
            size = loc & 0xff;
            loc >>= 8;
            glEnableVertexAttribArray( loc );
            ds->client.attr[ ds->client.attrCount++ ] = loc;
            glVertexAttribPointer( loc, size, GL_FLOAT, GL_FALSE,
                                   stof & 0xff, NULL + (stof >> 8) );
        }
            break;

        case DP_DRAW_ARR_POINTS:
            REPORT_1( "DRAW_ARR_POINTS %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawArrays( GL_POINTS, 0, *pc++ );
            break;

        case DP_DRAW_POINTS:
            REPORT_1( "DRAW_POINTS %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_POINTS, *pc++, GL_UNSIGNED_SHORT, 0 );
            break;

        case DP_DRAW_LINES:
            REPORT_1( "DRAW_LINES %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_LINES, *pc++, GL_UNSIGNED_SHORT, 0 );
            break;

        case DP_DRAW_LINE_STRIP:
            REPORT_1( "DRAW_LINE_STRIP %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_LINES, *pc++, GL_UNSIGNED_SHORT, 0 );
            break;

        case DP_DRAW_TRIS:
            REPORT_1( "DRAW_TRIS %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_TRIANGLES, *pc++, GL_UNSIGNED_SHORT, 0 );
            break;

        case DP_DRAW_TRI_STRIP:
            REPORT_1( "DRAW_TRI_STRIP %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_TRIANGLE_STRIP, *pc++, GL_UNSIGNED_SHORT, 0 );
            break;

        case DP_DRAW_TRI_FAN:
            REPORT_1( "DRAW_TRI_FAN %d\n", pc[0] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_TRIANGLE_FAN, *pc++, GL_UNSIGNED_SHORT, 0 );
            break;

        case DP_DRAW_POINTS_I:
            REPORT_2( "DRAW_POINTS_I %d %d\n", pc[0], pc[1] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_POINTS, pc[0], GL_UNSIGNED_SHORT,
                            NULL + pc[1] );
            pc += 2;
            break;

        case DP_DRAW_LINES_I:
            REPORT_2( "DRAW_LINES_I %d %d\n", pc[0], pc[1] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_LINES, pc[0], GL_UNSIGNED_SHORT,
                            NULL + pc[1] );
            pc += 2;
            break;

        case DP_DRAW_LINE_STRIP_I:
            REPORT_2( "DRAW_LINE_STRIP_I %d %d\n", pc[0], pc[1] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_LINE_STRIP, pc[0], GL_UNSIGNED_SHORT,
                            NULL + pc[1] );
            pc += 2;
            break;

        case DP_DRAW_TRIS_I:
            REPORT_2( "DRAW_TRIS_I %d %d\n", pc[0], pc[1] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_TRIANGLES, pc[0], GL_UNSIGNED_SHORT,
                            NULL + pc[1] );
            pc += 2;
            break;

        case DP_DRAW_TRI_STRIP_I:
            REPORT_2( "DRAW_TRI_STRIP_I %d %d\n", pc[0], pc[1] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_TRIANGLE_STRIP, pc[0], GL_UNSIGNED_SHORT,
                            NULL + pc[1] );
            pc += 2;
            break;

        case DP_DRAW_TRI_FAN_I:
            REPORT_2( "DRAW_TRI_FAN_I %d %d\n", pc[0], pc[1] );
            ES_UPDATE_MATRIX
            glDrawElements( GL_TRIANGLE_FAN, pc[0], GL_UNSIGNED_SHORT,
                            NULL + pc[1] );
            pc += 2;
            break;

        case DP_DEPTH_ON:
            glEnable( GL_DEPTH_TEST );
            break;

        case DP_DEPTH_OFF:
            glDisable( GL_DEPTH_TEST );
            break;

        case DP_BLEND_ON:
            glEnable( GL_BLEND );
            break;

        case DP_BLEND_OFF:
            glDisable( GL_BLEND );
            break;

        case DP_BLEND_ADD:      // Additive.
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE );
            break;

        case DP_BLEND_BURN:     // Controlled additivity.
            glEnable( GL_BLEND );
            glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
            break;
#if 0
        case DP_BLEND_COLOR:    // ?
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_COLOR, GL_ONE_MINUS_SRC_ALPHA );
            break;
#endif
        case DP_BLEND_TRANS:    // Standard Transparency.
            glEnable( GL_BLEND );
            glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
            break;

        case DP_CULL_ON:
            glEnable( GL_CULL_FACE );
            break;

        case DP_CULL_OFF:
            glDisable( GL_CULL_FACE );
            break;

        case DP_COLOR_MASK_ON:
            glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
            break;

        case DP_COLOR_MASK_OFF:
            glColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE );
            break;

        case DP_DEPTH_MASK_ON:
            glDepthMask( GL_TRUE );
            break;

        case DP_DEPTH_MASK_OFF:
            glDepthMask( GL_FALSE );
            break;
#ifndef GL_ES_VERSION_2_0
        case DP_POINT_SIZE_ON:
            glEnable( GL_VERTEX_PROGRAM_POINT_SIZE );
            break;

        case DP_POINT_SIZE_OFF:
            glDisable( GL_VERTEX_PROGRAM_POINT_SIZE );
            break;

        case DP_POINT_SPRITE_ON:
            glEnable( GL_POINT_SPRITE );
            break;

        case DP_POINT_SPRITE_OFF:
            glDisable( GL_POINT_SPRITE );
            break;
#endif
        case DP_COLOR3:
        {
            uint8_t* cp = (uint8_t*) pc++;
            glUniform4f( ULOC_COLOR, ((GLfloat) cp[0]) / 255.0f,
                                     ((GLfloat) cp[1]) / 255.0f,
                                     ((GLfloat) cp[2]) / 255.0f, 1.0f );
            REPORT_1("COLOR3 %08X\n", pc[-1]);
            //glColor3ubv( (GLubyte*) pc++ );
        }
            break;

        case DP_COLOR4:
        {
            uint8_t* cp = (uint8_t*) pc++;
            glUniform4f( ULOC_COLOR, ((GLfloat) cp[0]) / 255.0f,
                                     ((GLfloat) cp[1]) / 255.0f,
                                     ((GLfloat) cp[2]) / 255.0f,
                                     ((GLfloat) cp[3]) / 255.0f );
            REPORT_1("COLOR4 %08X\n", pc[-1]);
            //glColor4ubv( (GLubyte*) pc++ );
        }
            break;

        case DP_COLOR_WORD:
        {
#ifdef GL_ES_VERSION_2_0
            pc += 2;
#else
            uint8_t color[ 4 ];
            PC_WORD;
            if( ur_is(val, UT_VEC3) )
                glColor3fv( val->vec3.xyz );
            else if( cellToColorUB( val, color ) == 4 )
                glColor4ubv( color );
            else
                glColor3ubv( color );
#endif
        }
            break;

        case DP_PUSH:
            REPORT("PUSH");
            glPushMatrix();
            break;

        case DP_POP:
            REPORT("POP");
            glPopMatrix();
            break;

        case DP_TRANSLATE:
        {
            Number x, y, z;
            x.i = *pc++;
            y.i = *pc++;
            z.i = *pc++;
            glTranslatef( x.f, y.f, z.f );
        }
            break;

        case DP_TRANSLATE_WORD:
        {
            PC_WORD;
            if( ur_is(val, UT_VEC3) )
            {
                glTranslatef( val->vec3.xyz[0],
                              val->vec3.xyz[1],
                              val->vec3.xyz[2] );
            }
        }
            break;

        case DP_TRANSLATE_XY:
        {
            Number x, y;
            x.i = *pc++;
            y.i = *pc++;
            REPORT_2( "TRANSLATE_XY %f %f\n", x.f, y.f );
            glTranslatef( x.f, y.f, 0.0f );
        }
            break;

        case DP_ROTATE_X:
        {
            Number n;
            n.i = *pc++;
            glRotatef( n.f, 1.0, 0.0, 0.0 );
        }
            break;

        case DP_ROTATE_Y:
        {
            Number n;
            n.i = *pc++;
            glRotatef( n.f, 0.0, 1.0, 0.0 );
        }
            break;

        case DP_ROTATE_Z:
        {
            Number n;
            n.i = *pc++;
            glRotatef( n.f, 0.0, 0.0, 1.0 );
        }
            break;

        case DP_ROTATE_X_WORD:
        {
            PC_WORD;
            glRotatef( (GLfloat) ur_double( val ), 1.0, 0.0, 0.0 );
        }
            break;

        case DP_ROTATE_Y_WORD:
        {
            PC_WORD;
            glRotatef( (GLfloat) ur_double( val ), 0.0, 1.0, 0.0 );
        }
            break;

        case DP_ROTATE_Z_WORD:
        {
            PC_WORD;
            glRotatef( (GLfloat) ur_double( val ), 0.0, 0.0, 1.0 );
        }
            break;

        case DP_ROTATE_WORD:
        {
            PC_WORD;
            if( ur_is(val, UT_QUAT) )
            {
                float mat[16];
                quat_toMatrix( val, mat, 1 );
                glMultMatrixf( mat );
            }
            else if( ur_is(val, UT_VECTOR) )
            {
                const UBuffer* mat = ur_bufferSer( val );
                // Assuming (mat->form == UR_VEC_F32 && used == 16).
                glMultMatrixf( mat->ptr.f );
            }
        }
            break;

        case DP_SCALE_WORD:
        {
            PC_WORD;
            REPORT_2( "SCALE_WORD %d %d\n", pc[-2], pc[-1] );
            if( ur_is(val, UT_VEC3) )
            {
                glScalef( val->vec3.xyz[0],
                          val->vec3.xyz[1],
                          val->vec3.xyz[2] );
            }
            else if( ur_is(val, UT_DOUBLE) )
            {
                GLfloat n = ur_double( val );
                glScalef( n, n, n );
            }
        }
            break;

        case DP_SCALE_1:
        {
            Number n;
            n.i = *pc++;
            REPORT_1( "SCALE_1 %g\n", n.f );
            glScalef( n.f, n.f, n.f );
        }
            break;

        case DP_FRAMEBUFFER:
            glBindFramebuffer( GL_FRAMEBUFFER, *pc++ );
            break;

        case DP_FRAMEBUFFER_TEX_WORD:
        {
            GLenum attach;
            PC_WORD;
            attach = *pc++;
            if( ur_is(val, UT_TEXTURE) )
            {
                glFramebufferTexture2D( GL_FRAMEBUFFER, attach,
                                        GL_TEXTURE_2D, ur_texId(val), 0 );
            }
        }
            break;

#ifndef GL_ES_VERSION_2_0
        case DP_SHADOW_BEGIN:
            dop_shadow_begin( *pc++ );
            break;

        case DP_SHADOW_END:
            dop_shadow_end();
            break;

        case DP_SAMPLES_QUERY:
            ds->samplesQueryId = 0;
            break;

        case DP_SAMPLES_BEGIN:
            if( ds->samplesQueryId )
                glEndQuery( GL_SAMPLES_PASSED );
            ++ds->samplesQueryId;
            glBeginQuery( GL_SAMPLES_PASSED, ds->samplesQueryId );
            break;

        case DP_SAMPLES_END:
        {
            PC_WORD;
            if( ds->samplesQueryId )
            {
                GLuint samples;
                GLuint id = 1;

                glEndQuery( GL_SAMPLES_PASSED );

                if( ur_is(val, UT_VECTOR) )
                {
                    int used;

                    // if( ur_isShared() )
                    blk = ur_buffer( val->series.buf );
                    used = ds->samplesQueryId;
                    if( used > blk->used )
                        used = blk->used;

                    // Poke query results into result vector!
                    if( blk->form == UR_VEC_I32 )
                    {
                        int32_t* it  = blk->ptr.i;
                        int32_t* end = it + used;
                        for( ; it != end; ++id )
                        {
                            glGetQueryObjectuiv(id, GL_QUERY_RESULT, &samples);
                            *it++ = samples;
                        }
                    }
                    else if( blk->form == UR_VEC_F32 )
                    {
                        float* it  = blk->ptr.f;
                        float* end = it + used;
                        for( ; it != end; ++id )
                        {
                            glGetQueryObjectuiv(id, GL_QUERY_RESULT, &samples);
                            *it++ = (float) samples;
                        }
                    }
                }
                else
                {
                    glGetQueryObjectuiv( id, GL_QUERY_RESULT, &samples );
                    ur_setId(val, UT_INT);
                    ur_int(val) = samples;
                }
            }
        }
            break;

        case DP_LIGHT:
        {
            UCell cell;
            ur_initSeries( &cell, UT_BLOCK, *pc++ );

            dop_light( ut, &cell, GL_LIGHT0 );
        }
            break;

        case DP_READ_PIXELS:
        {
            uint16_t* rect;
            PC_WORD;
            if( ur_is(val, UT_VBO) )
            {
                UBuffer* res = ur_buffer( ur_vboResN(val) );
                GLuint* buf = vbo_bufIds(res);
                if( vbo_count(res) )
                {
                    rect = (uint16_t*) pc;
                    glBindBuffer( GL_PIXEL_PACK_BUFFER, buf[0] );
                    //glReadBuffer( GL_BACK );
                    glReadPixels( rect[0], rect[1], rect[2], rect[3],
                                  GL_RGBA, GL_FLOAT, NULL );
                    glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
                }
            }
            pc += 2;
        }
            break;
#endif

        case DP_FONT:
            ds->font = ur_buffer( *pc++ )->ptr.v;
            break;

        case DP_TEXT_WORD:
        {
            UBinaryIter bi;
            GLfloat* pen;
            int attrOffset;
            int cc;

            PC_WORD;
            attrOffset = *pc++;
            pen = (GLfloat*) pc;
            pc += 2;

            dp_toString( ut, val, &bi );
            cc = bi.end - bi.it;
            if( cc > 0 )
            {
                DrawTextState dts;
                GLfloat* attr;
                int bytes;
                //GLenum err;

                if( cc > MAX_DYNAMIC_TEXT_LEN )
                {
                    cc = MAX_DYNAMIC_TEXT_LEN;
                    bi.end = bi.it + MAX_DYNAMIC_TEXT_LEN;
                }

                bytes = cc * (4 * 5 * sizeof(GLfloat));
                ur_binReserve( &glEnv.tmpBin, bytes );
                attr = glEnv.tmpBin.ptr.f;
                vbo_drawTextInit( &dts, ds->font, pen[0], pen[1] );
                cc = vbo_drawText( &dts,
                                   // geo->uvOff, geo->vertOff, geo->attrSize,
                                   attr + 0, attr + 2, 5,
                                   bi.it, bi.end );

                // Change the following DP_DRAW_TRIS_I count.
                if( (uint8_t) (ops >> 8) == DP_DRAW_TRIS_I )
                    pc[0] = cc * 6;
                else
                    pc[1] = cc * 6;

                glBufferSubData( GL_ARRAY_BUFFER, attrOffset, bytes, attr );
                /*
                err = glGetError();
                if( err != GL_NO_ERROR )
                {
                    return ur_error( ut, UR_ERR_INTERNAL, gl_errorString(err) );
                }
                */
            }
        }
            break;

        default:
            return ur_error( ut, UR_ERR_SCRIPT,
                             "Invalid draw-prog opcode (%02X)", ops & 0xff );
        }
        ops >>= 8;      // Next Op
    }
}


int ur_runDrawProg( UThread* ut, UIndex n )
{
    DPState state;
    int ok;

    ur_initDrawState( &state );

    // NOTE: glDrawElements has been seen to segfault if non-existant
    //       client arrays are enabled (when glEnableClientState was used).

    glEnableVertexAttribArray( ALOC_VERTEX );

    ok = ur_runDrawProg2( ut, &state, n );
    if( state.client.flags )
        disableVertexAttrib( &state.client );
    return ok;
}


/*EOF*/
