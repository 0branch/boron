/* URL encoder template */


#ifdef URLENC_COMMON
#ifdef URLENC_FUNC_ENCODE
static char urlenc_hex[] = "0123456789ABCDEF";

/* "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_.~" */
static unsigned char urlenc_bitset[ 16 ] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0xFF, 0x03,
    0xFE, 0xFF, 0xFF, 0x87, 0xFE, 0xFF, 0xFF, 0x47
};
#endif

#ifdef URLENC_FUNC_DECODE
/*
  Convert hex character to integer value.
*/
static int urlenc_fromHex( int ch )
{
    if( ch >= '0' && ch <= '9' )
        return ch - '0';
    if( ch >= 'a' && ch <= 'f' )
        return ch - ('a' - 10);
    if( ch >= 'A' && ch <= 'F' )
        return ch - ('A' - 10);
    return 0;
}
#endif

#undef URLENC_COMMON
#endif


#ifdef URLENC_FUNC_ENCODE
/*
  Dest must have room for ((end - it) * 3) elements.
*/
URLENC_T* URLENC_FUNC_ENCODE( const URLENC_T* it, const URLENC_T* end,
                              URLENC_T* dest )
{
    URLENC_T ch;
    while( it != end )
    {
        ch = *it++;
        if( ch < (8 * sizeof(urlenc_bitset)) &&
            (urlenc_bitset[ ch>>3 ] & 1<<(ch & 7)) )
            *dest++ = ch;
        else if( ch == ' ' )
            *dest++ = '+';
        else 
        {
            *dest++ = '%';
            *dest++ = urlenc_hex[ (ch >> 4) & 15 ];
            *dest++ = urlenc_hex[ ch & 15 ];
        }
    }
    return dest;
}
#undef URLENC_FUNC_ENCODE
#endif


#ifdef URLENC_FUNC_DECODE
/*
  Dest must have room for (end - it) elements.
*/
URLENC_T* URLENC_FUNC_DECODE( const URLENC_T* it, const URLENC_T* end,
                              URLENC_T* dest )
{
    URLENC_T ch;
    while( it != end )
    {
        ch = *it++;
        if( ch == '%' )
        {
            if( it == end )
            {
                *dest++ = '%';
                break;
            }
            ch = urlenc_fromHex( *it++ );
            if( it == end )
            {
                *dest++ = '%';
                *dest++ = it[ -1 ];
                break;
            }
            *dest++ = (ch << 4) | urlenc_fromHex( *it++ );
        }
        else if( ch == '+' )
            *dest++ = ' ';
        else
            *dest++ = ch;
    }
    return dest;
}
#undef URLENC_FUNC_DECODE
#endif


#undef URLENC_T


/* EOF */
