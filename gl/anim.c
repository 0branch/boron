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
#include "urlan_atoms.h"
#include "gl_atoms.h"


extern void ur_markBlkN( UThread* ut, UIndex blkN );


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
    float    v1[3];         // Start value
    float    v2[3];         // End value
    uint16_t curveLen;
    uint16_t behavior;      // Loop, LoopBack, Ping, Pong, PingPong
    uint16_t repeatCount;
    uint8_t  outType;
    uint8_t  allocState;
}
Anim;                       // sizeof(Anim) = 72;


enum AnimBehavior
{
    ANIM_ONCE,
    ANIM_LOOP,
    ANIM_COMPLETE,          // Only used within anim_process().
    ANIM_UNSET,
    ANIM_DISABLED = 0x80
};


enum AnimOutput
{
    ANIM_OUT_VECTOR_1,      // out.cell.bufN is a vector!
    ANIM_OUT_VECTOR_3,      // out.cell.bufN is a vector!
    ANIM_OUT_CELL_DOUBLE,   // out.cell.bufN is a block!/context!
    ANIM_OUT_CELL_VEC3      // out.cell.bufN is a block!/context!
};


enum AnimAllocState
{
    ANIM_FREE,
    ANIM_SINGLE_USE,
    ANIM_USED
};


enum AnimFuncType
{
    AFT_CELL,
  //AFT_FLOAT,
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
  Return number of animations running.
*/
uint32_t anim_process( UThread* ut, AnimList* list, double dt )
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
    ANIM_BEGIN( AFT_CELL );
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
    it = (Anim*) list->buf[ AFT_CELL ].ptr.v;
    for( ; it != end; ++it )
    {
        if( it->behavior >= ANIM_UNSET )
            continue;

#define ANIM_INTERP(A,B)   A + (B - A) * it->fraction

        if( it->outType == ANIM_OUT_VECTOR_3 )
        {
            // Quick version of ur_seriesIterM().
            const UBuffer* buf = ur_buffer( it->out.cell.bufN );
            resF = buf->ptr.f + it->out.cell.i;

            assert( buf->type == UT_VECTOR );
            assert( buf->form == UR_ATOM_F32 );
            goto store_3f;
        }
        else if( it->outType == ANIM_OUT_CELL_VEC3 )
        {
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
        }
        else if( it->outType == ANIM_OUT_CELL_DOUBLE )
        {
            resC = ANIM_RESULT_CELL( it );
            ur_setId(resC, UT_DOUBLE);
            if( it->behavior == ANIM_COMPLETE )
                ur_double(resC) = it->v2[0];
            else
                ur_double(resC) = ANIM_INTERP( it->v1[0], it->v2[0] );
        }

        if( it->updateBlkN != UR_INVALID_BUF )
        {
            ur_initSeries( &blkC, UT_BLOCK, it->updateBlkN );
            if( boron_doVoid( ut, &blkC ) == UR_THROW )
                printf( "TODO: Handle animate update exception!\n" );
        }

        if( it->behavior == ANIM_COMPLETE )
        {
            if( it->finishBlkN != UR_INVALID_BUF )
            {
                ur_initSeries( &blkC, UT_BLOCK, it->finishBlkN );
                if( boron_doVoid( ut, &blkC ) == UR_THROW )
                    printf( "TODO: Handle animate finish exception!\n" );
            }
            if( it->allocState == ANIM_SINGLE_USE )
                anim_free( list, ANIM_ID(AFT_CELL, it) );
            else
                it->behavior = ANIM_ONCE | ANIM_DISABLED;
        }
    }

    return count;
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

animation: context [
    value: none
    curve: none
    scale: 1.0
    time:  0.0
    behavior: 1
]

ease-in: [
    0.00 0.0
    0.15 0.003375
    0.30 0.027000
    0.50 0.125000
    0.70 0.343000
    0.85 0.614125
    1.00 1.0
]
*/


// Two animation banks for interface (0) and simulation (1).
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
        word! '-> word! (io)
      | double!         (duration)
      | 'update block!  (update)
      | 'finish block!  (finish)
      | word!           (word)
      | vector!         (curve)
    ]
*/

enum AnimRulesReport
{
    ANIM_REP_IO,
    ANIM_REP_DURATION,
    ANIM_REP_UPDATE,
    ANIM_REP_FINISH,
    ANIM_REP_WORD,
    ANIM_REP_CURVE
};

/*
enum AnimRulesAtomIndex
{
    ANIM_AI_->,
    ANIM_AI_UPDATE,
    ANIM_AI_FINISH,
};
*/

static const uint8_t _animParseRules[] =
{
    // 0
    0x10, 0x03, 0x00,
    // 3
    0x04, 0x08, 0x08, 0x0D, 0x06, 0x00, 0x08, 0x0D, 0x03, 0x00, 0x04, 0x04, 0x08, 0x06, 0x03, 0x01, 0x04, 0x06, 0x06, 0x01, 0x08, 0x17, 0x03, 0x02, 0x04, 0x06, 0x06, 0x02, 0x08, 0x17, 0x03, 0x03, 0x04, 0x04, 0x08, 0x0D, 0x03, 0x04, 0x08, 0x16, 0x03, 0x05,
};


static const UAtom _animAtoms[3] =
{
    UR_ATOM_SYMBOL_R_ARROW,     // ->
    UR_ATOM_UPDATE,
    UR_ATOM_FINISH,
  //UR_ATOM_SYMBOL_R_SHIFT,     // >>
  //UR_ATOM_ONCE,
};


#if 0
#define AR_REPORT(...)  printf(__VA_ARGS__)
#else
#define AR_REPORT(...)
#endif

static void _animSetValue( UThread* ut, const UCell* cell, float* f3 )
{
    if( ur_is(cell, UT_WORD) )
        cell = ur_wordCell(ut, cell);
    if( ur_is(cell, UT_VEC3) )
        memcpy( f3, cell->vec3.xyz, sizeof(float)*3 );
    else if( ur_is(cell, UT_DOUBLE) )
    {
        f3[0] = ur_double(cell);
        f3[1] = f3[2] = 0.0f;
    }
    else
        f3[0] = f3[1] = f3[2] = 0.0f;
}

static void _animRuleHandler( UBlockParser* par, int rule,
                              const UCell* it, const UCell* end )
{
    Anim* anim = ((AnimSpecParser*) par)->anim;
    UThread* ut = par->ut;
    (void) end;

    switch( rule )
    {
        case ANIM_REP_IO:
            AR_REPORT( "ANIM_REP_IO\n" );
            _animSetValue(ut, it,   anim->v1);
            _animSetValue(ut, it+2, anim->v2);
            anim->behavior &= ~ANIM_DISABLED;
            break;

        case ANIM_REP_DURATION:
            AR_REPORT( "ANIM_REP_DURATION %f\n", ur_double(it) );
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
                    break;
            }
            break;

        case ANIM_REP_CURVE:
            AR_REPORT( "ANIM_REP_CURVE\n" );
curve:
            {
            USeriesIter si;
            ur_seriesSlice( ut, &si, it );
            anim->curve = si.buf->ptr.f + si.it;
            }
            break;
    }
}


static UStatus anim_parseSpec( UThread* ut, Anim* anim, const UCell* blkC )
{
    AnimSpecParser sp;

    sp.bp.ut = ut;
    sp.bp.atoms  = _animAtoms;
    sp.bp.rules  = _animParseRules;
    sp.bp.report = _animRuleHandler;
    sp.bp.rflag  = 0;
    ur_blockIt( ut, (UBlockIt*) &sp.bp.it, blkC );

    sp.anim = anim;

    if( ! ur_parseBlockI( &sp.bp, sp.bp.rules, sp.bp.it ) )
        return ur_error( ut, UR_ERR_SCRIPT, "Invalid animatef spec" );
    return UR_OK;
}


static void anim_init( Anim* anim, UIndex buf, UIndex bufIndex, int outType )
{
    anim->out.cell.bufN = buf;
    anim->out.cell.i    = bufIndex;
    anim->updateBlkN    = UR_INVALID_BUF;
    anim->finishBlkN    = UR_INVALID_BUF;
    anim->curve         = NULL;
    anim->curvePos      = 0.0f;
    anim->timeScale     = 1.0f;
    anim->curveLen      = 0;
    anim->behavior      = ANIM_ONCE | ANIM_DISABLED;
    anim->repeatCount   = 0;
    anim->outType       = outType;
    anim->allocState    = ANIM_USED;
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
                if( anim->outType == ANIM_OUT_CELL_DOUBLE )
                {
                    cell = ANIM_RESULT_CELL(anim);
                    anim->v1[0] = ur_double(cell);
                    anim->v2[0] = ur_double(a2);
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
        Anim* anim;
        int type;

        if( ! ur_is(a2, UT_BLOCK) )
            goto bad_arg2;

        cell = ur_wordCellM(ut, a1);
        if( ! cell )
            return UR_THROW;

        type = ur_type(cell);
        if( type == UT_VEC3 || type == UT_DOUBLE )
        {
            // TODO: Handle both animation banks.
            anim = anim_alloc( &_animUI, AFT_CELL, &id );
            anim_init( anim, a1->word.ctx, a1->word.index,
                       (type == UT_VEC3) ? ANIM_OUT_CELL_VEC3 :
                                           ANIM_OUT_CELL_DOUBLE );
            if( ! anim_parseSpec( ut, anim, a2 ) )
                return UR_THROW;
        }
        else
            return boron_badArg( ut, type, 1 );
    }
    else if( ur_is(a1, UT_VECTOR) )
    {
        Anim* anim;

        if( ! ur_is(a2, UT_BLOCK) )
            goto bad_arg2;

        // TODO: Handle both animation banks.
        anim = anim_alloc( &_animUI, AFT_CELL, &id );
        anim_init( anim, a1->series.buf, a1->series.it, ANIM_OUT_VECTOR_3 );
        if( ! anim_parseSpec( ut, anim, a2 ) )
            return UR_THROW;
    }
    else // if( ur_is(a1, UT_DOUBLE) )
    {
        id = anim_process( ut, ur_int(a2) ? &_animSim : &_animUI,
                           ur_double(a1) );
    }
    ur_setId(res, UT_INT);
    ur_int(res) = id;
    return UR_OK;

bad_arg2:
    return boron_badArg( ut, ur_type(a2), 2 );
}
