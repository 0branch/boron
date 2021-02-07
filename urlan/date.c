/*
  Copyright 2007-2010 Karl Robillard

  This file is part of the Urlan datatype system.

  Urlan is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Urlan is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Urlan.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifdef __linux
//#define _BSD_SOURCE     // Needed for timegm (Fedora 11)
#endif

#include <time.h>
#include "urlan.h"
#include "os.h"


extern double str_toDouble( const uint8_t* it, const uint8_t* end,
                            const uint8_t** pos );


static char* append02d( char* cp, int n )
{
    *cp++ = '0' + (n / 10);
    *cp++ = '0' + (n % 10);
    return cp;
}


void date_toString( UThread* ut, const UCell* cell, UBuffer* str, int depth )
{
    char tmp[ 30 ];
    struct tm st;
    time_t tt;
    long timeZone;
    int hour;
    char* cp = tmp;
    double sec = ur_double(cell);
    //double frac;

    (void) ut;
    (void) depth;

    tt = (time_t) sec;
#if defined(_WIN32)
#if _MSC_VER >= 1400
    localtime_s( &st, &tt );
#else
    st = *localtime( &tt );
#endif
#else
    localtime_r( &tt, &st );
    //gmtime_r( &tt, &st );
#endif

#if defined(_WIN32)
#if _MSC_VER >= 1400
    _get_timezone( &timeZone );
    timeZone = -timeZone;
#else
    timeZone = -timezone;
#endif
#elif defined(__sun__)
    {
        struct tm gt;
        int days, hours;

        gmtime_r( &tt, &gt );
        days = st.tm_yday - gt.tm_yday;
        hours = (days < -1 ? 24 : days > 1 ? -24 : days * 24) +
                 st.tm_hour - gt.tm_hour;
        timeZone = (hours * 60 + st.tm_min - gt.tm_min) * 60;
    }
#else
    timeZone = st.tm_gmtoff;
#endif
    //printf( "KR gmtoff %ld %ld\n", st.tm_gmtoff, timezone );

    {
        int year = st.tm_year + 1900;
        cp = append02d( cp, year / 100 );
        cp = append02d( cp, year % 100 );
    }
    *cp++ = '-';
    cp = append02d( cp, st.tm_mon + 1 );
    *cp++ = '-';
    cp = append02d( cp, st.tm_mday );

    if( st.tm_hour || st.tm_min || st.tm_sec )
    {
        *cp++ = 'T';
        cp = append02d( cp, st.tm_hour );
        *cp++ = ':';
        cp = append02d( cp, st.tm_min );
        if( st.tm_sec )
        {
            *cp++ = ':';
            cp = append02d( cp, st.tm_sec );

            /*
            if( st.tm_sec < 10 )
                *cp++ = '0';
            frac = sec - floor(sec);
            if( frac )
                appendDecimal( out, frac );
            */
        }
    }

    if( timeZone )
    {
        if( timeZone < 0 )
        {
            *cp++ = '-';
            timeZone = -timeZone;
        }
        else
        {
            *cp++ = '+';
        }
        hour = timeZone / 3600;
        cp = append02d( cp, hour );
        *cp++ = ':';
        cp = append02d( cp, (timeZone / 60) - (hour * 60) );
    }
    else
    {
        *cp++ = 'Z';
    }
    *cp = '\0';

    ur_strAppendCStr( str, tmp );
}


static int twoInt( const uint8_t* cp )
{
    int a, b;
    a = cp[0] - '0';
    if( a < 0 || a > 9 )
        return -1;
    b = cp[1] - '0';
    if( b < 0 || b > 9 )
        return -1;
    return (a * 10) + b;
}


/*
  Read ISO 8601 date and time notation (extended format only).
*/
double ur_stringToDate( const uint8_t* start, const uint8_t* end,
                        const uint8_t** pos )
{
    // yyyy-mm-ddThh:mm:ss.fff[+/-hh:mm]
    // _123456789_123456789_123456789

    struct tm tmp;
    const uint8_t* it;
    double sec = 0.0;
    int utc = 0;
    int offset;
    int len = end - start;

    if( (len < 10) || (start[4] != '-') || (start[7] != '-') )
    {
        if( pos )
            *pos = start;
        return 0.0;
    }

    // Handle date portion.

    tmp.tm_year  = (twoInt( start ) * 100 + twoInt( start + 2 )) - 1900;
    tmp.tm_mon   = twoInt( start + 5 ) - 1;
    tmp.tm_mday  = twoInt( start + 8 );
    tmp.tm_hour  = 0;
    tmp.tm_min   = 0;
    tmp.tm_sec   = 0;
    tmp.tm_isdst = -1;


    // Handle time portion.  Both 'T' and '/' are accepted as separator.

    it = start + 10;

    if( (len < 13) || (*it != 'T' && *it != '/') )
        goto done;
    ++it;
    len = end - it;

    tmp.tm_hour = twoInt( it );
    it += 2;
    if( (len < 5) || (*it != ':') )
        goto done;

    tmp.tm_min = twoInt( ++it );
    it += 2;
    if( it == end )
        goto done;
    if( *it == ':' )
    {
        ++it;
        sec = str_toDouble( it, end, &it );
        if( it == end )
            goto done;
    }


    // Handle UTC offset.

    switch( *it )
    {
        case '+':
            utc = 1;
            break;
        case '-':
            utc = -1;
            break;
        case 'Z':
            utc = 1;
            ++it;
        default:
            goto done;
    }
    ++it;
    len = end - it;
    if( len < 2 )
        goto done;
    offset = twoInt( it );
    tmp.tm_hour += (utc < 0) ? offset : -offset;
    it += 2;
    if( (len < 5) || (*it != ':') )
        goto done;
    ++it;
    offset = twoInt( it );
    tmp.tm_min += (utc < 0) ? offset : -offset;
    it += 2;

done:

    if( pos )
        *pos = it;
#ifdef _WIN32
#if _MSC_VER >= 1400
    return sec + (double) (utc ? _mkgmtime( &tmp ) : mktime( &tmp ));
#else
    return sec + (double) mktime( &tmp );   // TODO: Handle utc.
#endif
#elif defined(__sun__)
    return sec + (double) mktime( &tmp );   // TODO: Handle utc.
#else
    return sec + (double) (utc ? timegm( &tmp ) : mktime( &tmp ));
#endif
}


/*EOF*/
