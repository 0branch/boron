#ifndef STR_H
#define STR_H

#ifdef __cplusplus
extern "C" {
#endif

extern char* str_copy( char* to, const char* from );
extern const char* str_skipWhite( const char* cp );
extern const char* str_toWhite( const char* cp );

#ifdef __cplusplus
}
#endif

#endif
