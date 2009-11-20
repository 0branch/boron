/*
  CBParser.c

  C Block Parser is a simple pattern matcher.
 
  Here is an example pattern rule set with three patterns for describing
  a cylinder:

    'radius int!/decimal! | 'length int!/decimal! | 'close-ends
*/


#include <assert.h>
#include "urlan.h"
#include "urlan_atoms.h"
#include "CBParser.h"


/**
  Initialize CBParser given a range of input cells and a block of rules.

  ruleSet is a block of patterns to match, separated by '|'.
  Note that the order of rules is important.  Earlier rules will match before
  later ones.
*/
void cbp_beginParse( CBParser* cbp, const UCell* it, const UCell* end,
                     const UBuffer* ruleSet )
{
    cbp->_inputPos = it;
    cbp->_inputEnd = end;
    cbp->_rules    = ruleSet;
}


/**
  Initialize CBParser given an input block and a set of rules in a C string.
  Returns index of rule block.
*/
UIndex cbp_beginParseStr( CBParser* cbp, UThread* ut, UBuffer* input,
                          const char* rules, int rulesLen )
{
    UCell tmp;
    UIndex ruleN = ur_tokenize( ut, rules, rules + rulesLen, &tmp );
    assert( ruleN );
    cbp_beginParse( cbp, input->ptr.cell, input->ptr.cell + input->used,
                    ur_buffer( ruleN ) );
    return ruleN;
}


/**
  Returns rule number of matched rule or -1 if no match was found.
  cbp->values points to the current position in the input block, or is set
  to zero when the end of input is reached.
*/
int cbp_matchRule( CBParser* cbp )
{
    const UBuffer* rules;
    const UCell* rit;
    const UCell* rend;
    const UCell* sit  = cbp->_inputPos;
    const UCell* send = cbp->_inputEnd;
    int ruleN = 0;

    if( sit == send )
    {
        cbp->values = 0;
        return -1;
    }

    cbp->values = sit;

    rules = cbp->_rules;
    if( ! rules || (rules->used < 1) )
        return -1;

    rit  = rules->ptr.cell;
    rend = rit + rules->used;

next:

    if( ur_is(rit, UT_LITWORD) )
    {
        if( ur_is(sit, UT_WORD) && (ur_atom(rit) == ur_atom(sit)) )
            goto value_matched;
    }
    else if( ur_is(rit, UT_WORD) )
    {
        UAtom atom = ur_atom(rit);
        if( atom < UT_BI_COUNT )
        {
            if( atom == ur_type(sit) )
                goto value_matched;
        }
        else if( atom == UR_ATOM_BAR )
            goto rule_matched;
    }
    else if( ur_is(rit, UT_DATATYPE) )
    {
        if( ur_isDatatype( sit, rit ) )
            goto value_matched;
    }

next_rule:

    sit = cbp->_inputPos;
    do
    {
        if( ur_is(rit, UT_WORD) && (ur_atom(rit) == UR_ATOM_BAR) )
        {
            ++ruleN;
            if( ++rit == rend )
                goto fail;
            goto next;
        }
        ++rit;
    }
    while( rit != rend );

fail:

    cbp->_inputPos = send;
    return -1;

value_matched:

    ++rit;
    ++sit;
    if( rit == rend )
    {
        if( sit == send )
            goto rule_matched;
        goto fail;
    }
    else if( sit == send )
    {
        if( ur_is(rit, UT_WORD) && (ur_atom(rit) == UR_ATOM_BAR) )
            goto rule_matched;
        goto next_rule;
    }
    goto next;

rule_matched:

    cbp->_inputPos = sit;
    return ruleN;
}


/*EOF*/
