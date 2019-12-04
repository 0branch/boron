#ifndef I_PARSE_BLK_H
#define I_PARSE_BLK_H
/*
  Bytecode interpreter for parsing blocks
  Copyright 2016, 2019 Karl Robillard
*/


#include "urlan.h"


enum ParseBlockInstruction
{
    PB_End,
    PB_Flag,        // rflag bitmask (limit 8 bits)
    PB_Report,      // report id
    PB_ReportEnd,   // report id
    PB_Next,        // Number of bytes to skip to next alternate rule
    //PB_Repeat,
    //PB_RepeatV,
    PB_Skip,
    PB_LitWord,     // atoms index
    PB_Rule,        // rules offset to instructions
    PB_Type,        // Datatype
    PB_Typeset,     // rules offset of 64-bit mask
    PB_OptR,        // rules offset to instructions
    PB_OptT,        // Datatype
    PB_OptTs,       // rules offset of 64-bit mask
    PB_AnyR,        // rules offset to instructions
    PB_AnyT,        // Datatype
    PB_AnyTs,       // rules offset of 64-bit mask
    PB_SomeR,       // rules offset to instructions
    PB_SomeT,       // Datatype
    PB_SomeTs,      // rules offset of 64-bit mask
    PB_ToT,         // Datatype
    PB_ToTs,        // rules offset of 64-bit mask
    PB_ToLitWord,   // atoms index
    PB_ThruT,       // Datatype
    PB_ThruTs       // rules offset of 64-bit mask
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
