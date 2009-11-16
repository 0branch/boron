

/*
  Returns pointer in 'to' buffer at end of copy.
*/
char* str_copy( char* to, const char* from )
{
    while( *from )
        *to++ = *from++;
    return to;
}


/*
  Returns pointer in 'cp' buffer at end of whitespace.
*/
const char* str_skipWhite( const char* cp )
{
    int c;
    while( (c = *cp) )
    {
        if( c == ' ' || c == '\t' || c == '\n' || c == '\r' )
            ++cp;
        else
            break;
    }
    return cp;
}


/*
  Returns pointer in 'cp' buffer at start of whitespace.
*/
const char* str_toWhite( const char* cp )
{
    int c;
    while( (c = *cp) )
    {
        if( c == ' ' || c == '\t' || c == '\n' || c == '\r' )
            break;
        ++cp;
    }
    return cp;
}


/*EOF*/
