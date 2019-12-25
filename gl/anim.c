/*
  Boron Animation
  Copyright 2019 Karl Robillard

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


#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "boron.h"
#include "i_parse_blk.h"
#include "quat.h"
#include "urlan_atoms.h"
#include "gl_atoms.h"


extern void ur_markBlkN( UThread* ut, UIndex blkN );

#if 0
#define AR_REPORT(...)  printf(__VA_ARGS__)
#else
#define AR_REPORT(...)
#endif


typedef struct
{
    union
    {
        struct {
            UIndex bufN;
            UIndex i;
        } cell;
        float* pf;
    } out;
    UIndex   updateBlkN;
    UIndex   finishBlkN;
    float*   curve;
    float    curvePos;
    float    timeScale;     // 1.0 / duration
    float    fraction;      // Temporary result of curve calculation.
    float    v1[4];         // Start value
    float    v2[4];         // End value
    uint16_t curveLen;
    uint16_t behavior;      // Loop, LoopBack, Ping, Pong, PingPong
    uint16_t repeatCount;
    uint8_t  outType;
    uint8_t  allocState;
}
Anim;                       // sizeof(Anim) = 80


#define KEY_TABLE(an)   ((uint16_t*) &(an)->fraction)
#define KEY_RESULT(an)  (an)->v2


enum AnimBehavior
{
    ANIM_ONCE,
    ANIM_LOOP,
    ANIM_COMPLETE,          // Only used within anim_process*().
    ANIM_UNSET,
    ANIM_DISABLED = 0x80
};


enum AnimOutput
{
    ANIM_OUT_VECTOR_1,      // out.cell.bufN is a vector!
    ANIM_OUT_VECTOR_3,      // out.cell.bufN is a vector!
    ANIM_OUT_MATRIX_ROT,    // out.cell.bufN is a vector!
    ANIM_OUT_MATRIX_TRANS,  // out.cell.bufN is a vector!
    ANIM_OUT_CELL_VEC3,     // out.cell.bufN is a block!/context!
    ANIM_OUT_CELL_DOUBLE    // out.cell.bufN is a block!/context!
};

static const char anim_keySize[6] = { 2, 4, 5, 4, 4, 2 };


enum AnimAllocState
{
    ANIM_FREE,
    ANIM_SINGLE_USE,
    ANIM_USED
};


enum AnimFuncType
{
    AFT_HERMITE,
    AFT_LINEAR_KEY,
    AFT_COUNT
};


typedef struct
{
    UBuffer buf[ AFT_COUNT ];
    UIndex  firstFree[ AFT_COUNT ];
}
AnimList;


void anim_initList( AnimList* list )
{
    UIndex* ff = list->firstFree;
    int i;
    for( i = 0; i < AFT_COUNT; ++i )
    {
        ur_arrInit( list->buf + i, sizeof(Anim), 0 );
        *ff++ = -1;
    }
}


void anim_cleanupList( AnimList* list )
{
    UIndex* ff = list->firstFree;
    int i;
    for( i = 0; i < AFT_COUNT; ++i )
    {
        ur_arrFree( list->buf + i );
        *ff++ = -1;
    }
}


#define ID_MAKE(ft,n)   (((n) << 4) | ft)
#define ID_FTYPE(id)    (id & 15)
#define ID_INDEX(id)    (id >> 4)

Anim* anim_alloc( AnimList* list, enum AnimFuncType ftype, uint32_t* idP )
{
    Anim* anim;
    UBuffer* buf = list->buf + ftype;
    UIndex n = list->firstFree[ ftype ];

    if( n < 0 )
    {
        n = buf->used;
        ur_arrExpand1(Anim, buf, anim);
    }
    else
    {
        anim = ur_ptr(Anim, buf) + n;
        list->firstFree[ ftype ] = anim->updateBlkN;
    }
    *idP = ID_MAKE(ftype, n);
    return anim;
}


void anim_free( AnimList* list, uint32_t id )
{
    Anim* anim;
    UBuffer* buf;
    int ftype = ID_FTYPE(id);
    int n = ID_INDEX(id);

    buf = list->buf + ftype;
    anim = ur_ptr(Anim, buf) + n;
    anim->behavior   = ANIM_UNSET;
    anim->allocState = ANIM_FREE;

    if( n == buf->used - 1 )
    {
        --buf->used;
    }
    else
    {
        anim->updateBlkN = list->firstFree[ ftype ];
        list->firstFree[ ftype ] = n;
    }
}


static Anim* anim_pointer( AnimList* list, uint32_t id )
{
    UBuffer* buf = list->buf + ID_FTYPE(id);
    return ur_ptr(Anim, buf) + ID_INDEX(id);
}


/*
void anim_enable( AnimList* list, uint32_t id, int on )
{
    UBuffer* buf = list->buf + ID_FTYPE(id);
    Anim* anim = ur_ptr(Anim, buf) + ID_INDEX(id);
    if( on )
        anim->behavior &= ~ANIM_DISABLED;
    else
        anim->behavior |= ANIM_DISABLED;
}
*/


float calc_hermiteY( const float* a, const float* b,
                     const float* tanA, const float* tanB, float t )
{
    float t2 = t * t;
    float t3 = t * t2;
    float t2x3 = t2 * 3.0f;
    float t3x2 = t3 * 2.0f;
    float res;

    res  = a[1]    * ( t3x2 - t2x3 + 1.0f);
    res += b[1]    * (-t3x2 + t2x3);
    res += tanA[1] * (t3 - 2.0f*t2 + t);
    res += tanB[1] * (t3 - t2);

    return res;
}


#if 0
/*
  https://www.cubic.org/docs/hermite.htm

  \param a     2D start point.
  \param b     2D end point.
  \param tanA  2D vector from a.
  \param tanB  2D vector from b.
  \param t     Position on curve in range 0.0 to 1.0.
  \param res   Result 2D point on curve at t.
*/
void calc_hermite2( const float* a, const float* b,
                    const float* tanA, const float* tanB, float t,
                    float* res )
{
    float t2 = t * t;
    float t3 = t * t2;
    float m, rx, ry;

    m = 2.0f*t3 - 3.0f*t2 + 1.0f;
    rx = a[0] * m;
    ry = a[1] * m;

    m = -2.0f*t3 + 3.0f*t2;
    rx += b[0] * m;
    ry += b[1] * m;

    m = t3 - 2.0f*t2 + t;
    rx += tanA[0] * m;
    ry += tanA[1] * m;

    m = t3 - t2;
    rx += tanB[0] * m;
    ry += tanB[1] * m;

    res[0] = rx;
    res[1] = ry;
}


void calc_linear2( const float* a, const float* b, float t, float* res )
{
#define CL2_INTERP(A,B) (A + (B - A) * t)
    res[0] = CL2_INTERP(a[0], b[0]);
    res[1] = CL2_INTERP(a[1], b[1]);
}
#endif


#define ANIM_BEGIN(AT) \
    it = (Anim*) list->buf[AT].ptr.v; \
    end = it + list->buf[AT].used

#define ANIM_ID(AT,PTR)     ID_MAKE(AT, PTR - (Anim*) list->buf[AT].ptr.v)

#define ANIM_RESULT_CELL(it) \
    ur_buffer(it->out.cell.bufN)->ptr.cell + it->out.cell.i

#define ANIM_RESULT_PF(it) \
    ur_buffer(it->out.cell.bufN)->ptr.f + it->out.cell.i


extern UStatus boron_doVoid( UThread* ut, const UCell* blkC );

/*
  Update all AFT_HERMITE animations in list.
  Return number of animations running in runCount.
*/
UStatus anim_processHermite( UThread* ut, AnimList* list, double dt,
                             uint32_t* runCount )
{
    UCell blkC;
    UCell* resC;
    float* resF;
    const float* v1;
    const float* v2;
    Anim* it;
    Anim* end;
    const float* cp;
    float pos;
    uint32_t count = 0;

    // Curve calculation loop.
    ANIM_BEGIN( AFT_HERMITE );
    for( ; it != end; ++it )
    {
        if( it->behavior >= ANIM_UNSET )
            continue;
        ++count;

        cp  = it->curve;
        pos = it->curvePos;
        it->fraction = calc_hermiteY( cp+4, cp+6, cp, cp+2, pos );
        pos += it->timeScale * dt;
        if( pos >= 1.0f )
        {
            // Animation complete.
            if( it->behavior == ANIM_LOOP )
                it->curvePos = pos - 1.0f;
            else    // ANIM_ONCE
                it->behavior = ANIM_COMPLETE;
        }
        else
        {
            it->curvePos = pos;
        }
    }

    // Store output and invoke update & finish handlers.
    it = (Anim*) list->buf[ AFT_HERMITE ].ptr.v;
    for( ; it != end; ++it )
    {
        if( it->behavior >= ANIM_UNSET )
            continue;

#define ANIM_INTERP(A,B)   A + (B - A) * it->fraction

        switch( it->outType )
        {
            case ANIM_OUT_VECTOR_3:
            {
                // Quick version of ur_seriesIterM().
                const UBuffer* buf = ur_buffer( it->out.cell.bufN );
                resF = buf->ptr.f + it->out.cell.i;

                assert( buf->type == UT_VECTOR );
                assert( buf->form == UR_VEC_F32 );
            }
                goto store_3f;

            case ANIM_OUT_MATRIX_ROT:
            {
                float quat[4];
                resF = ANIM_RESULT_PF(it);
                quat_slerp( it->v1, it->v2, it->fraction, quat );
                //printf( "KR quat %f %f %f %f\n",
                //        quat[0], quat[1], quat[2], quat[3] );
                quat_toMatrix( quat, quat[3], resF, 1 );
            }
                break;

            case ANIM_OUT_MATRIX_TRANS:
                resF = ANIM_RESULT_PF(it) + 12;
                goto store_3f;

            case ANIM_OUT_CELL_VEC3:
                resC = ANIM_RESULT_CELL( it );
                ur_setId(resC, UT_VEC3);
                resF = resC->vec3.xyz;
store_3f:
                v2 = it->v2;
                if( it->behavior == ANIM_COMPLETE )
                {
                    *resF++ = v2[0];
                    *resF++ = v2[1];
                    *resF   = v2[2];
                }
                else
                {
                    v1 = it->v1;
                    *resF++ = ANIM_INTERP( v1[0], v2[0] );
                    *resF++ = ANIM_INTERP( v1[1], v2[1] );
                    *resF   = ANIM_INTERP( v1[2], v2[2] );
                }
                break;

            case ANIM_OUT_CELL_DOUBLE:
                resC = ANIM_RESULT_CELL( it );
                ur_setId(resC, UT_DOUBLE);
                if( it->behavior == ANIM_COMPLETE )
                    ur_double(resC) = it->v2[0];
                else
                    ur_double(resC) = ANIM_INTERP( it->v1[0], it->v2[0] );
                break;
        }

        if( it->updateBlkN != UR_INVALID_BUF )
        {
            ur_initSeries( &blkC, UT_BLOCK, it->updateBlkN );
            if( boron_doVoid( ut, &blkC ) == UR_THROW )
                return UR_THROW;
        }

        if( it->behavior == ANIM_COMPLETE )
        {
            if( it->finishBlkN != UR_INVALID_BUF )
            {
                ur_initSeries( &blkC, UT_BLOCK, it->finishBlkN );
                if( boron_doVoid( ut, &blkC ) == UR_THROW )
                    return UR_THROW;
            }
            if( it->allocState == ANIM_SINGLE_USE )
                anim_free( list, ANIM_ID(AFT_HERMITE, it) );
            else
                it->behavior = ANIM_ONCE | ANIM_DISABLED;
        }
    }

    *runCount = count;
    return UR_OK;
}


static void calc_linearKey3( const float* tvalues, float t,
                             uint16_t* keyTable, float* res )
{
    if( t <= 0.0f )
    {
        tvalues += 1;
    }
    else if( t >= 1.0f )
    {
        tvalues += keyTable[9] + 1;
    }
    else
    {
        const float* v1;
        float t1, t2;
        float f;
        int i = (int) (t * 10.0f);

        tvalues += keyTable[i];
        v1 = tvalues - 4;
        t2 = *tvalues++;
        t1 = *v1++;

#define CL_INTERP(A,B) (A + (B - A) * f)
        f = (t - t1) / (t2 - t1);
        *res++ = CL_INTERP( v1[0], tvalues[0] );
        *res++ = CL_INTERP( v1[1], tvalues[1] );
        *res   = CL_INTERP( v1[2], tvalues[2] );
        return;
    }
    memcpy( res, tvalues, sizeof(float)*3 );
}


/*
  Update all AFT_LINEAR_KEY animations in list.
  Return number of animations running in runCount.
*/
UStatus anim_processLinearKey( UThread* ut, AnimList* list, double dt,
                               uint32_t* runCount )
{
    UCell blkC;
    UCell* resC;
    float* resF;
    Anim* it;
    Anim* end;
    float pos;
    uint32_t count = 0;

    // Curve calculation loop.
    ANIM_BEGIN( AFT_LINEAR_KEY );
    for( ; it != end; ++it )
    {
        if( it->behavior >= ANIM_UNSET )
            continue;
        ++count;

        pos = it->curvePos;
        calc_linearKey3( it->curve, pos, KEY_TABLE(it), KEY_RESULT(it) );
        pos += it->timeScale * dt;
        if( pos >= 1.0f )
        {
            // Animation complete.
            if( it->behavior == ANIM_LOOP )
                it->curvePos = pos - 1.0f;
            else    // ANIM_ONCE
                it->behavior = ANIM_COMPLETE;
        }
        else
        {
            it->curvePos = pos;
        }
    }

    // Store output and invoke update & finish handlers.
    it = (Anim*) list->buf[ AFT_LINEAR_KEY ].ptr.v;
    for( ; it != end; ++it )
    {
        if( it->behavior >= ANIM_UNSET )
            continue;

#define ANIM_INTERP(A,B)   A + (B - A) * it->fraction

        switch( it->outType )
        {
            case ANIM_OUT_VECTOR_3:
            {
                // Quick version of ur_seriesIterM().
                const UBuffer* buf = ur_buffer( it->out.cell.bufN );
                resF = buf->ptr.f + it->out.cell.i;

                assert( buf->type == UT_VECTOR );
                assert( buf->form == UR_VEC_F32 );
            }
                goto store_3f;
/*
            case ANIM_OUT_MATRIX_ROT:
            {
                float quat[4];
                resF = ANIM_RESULT_PF(it);
                quat_slerp( it->v1, it->v2, it->fraction, quat );
                //printf( "KR quat %f %f %f %f\n",
                //        quat[0], quat[1], quat[2], quat[3] );
                quat_toMatrix( quat, quat[3], resF, 1 );
            }
                break;
*/
            case ANIM_OUT_MATRIX_TRANS:
                resF = ANIM_RESULT_PF(it) + 12;
                goto store_3f;

            case ANIM_OUT_CELL_VEC3:
                resC = ANIM_RESULT_CELL( it );
                ur_setId(resC, UT_VEC3);
                resF = resC->vec3.xyz;
store_3f:
                memcpy( resF, it->v2, sizeof(float)*3 );
                break;

            case ANIM_OUT_CELL_DOUBLE:
                resC = ANIM_RESULT_CELL( it );
                ur_setId(resC, UT_DOUBLE);
                ur_double(resC) = it->v2[0];
                break;
        }

        if( it->updateBlkN != UR_INVALID_BUF )
        {
            ur_initSeries( &blkC, UT_BLOCK, it->updateBlkN );
            if( boron_doVoid( ut, &blkC ) == UR_THROW )
                return UR_THROW;
        }

        if( it->behavior == ANIM_COMPLETE )
        {
            if( it->finishBlkN != UR_INVALID_BUF )
            {
                ur_initSeries( &blkC, UT_BLOCK, it->finishBlkN );
                if( boron_doVoid( ut, &blkC ) == UR_THROW )
                    return UR_THROW;
            }
            if( it->allocState == ANIM_SINGLE_USE )
                anim_free( list, ANIM_ID(AFT_LINEAR_KEY, it) );
            else
                it->behavior = ANIM_ONCE | ANIM_DISABLED;
        }
    }

    *runCount = count;
    return UR_OK;
}


/*
anim-id: animate dest-matrix [
    2.0 ; seconds
    ; target could move so update each cycle?  (-> static, >> moving)
    vec3 deck-top >> hand-target 12
        x-curve y-curve z-curve
        ; #[1.2 0.0  0.5 0.5  0.0 0.0  1.0 1.0]
        ; #[1.2 0.0  0.5 0.5  0.0 0.0  1.0 1.0]
        ; #[1.2 0.0  0.5 0.5  0.0 0.0  1.0 1.0]
    quat_to_mat deck-rot-quat -> hand-target-rot-quat 0
        ease-in
]
*/


// Two animation banks for interface (0) and simulation (1).
#define ANIM_BANK_UI    0
#define ANIM_BANK_SIM   1

AnimList _animUI;
AnimList _animSim;


void anim_system( int boot )
{
    if( boot )
    {
        anim_initList( &_animUI );
        anim_initList( &_animSim );
    }
    else
    {
        anim_cleanupList( &_animUI );
        anim_cleanupList( &_animSim );
    }
}


static void anim_recycleList( UThread* ut, AnimList* list )
{
    Anim* it;
    Anim* end;
    int ft;

    for( ft = 0; ft < AFT_COUNT; ++ft )
    {
        ANIM_BEGIN( ft );
        for( ; it != end; ++it )
        {
            if( it->behavior == ANIM_UNSET )
                continue;
            ur_markBlkN( ut, it->updateBlkN );
            ur_markBlkN( ut, it->finishBlkN );
        }
    }
}


void anim_recycle( UThread* ut )
{
    anim_recycleList( ut, &_animUI );
    //anim_recycleList( ut, &_animSim );
}


typedef struct
{
    UBlockParser bp;
    Anim* anim;
}
AnimSpecParser;


/*
  scripts/parse_blk_compile.b -p Anim -e gl/anim.c

    some [
        double!/vec3!/word! '-> double!/vec3!/word! (init_target)
      | '-> double!/vec3!/word! (target)
      | 'as word!       (as)
      | double!         (duration)
      | 'update block!  (update)
      | 'finish block!  (finish)
      | 'frames vector! (frames)
      | word!           (word)
      | vector!         (curve)
    ]
*/

enum AnimRulesReport
{
    ANIM_REP_INIT_TARGET,
    ANIM_REP_TARGET,
    ANIM_REP_AS,
    ANIM_REP_DURATION,
    ANIM_REP_UPDATE,
    ANIM_REP_FINISH,
    ANIM_REP_FRAMES,
    ANIM_REP_WORD,
    ANIM_REP_CURVE
};

/*
enum AnimRulesAtomIndex
{
    ANIM_AI_->,
    ANIM_AI_AS,
    ANIM_AI_UPDATE,
    ANIM_AI_FINISH,
    ANIM_AI_FRAMES,
};
*/

static const uint8_t _animParseRules[] =
{
    // 0
    0x10, 0x03, 0x00,
    // 3
    0x04, 0x08, 0x09, 0x45, 0x06, 0x00, 0x09, 0x45, 0x03, 0x00, 0x04, 0x06, 0x06, 0x00, 0x09, 0x45, 0x03, 0x01, 0x04, 0x06, 0x06, 0x01, 0x08, 0x0D, 0x03, 0x02, 0x04, 0x04, 0x08, 0x06, 0x03, 0x03, 0x04, 0x06, 0x06, 0x02, 0x08, 0x17, 0x03, 0x04, 0x04, 0x06, 0x06, 0x03, 0x08, 0x17, 0x03, 0x05, 0x04, 0x06, 0x06, 0x04, 0x08, 0x16, 0x03, 0x06, 0x04, 0x04, 0x08, 0x0D, 0x03, 0x07, 0x08, 0x16, 0x03, 0x08,
    // 69 - Typesets
    0x40, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};


static const UAtom _animAtoms[5] =
{
    UR_ATOM_SYMBOL_R_ARROW,     // ->
    UR_ATOM_AS,
    UR_ATOM_UPDATE,
    UR_ATOM_FINISH,
    UR_ATOM_FRAMES,
  //UR_ATOM_SYMBOL_R_SHIFT,     // >>
};


static const float vzero[4] = { 0.0, 0.0, 0.0, 0.0 };

static void _animSetValue3( UThread* ut, const UCell* cell, float* f3 )
{
    if( ur_is(cell, UT_WORD) )
        cell = ur_wordCell(ut, cell);

    switch( ur_type(cell) )
    {
        case UT_DOUBLE:
            f3[0] = ur_double(cell);
            f3[1] = f3[2] = 0.0f;
            break;

        case UT_VEC3:
            memcpy( f3, cell->vec3.xyz, sizeof(float)*3 );
            break;

        case UT_VECTOR:
        {
            USeriesIter si;
            ur_seriesSlice( ut, &si, cell );
            assert( si.buf->form == UR_VEC_F32 );
            memcpy( f3, si.buf->ptr.f + si.it, sizeof(float)*3 );
        }
            break;

        default:
            memcpy( f3, vzero, sizeof(float)*3 );
            break;
    }
}


static const float* _animVector( UThread* ut, const UCell* cell )
{
    USeriesIter si;

    if( ur_is(cell, UT_WORD) )
        cell = ur_wordCell(ut, cell);
    if( ! ur_is(cell, UT_VECTOR) )
        return NULL;
    ur_seriesSlice( ut, &si, cell );
    assert( si.buf->form == UR_VEC_F32 );
    return si.buf->ptr.f + si.it;
}


static void _setCurve( UThread* ut, const UCell* cell, Anim* anim )
{
    USeriesIter si;
    ur_seriesSlice( ut, &si, cell );
    anim->curve    = si.buf->ptr.f + si.it;
    anim->curveLen = si.end - si.it;
}


static void _buildKeyTable( Anim* anim )
{
    uint16_t* tab = KEY_TABLE(anim);
    uint16_t* end = tab + 10;
    const float* key = anim->curve;
    float t;
    int i = 0;
    int keySize = anim_keySize[ anim->outType ];

    for( t = 0.0f; tab != end; t += 0.1f )
    {
        while( t >= key[i] )
            i += keySize;
        assert( i < anim->curveLen );
        //printf("KR keyTable %ld %d (%f)\n", tab - KEY_TABLE(anim), i, key[i]);
        *tab++ = i;
    }
}


static void _animRuleHandler( UBlockParser* par, int rule,
                              const UCell* it, const UCell* end )
{
    Anim* anim = ((AnimSpecParser*) par)->anim;
    UThread* ut = par->ut;
    (void) end;

    switch( rule )
    {
        case ANIM_REP_INIT_TARGET:
            AR_REPORT( "ANIM_REP_INIT_TARGET\n" );
            _animSetValue3(ut, it, anim->v1);
            _animSetValue3(ut, it+2, anim->v2);
            anim->behavior &= ~ANIM_DISABLED;
            break;

        case ANIM_REP_TARGET:
            AR_REPORT( "ANIM_REP_TARGET %d\n", anim->outType );
            // Animate from the current value of the result to a target.
            ++it;
            switch( anim->outType )
            {
                case ANIM_OUT_VECTOR_3:
                    memcpy( anim->v1, ANIM_RESULT_PF(anim), sizeof(float)*3 );
                    goto restart_vec3;

                case ANIM_OUT_MATRIX_ROT:
                    quat_fromMatrix( anim->v1, ANIM_RESULT_PF(anim) );
                    {
                    const float* mat = _animVector( ut, it );
                    if( mat )
                        quat_fromMatrix( anim->v2, mat );
                    else
                    {
                        anim->v2[0] = 1.0f;
                        memcpy( anim->v2 + 1, vzero, sizeof(float)*3 );
                    }
                    }
                    break;

                case ANIM_OUT_MATRIX_TRANS:
                    memcpy( anim->v1, ANIM_RESULT_PF(anim) + 12,
                            sizeof(float)*3 );
                    {
                    const float* mat = _animVector( ut, it );
                    memcpy( anim->v2, mat ? mat + 12 : vzero, sizeof(float)*3 );
                    }
                    break;

                case ANIM_OUT_CELL_VEC3:
                    _animSetValue3(ut, ANIM_RESULT_CELL(anim), anim->v1);
restart_vec3:
                    _animSetValue3(ut, it, anim->v2);
                    break;

                case ANIM_OUT_CELL_DOUBLE:
                {
                    const UCell* cell = ANIM_RESULT_CELL(anim);
                    anim->v1[0] = ur_double(cell);

                    if( ur_is(it, UT_WORD) )
                        it = ur_wordCell(ut, it);
                    anim->v2[0] = ur_double(it);
                }
                    break;
            }
            anim->behavior &= ~ANIM_DISABLED;
            break;

        case ANIM_REP_AS:
            if( ur_atom(it+1) == UR_ATOM_TRANS )
                anim->outType = ANIM_OUT_MATRIX_TRANS;
            else //if( ur_atom(it+1) == UR_ATOM_ROTATION )
                anim->outType = ANIM_OUT_MATRIX_ROT;
            break;

        case ANIM_REP_DURATION:
            AR_REPORT( "ANIM_REP_DURATION %f\n", ur_double(it) );
duration:
            anim->timeScale = (float) (1.0 / ur_double(it));
            break;

        case ANIM_REP_UPDATE:
            AR_REPORT( "ANIM_REP_UPDATE\n" );
            anim->updateBlkN = it[1].series.buf;
            break;

        case ANIM_REP_FINISH:
            AR_REPORT( "ANIM_REP_FINISH\n" );
            anim->finishBlkN = it[1].series.buf;
            break;

        case ANIM_REP_FRAMES:
            AR_REPORT( "ANIM_REP_FRAMES\n" );
            _setCurve( ut, it+1, anim );
            _buildKeyTable( anim );
            break;

        case ANIM_REP_WORD:
            AR_REPORT( "ANIM_REP_WORD %s\n", ur_atomCStr(ut, ur_atom(it)) );
            switch( ur_atom(it) )
            {
                case UR_ATOM_LOOP:
                    anim->behavior = ANIM_LOOP|(anim->behavior & ANIM_DISABLED);
                    break;
                case UR_ATOM_SINGLE_USE:
                    if( anim->allocState == ANIM_USED )
                        anim->allocState = ANIM_SINGLE_USE;
                    break;
                default:
                    it = ur_wordCell(ut, it);
                    if( ur_is(it, UT_VECTOR) )
                        goto curve;
                    if( ur_is(it, UT_DOUBLE) )
                        goto duration;
                    break;
            }
            break;

        case ANIM_REP_CURVE:
            AR_REPORT( "ANIM_REP_CURVE\n" );
curve:
            _setCurve( ut, it, anim );
            break;
    }
}


typedef struct
{
    UIndex  buf;
    UIndex  bufIndex;
    uint8_t outType;
    uint8_t funcType;
    uint8_t bank;
}
AnimInit;


static void animInit_init( AnimInit* init, UIndex buf, UIndex bufIndex,
                           int outType )
{
    init->buf      = buf;
    init->bufIndex = bufIndex;
    init->outType  = outType;
    init->funcType = AFT_HERMITE;
    init->bank     = ANIM_BANK_UI;
}


static float _hermiteLinear[8] = { 0.5, 0.5, 0.1, 0.1, 0.0, 0.0, 1.0, 1.0 };

static void anim_init( Anim* anim, UIndex buf, UIndex bufIndex, int outType )
{
    anim->out.cell.bufN = buf;
    anim->out.cell.i    = bufIndex;
    anim->updateBlkN    = UR_INVALID_BUF;
    anim->finishBlkN    = UR_INVALID_BUF;
    anim->curve         = _hermiteLinear;
    anim->curvePos      = 0.0f;
    anim->timeScale     = 1.0f;
    anim->curveLen      = 8;
    anim->behavior      = ANIM_ONCE | ANIM_DISABLED;
    anim->repeatCount   = 0;
    anim->outType       = outType;
    anim->allocState    = ANIM_USED;
}


static UStatus anim_parseSpec( UThread* ut, AnimInit* init, const UCell* blkC,
                               uint32_t* idP )
{
    AnimSpecParser sp;
    const UCell* it;

    sp.bp.ut = ut;
    sp.bp.atoms  = _animAtoms;
    sp.bp.rules  = _animParseRules;
    sp.bp.report = _animRuleHandler;
    sp.bp.rflag  = 0;
    ur_blockIt( ut, (UBlockIt*) &sp.bp.it, blkC );

    // Pre-parse pass to determine anim_alloc ftype.
    for( it = sp.bp.it; it != sp.bp.end; ++it )
    {
        if( ur_is(it, UT_WORD) && ur_atom(it) == UR_ATOM_FRAMES )
        {
            init->funcType = AFT_LINEAR_KEY;
            break;
        }
    }

    sp.anim = anim_alloc( init->bank ? &_animSim : &_animUI,
                          init->funcType, idP );
    anim_init( sp.anim, init->buf, init->bufIndex, init->outType );

    if( ! ur_parseBlockI( &sp.bp, sp.bp.rules, sp.bp.it ) )
        return ur_error( ut, UR_ERR_SCRIPT, "Invalid animatef spec" );
    return UR_OK;
}


/*-cf-
    animatef
        value int!/double!/word!/vector!  Value to animate or bank time delta.
        spec        Animation specification or bank id (int!).
    return: int! Animation id (for new animation) or count (for update).

    Starts a new animation if value is a word!, restarts an existing animation
    if it's an int!, or update a bank of running animations if value is a
    double!.

    There are two animation banks, 0 and 1, for real-time (e.g. GUI) updates
    and simulation updates respectively.
*/
CFUNC_PUB( cfunc_animatef )
{
    UCell* cell;
    const UCell* a2 = a1+1;
    uint32_t id;

    if( ur_is(a1, UT_INT) )
    {
        Anim* anim;

        id = ur_int(a1);
        // TODO: Handle both animation banks.
        anim = anim_pointer( &_animUI, id );
        if( anim->allocState == ANIM_FREE )
            return ur_error( ut, UR_ERR_SCRIPT,
                             "Animation 0x%X has been freed", id );

        switch( ur_type(a2) )
        {
            /*
            case UT_NONE:
                anim_free( &_animUI, id );
                ur_setId(res, UT_UNSET);
                return UR_OK;
            */
            case UT_LOGIC:
                if( ur_logic(a2) )
                    anim->behavior &= ~ANIM_DISABLED;
                else
                    anim->behavior |= ANIM_DISABLED;
                break;

            case UT_DOUBLE:
                if( ID_FTYPE(id) == AFT_HERMITE )
                {
                    if( anim->outType == ANIM_OUT_CELL_DOUBLE )
                    {
                        cell = ANIM_RESULT_CELL(anim);
                        anim->v1[0] = ur_double(cell);
                        anim->v2[0] = ur_double(a2);
                        goto restart_anim;
                    }
                }
                else
                {
                    goto restart_anim;
                }
                break;

            case UT_VEC3:
                if( anim->outType == ANIM_OUT_VECTOR_3 )
                {
                    memcpy( anim->v1, ANIM_RESULT_PF(anim), sizeof(float)*3 );
                    goto restart_vec3;
                }
                else if( anim->outType == ANIM_OUT_CELL_VEC3 )
                {
                    cell = ANIM_RESULT_CELL(anim);
                    memcpy( anim->v1, cell->vec3.xyz, sizeof(float)*3 );
restart_vec3:
                    memcpy( anim->v2, a2->vec3.xyz, sizeof(float)*3 );
restart_anim:
                    anim->curvePos = 0.0f;
                    anim->behavior &= ~ANIM_DISABLED;
                }
                break;

            //case UT_BLOCk:
            //    break;

            default:
                goto bad_arg2;
        }
    }
    else if( ur_is(a1, UT_WORD) )
    {
        int type;

        if( ! ur_is(a2, UT_BLOCK) )
            goto bad_arg2;

        cell = ur_wordCellM(ut, a1);
        if( ! cell )
            return UR_THROW;

        type = ur_type(cell);
        if( type == UT_VEC3 || type == UT_DOUBLE )
        {
            AnimInit init;
            animInit_init( &init, a1->word.ctx, a1->word.index,
                           (type == UT_VEC3) ? ANIM_OUT_CELL_VEC3
                                             : ANIM_OUT_CELL_DOUBLE );
            if( ! anim_parseSpec( ut, &init, a2, &id ) )
                return UR_THROW;
        }
        else
            return boron_badArg( ut, type, 1 );
    }
    else if( ur_is(a1, UT_VECTOR) )
    {
        if( ur_is(a2, UT_BLOCK) )
        {
            AnimInit init;
            animInit_init( &init, a1->series.buf, a1->series.it,
                           ANIM_OUT_VECTOR_3 );
            if( ! anim_parseSpec( ut, &init, a2, &id ) )
                return UR_THROW;
        }
        else
            goto bad_arg2;
    }
    else // if( ur_is(a1, UT_DOUBLE) )
    {
        AnimList* list = ur_int(a2) ? &_animSim : &_animUI;
        uint32_t kcount;

        if( ! anim_processHermite( ut, list, ur_double(a1), &id ) )
            return UR_THROW;
        if( ! anim_processLinearKey( ut, list, ur_double(a1), &kcount ) )
            return UR_THROW;
        id += kcount;
    }
    ur_setId(res, UT_INT);
    ur_int(res) = id;
    return UR_OK;

bad_arg2:
    return boron_badArg( ut, ur_type(a2), 2 );
}
