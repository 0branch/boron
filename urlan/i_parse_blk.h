#ifndef I_PARSE_BLK_H
#define I_PARSE_BLK_H
/*
  Bytecode interpreter for parsing blocks
  Copyright 2016, 2019 Karl Robillard
*/


#include "urlan.h"


enum ParseBlockOperator
{
    PB_End,
    PB_Flag,
    PB_Report,
    PB_ReportEnd,
    PB_Next,
    //PB_Repeat,
    //PB_RepeatV,
    PB_Skip,
    PB_LitWord,
    PB_Rule,
    PB_Type,
    PB_Typeset,
    PB_OptR,
    PB_OptT,
    PB_OptTs,
    PB_AnyR,
    PB_AnyT,
    PB_AnyTs,
    PB_SomeR,
    PB_SomeT,
    PB_SomeTs,
    PB_ToT,
    PB_ToTs,
    PB_ToLitWord,
    PB_ThruT,
    PB_ThruTs
};


typedef struct UBlockParser UBlockParser;

struct UBlockParser
{
    UThread* ut;
    const UAtom* atoms;
    const uint8_t* rules;
    const UCell* it;
    const UCell* end;
    void (*report)(UBlockParser*, int, const UCell*, const UCell*);
    int rflag;
};


#ifdef __cplusplus
extern "C" {
#endif

int ur_parseBlockI( UBlockParser* par, const uint8_t* pc, const UCell* it );

#ifdef __cplusplus
}
#endif


#endif /* I_PARSE_BLK_H */
