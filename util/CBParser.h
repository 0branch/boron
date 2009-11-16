#ifndef CBPARSER_H
#define CBPARSER_H


typedef struct
{
    const UCell* values;

    // Private
    const UBuffer* _rules;
    const UCell* _inputPos;
    const UCell* _inputEnd;
}
CBParser;


#ifdef __cplusplus
extern "C" {
#endif

extern void   cbp_beginParse( CBParser*, const UCell*, const UCell*,
                              const UBuffer* ruleSet );
extern UIndex cbp_beginParseStr( CBParser*, UThread*, UBuffer* input,
                                 const char* rules, int rulesLen );
extern int    cbp_matchRule( CBParser* );

#ifdef __cplusplus
}
#endif


#endif /* CBPARSER_H */
