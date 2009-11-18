#ifndef MEM_UTIL_H
#define MEM_UTIL_H


#ifdef __cplusplus
extern "C" {
#endif

const uint8_t* find_uint8_t( const uint8_t* it, const uint8_t* end, int val );
const uint16_t* find_uint16_t( const uint16_t* it, const uint16_t* end,
                               int val );
const uint32_t* find_uint32_t( const uint32_t* it, const uint32_t* end,
                               int val );

const uint8_t* find_last_uint8_t( const uint8_t* it, const uint8_t* end,
                                  int val );
const uint16_t* find_last_uint16_t( const uint16_t* it, const uint16_t* end,
                                    int val );

const uint8_t* find_charset_uint8_t( const uint8_t* it, const uint8_t* end,
                                     const uint8_t* cset, int csetLen );
const uint16_t* find_charset_uint16_t( const uint16_t* it, const uint16_t* end,
                                       const uint8_t* cset, int csetLen );

const uint8_t* find_pattern_uint8_t( const uint8_t* it, const uint8_t* end,
                               const uint8_t* pit, const uint8_t* pend );
const uint16_t* find_pattern_uint16_t( const uint16_t* it, const uint16_t* end,
                               const uint16_t* pit, const uint16_t* pend );

const uint8_t* match_pattern_uint8_t( const uint8_t* it, const uint8_t* end,
                                const uint8_t* pit, const uint8_t* pend );
const uint16_t* match_pattern_uint16_t( const uint16_t* it, const uint16_t* end,
                                const uint16_t* pit, const uint16_t* pend );

void reverse_char( char* it, char* end );
void reverse_uint16_t( uint16_t* it, uint16_t* end );
void reverse_uint32_t( uint32_t* it, uint32_t* end );

int compare_uint8_t( const uint8_t* it,  const uint8_t* end,
                     const uint8_t* itB, const uint8_t* endB );
int compare_uint16_t( const uint16_t* it,  const uint16_t* end,
                      const uint16_t* itB, const uint16_t* endB );

#ifdef __cplusplus
}
#endif


#endif
