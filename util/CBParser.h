#ifndef CBPARSER_H
#define CBPARSER_H


typedef struct
{
    const UCell* values;

    // Private
    const UCell* _rules;
    const UCell* _rulesEnd;
    const UCell* _inputPos;
    const UCell* _inputEnd;
}
CBParser;


#ifdef __cplusplus
extern "C" {
#endif

extern void   cbp_beginParse( UThread*, CBParser*, const UCell*, const UCell*,
                              UIndex ruleBlkN );
extern UIndex cbp_beginParseStr( UThread*, CBParser*, const UBuffer* input,
                                 const char* rules, int rulesLen );
extern int    cbp_matchRule( CBParser* );

#ifdef __cplusplus
}
#endif


#endif /* CBPARSER_H */
