/* Trim string template */
/*
  Copyright 2009 Karl Robillard

  This file is part of the Boron programming language.

  Boron is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Boron is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with Boron.  If not, see <http://www.gnu.org/licenses/>.
*/


/*
  Returns number of characters skipped from start.
*/
static int TRIM_FUNC_HEAD( TRIM_T* it, TRIM_T* end )
{
    TRIM_T* origIt = it;
    while( it != end )
    {
        if( *it > ' ' )
            break;
        ++it;
    }
    return it - origIt;
}


/*
  Returns number of characters removed.
*/
static int TRIM_FUNC_TAIL( TRIM_T* it, TRIM_T* end )
{
    TRIM_T* origEnd = end;
    while( end != it )
    {
        if( end[-1] > ' ' )
            break;
        --end;
    }
    return origEnd - end;
}


/*
  Returns number of characters removed.
*/
static int TRIM_FUNC_LINES( TRIM_T* it, TRIM_T* end )
{
    TRIM_T* out;
    int prev = 0;

    while( it != end )
    {
        if( (*it == ' ' || *it == '\t') && (*it == prev) )
            goto copy;
        else if( *it == '\n' )
            goto copy;
        prev = *it++;
    }
    return 0;

copy:

    out = it++;
    while( it != end )
    {
        if( (*it == ' ' || *it == '\t') && (*it == prev) )
            prev = *it++;
        else if( *it == '\n' )
            ++it;
        else
            *out++ = prev = *it++;
    }
    return it - out;
}


/*
  Returns number of characters removed.
*/
int TRIM_FUNC_INDENT( TRIM_T* it, TRIM_T* end )
{
    TRIM_T* cp = it;
    int margin = 0;

    // Set margin to number of spaces on first line with text.
    while( cp != end )
    {
        if( *cp > ' ' )
            break;
        if( *cp == '\n' )
            margin = 0;
        else
            ++margin;
        ++cp;
    }

    if( margin )
    {
        int removed;
        TRIM_T* out = it;

copy_line:

        while( (cp != end) )
        {
            if( *cp == '\n' )
            {
                *out++ = *cp++;
                break;
            }
            *out++ = *cp++;
        }

        // Skip margin
        removed = 0;
        while( cp != end )
        {
            if( *cp > ' ' )
                goto copy_line;

            if( *cp == '\n' )
            {
                *out++ = *cp++;
                removed = 0;
            }
            else
            {
                ++cp;
                ++removed;
                if( removed == margin )
                    goto copy_line;
            }
        }
        return end - out;
    }
    return 0;
}


#undef TRIM_FUNC_HEAD
#undef TRIM_FUNC_TAIL
#undef TRIM_FUNC_LINES
#undef TRIM_FUNC_INDENT
#undef TRIM_T


/* EOF */
