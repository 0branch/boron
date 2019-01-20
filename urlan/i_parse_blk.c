/*
  Bytecode interpreter for parsing blocks
  Copyright 2016, 2019 Karl Robillard
*/


#include "i_parse_blk.h"


static inline int bitIsSet64( const uint8_t* mem, uint32_t n )
{
    return (n > 63) ? 0 : mem[n>>3] & 1<<(n&7);
}

/*
  \param pc    Parse rule.
  \param it    Current input position.

  \returns non-zero if end of rule reached.
*/
int ur_parseBlockI( UBlockParser* par, const uint8_t* pc, const UCell* it )
{
    const uint8_t* bset;
    const uint8_t* next = 0;
    const UCell* start = it;
    const UCell* sit;
    int n, t;

    // TODO: Handle repeating of any operation (e.g. 4 skip, 2 to 3 of rule).

next_op:
    switch( *pc++ )
    {
        default:
        case PB_End:
            break;

        case PB_Flag:
            par->rflag |= *pc++;
            goto next_op;

        case PB_Report:
            par->report( par, *pc++, start, it );
            goto next_op;

        case PB_ReportEnd:
            par->report( par, *pc, start, it );
            break;

        case PB_Next:
            n = *pc++;
            next = pc + n;
            goto next_op;

        case PB_Skip:
            if( it == par->end )
                goto fail;
            ++it;
            goto next_op;

        case PB_LitWord:
            n = par->atoms[ *pc++ ];
            t = ur_type(it);
            if( ur_isWordType(t) && ur_atom(it) == n )
            {
                ++it;
                goto next_op;
            }
            goto fail;

        case PB_Rule:
            if( ! ur_parseBlockI( par, par->rules + *pc++, it ) )
                goto fail;
            it = par->it;
            goto next_op;

        case PB_Type:
            if( ur_type(it) != *pc++ )
                goto fail;
            ++it;
            goto next_op;

        case PB_Typeset:
            bset = par->rules + *pc++;
            if( ! bitIsSet64(bset, ur_type(it)) )
                goto fail;
            ++it;
            goto next_op;

        case PB_OptR:
            if( ur_parseBlockI( par, par->rules + *pc++, it ) )
                it = par->it;
            goto next_op;

        case PB_OptT:
            if( ur_type(it) == *pc++ )
                ++it;
            goto next_op;

        case PB_OptTs:
            bset = par->rules + *pc++;
            if( bitIsSet64(bset, ur_type(it)) )
                ++it;
            goto next_op;

        case PB_AnyR:
            bset = par->rules + *pc++;
            while( it != par->end && ur_parseBlockI( par, bset, it ) )
                it = par->it;
            goto next_op;

        case PB_AnyT:
            n = *pc++;
            while( it != par->end && ur_type(it) == n )
                ++it;
            goto next_op;

        case PB_AnyTs:
            bset = par->rules + *pc++;
            while( it != par->end && bitIsSet64(bset, ur_type(it)) )
                ++it;
            goto next_op;

        case PB_SomeR:
            sit = it;
            bset = par->rules + *pc++;
            while( it != par->end && ur_parseBlockI( par, bset, it ) )
                it = par->it;
            if( sit == it )
                goto fail;
            goto next_op;

        case PB_SomeT:
            sit = it;
            n = *pc++;
            while( it != par->end && ur_type(it) == n )
                ++it;
            if( sit == it )
                goto fail;
            goto next_op;

        case PB_SomeTs:
            sit = it;
            bset = par->rules + *pc++;
            while( it != par->end && bitIsSet64(bset, ur_type(it)) )
                ++it;
            if( sit == it )
                goto fail;
            goto next_op;

        case PB_ToT:
            n = *pc++;
            while( it != par->end )
            {
                if( ur_type(it) == n )
                    goto next_op;
                ++it;
            }
            goto fail;

        case PB_ToTs:
            bset = par->rules + *pc++;
            while( it != par->end )
            {
                if( bitIsSet64(bset, ur_type(it)) )
                    goto next_op;
                ++it;
            }
            goto fail;

        case PB_ToLitWord:
            n = par->atoms[ *pc++ ];
            while( it != par->end )
            {
                t = ur_type(it);
                if( ur_isWordType(t) && ur_atom(it) == n )
                    goto next_op;
                ++it;
            }
            goto fail;

        case PB_ThruT:
            n = *pc++;
            while( it != par->end )
            {
                if( ur_type(it) == n )
                {
                    ++it;
                    goto next_op;
                }
                ++it;
            }
            goto fail;

        case PB_ThruTs:
            bset = par->rules + *pc++;
            while( it != par->end )
            {
                if( bitIsSet64(bset, ur_type(it)) )
                {
                    ++it;
                    goto next_op;
                }
                ++it;
            }
            goto fail;
    }

    par->it = it;
    return 1;

fail:
    if( next )
    {
        pc = next;
        next = 0;
        it = start;
        goto next_op;
    }
    return 0;
}
