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


#include "i_parse_blk.h"


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
    float    timeScale;     // or duration?
    float    fraction;      // Temporary result of curve calculation.
    float    v1[3];
    float    v2[3];
    uint16_t curveLen;
    uint16_t behavior;      // Loop, LoopBack, Ping, Pong, PingPong
    uint16_t repeatCount;
    uint16_t cellType;
}
Anim;                       // sizeof(Anim) = 68;


enum AnimBehavior
{
    ANIM_UNSET,
    ANIM_COMPLETE,
    ANIM_ONCE,
    ANIM_LOOP
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
    if( n == buf->used - 1 )
    {
        --buf->used;
    }
    else
    {
        anim = ur_ptr(Anim, buf) + n;
        anim->behavior = ANIM_UNSET;
        anim->updateBlkN = list->firstFree[ ftype ];
        list->firstFree[ ftype ] = n;
    }
}


/*
void anim_enable( AnimList* list, uint32_t id, int on )
{
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


/*
static void anim_finish( AnimList* list, int ftype, Anim* it, Anim* end,
                         uint32_t freeCount )
{
    do
    {
        if( it->behavior != ANIM_COMPLETE )
            continue;
        if( it->finishBlkN != UR_INVALID_BUF )
        {
            // Do finishBlkN.
        }
        anim_free( list, ANIM_ID(ftype, it) );
        if( --freeCount == 0 )
            break;
    }
    while( ++it != end );
}
*/


extern UStatus boron_doVoid( UThread* ut, const UCell* blkC );

/*
  Return number of animations running.
*/
uint32_t anim_process( UThread* ut, AnimList* list, double dt )
{
    UCell blkC;
    UCell* resC;
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
        if( it->behavior == ANIM_UNSET )
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
            {
                it->curvePos = pos - 1.0f;
            }
            else    // ANIM_ONCE
            {
                it->behavior = ANIM_COMPLETE;
            }
        }
        else
        {
            it->curvePos = pos;
        }
    }

    // Update and finish invocation loop.
    it = (Anim*) list->buf[ AFT_CELL ].ptr.v;
    for( ; it != end; ++it )
    {
        if( it->behavior == ANIM_UNSET )
            continue;

#define ANIM_INTERP(A,B)   A + (B - A) * it->fraction

        resC = ur_buffer( it->out.cell.bufN )->ptr.cell + it->out.cell.i;
        if( it->cellType == UT_VEC3 )
        {
            ur_setId(resC, UT_VEC3);
            v2 = it->v2;

            if( it->behavior == ANIM_COMPLETE )
            {
                V3(resC,0) = v2[0];
                V3(resC,1) = v2[1];
                V3(resC,2) = v2[2];
            }
            else
            {
                v1 = it->v1;
                V3(resC,0) = ANIM_INTERP( v1[0], v2[0] );
                V3(resC,1) = ANIM_INTERP( v1[1], v2[1] );
                V3(resC,2) = ANIM_INTERP( v1[2], v2[2] );
            }
        }
        else if( it->cellType == UT_VECTOR )
        {
            // Quick version of ur_seriesIterM().
            const UBuffer* buf = ur_buffer( resC->series.buf );
            float* resF = buf->ptr.f + resC->series.it;
            assert( buf->type == UT_VECTOR );
            assert( buf->form == UR_ATOM_F32 );

            if( it->behavior == ANIM_COMPLETE )
            {
                *resF++ = v2[0];
                *resF++ = v2[1];
                *resF   = v2[2];
            }
            else
            {
                *resF++ = ANIM_INTERP( v1[0], v2[0] );
                *resF++ = ANIM_INTERP( v1[1], v2[1] );
                *resF   = ANIM_INTERP( v1[2], v2[2] );
            }
        }
        else if( it->cellType == UT_DOUBLE )
        {
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
            anim_free( list, ANIM_ID(AFT_CELL, it) );
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


typedef struct
{
    UBlockParser bp;
    Anim* anim;
}
AnimSpecParser;


/*
  scripts/parse_blk_compile.b -p Anim -e gl/anim.c

    word! '-> word!     (io)
    any [
        double!         (duration)
      | 'update block!  (update)
      | 'finish block!  (finish)
      | word!/vector!   (curve)
    ]
*/

enum AnimRulesReport
{
    ANIM_REP_IO,
    ANIM_REP_DURATION,
    ANIM_REP_UPDATE,
    ANIM_REP_FINISH,
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
    0x08, 0x0D, 0x06, 0x00, 0x08, 0x0D, 0x02, 0x00, 0x0D, 0x0B, 0x00,
    // 11
    0x04, 0x04, 0x08, 0x06, 0x03, 0x01, 0x04, 0x06, 0x06, 0x01, 0x08, 0x17, 0x03, 0x02, 0x04, 0x06, 0x06, 0x02, 0x08, 0x17, 0x03, 0x03, 0x09, 0x25, 0x03, 0x04,
    // 37 - Typesets
    0x00, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00
};


static const UAtom _animAtoms[3] =
{
    UR_ATOM_SYMBOL_R_ARROW,     // ->
    UR_ATOM_UPDATE,
    UR_ATOM_FINISH,
  //UR_ATOM_SYMBOL_R_SHIFT,     // >>
  //UR_ATOM_ONCE,
};


#if 1
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
            break;

        case ANIM_REP_DURATION:
            AR_REPORT( "ANIM_REP_DURATION %f\n", ur_double(it) );
            anim->timeScale = (float) (1.0 / ur_double(it));
            break;

        case ANIM_REP_CURVE:
            AR_REPORT( "ANIM_REP_CURVE\n" );
            if( ur_is(it, UT_WORD) )
                it = ur_wordCell(ut, it);
            if( ur_is(it, UT_VECTOR) )
            {
                USeriesIter si;
                ur_seriesSlice( ut, &si, it );
                anim->curve = si.buf->ptr.f + si.it;
            }
            break;

        case ANIM_REP_UPDATE:
            AR_REPORT( "ANIM_REP_UPDATE\n" );
            anim->updateBlkN = it[1].series.buf;
            break;

        case ANIM_REP_FINISH:
            AR_REPORT( "ANIM_REP_FINISH\n" );
            anim->finishBlkN = it[1].series.buf;
            break;
    }
}


static void anim_init( Anim* anim, UCell* wordC, int dataType )
{
    anim->out.cell.bufN = wordC->word.ctx;
    anim->out.cell.i    = wordC->word.index;
    anim->updateBlkN    = UR_INVALID_BUF;
    anim->finishBlkN    = UR_INVALID_BUF;
    anim->curve         = NULL;
    anim->curvePos      = 0.0f;
    anim->timeScale     = 1.0f;
    anim->curveLen      = 0;
    anim->behavior      = ANIM_ONCE;
    anim->repeatCount   = 0;
    anim->cellType      = dataType;
}


/*-cf-
    animatef
        value   int!/double!/word!  Value to animate or bank time delta.
        spec    int!/block!         Animation data or bank id.
    return: int! Animation id (for new animation) or count (for update).

    Starts a new animation if value is a word!, restarts an existing animation
    if it's an int!, or update a bank of running animations if value is a
    double!.

    There are two animation banks, 0 and 1, for real-time (e.g. GUI) updates
    and simulation updates respectively.
*/
CFUNC( cfunc_animatef )
{
    uint32_t id;

    if( ur_is(a1, UT_INT) )
    {
        id = ur_int(a1);
    }
    else if( ur_is(a1, UT_WORD) )
    {
        AnimSpecParser sp;
        Anim* anim;
        int type;
        UCell* cell = ur_wordCellM(ut, a1);
        if( ! cell )
            return UR_THROW;

        type = ur_type(cell);
        if( type == UT_VEC3 || type == UT_DOUBLE )
        {
            anim = anim_alloc( &_animUI, AFT_CELL, &id );
            anim_init( anim, a1, type );
        }
        else
        {
            return ur_error( ut, UR_ERR_TYPE, "Invalid animatef value type %d",
                             ur_atomCStr(ut, type) );
        }

        sp.bp.ut = ut;
        sp.bp.atoms  = _animAtoms;
        sp.bp.rules  = _animParseRules;
        sp.bp.report = _animRuleHandler;
        sp.bp.rflag  = 0;
        ur_blockIt( ut, (UBlockIt*) &sp.bp.it, a1+1 );

        sp.anim = anim;

        if( ! ur_parseBlockI( &sp.bp, sp.bp.rules, sp.bp.it ) )
            return ur_error( ut, UR_ERR_SCRIPT, "Invalid animatef spec" );
    }
    else // if( ur_is(a1, UT_DOUBLE) )
    {
        id = anim_process( ut, ur_int(a1+1) ? &_animSim : &_animUI,
                           ur_double(a1) );
    }
    ur_setId(res, UT_INT);
    ur_int(res) = id;
    return UR_OK;
}
