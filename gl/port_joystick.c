/*
  Copyright 2010,2016 Karl Robillard

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


#ifdef __linux__

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/joystick.h>
#include "boron.h"


#define AXIS_COUNT  elemSize
#define BTN_COUNT   flags
#define FD          used

// NAME,
// STATE


static int joy_open( UThread* ut, const UPortDevice* pdev, const UCell* from,
                     int opt, UCell* res )
{
    UBuffer* port;
    const char* osDev = 0;
    char str[16];
    int fd;
    (void) opt;

    if( ur_is(from, UT_STRING) )
        osDev = boron_cstr( ut, from, 0 );
    else if( ur_is(from, UT_INT) )
    {
        strcpy( str, "/dev/input/js0" );
        str[13] = '0' + ur_int(from);
        osDev = str;
    }
    else
        osDev = "/dev/input/js0";

    fd = open( osDev, O_RDONLY | O_NONBLOCK );
    if( fd >= 0 )
    {
        char buttons, axes;
        //char name[80];

        ioctl( fd, JSIOCGAXES, &axes );
        ioctl( fd, JSIOCGBUTTONS, &buttons );
        //ioctl( fd, JSIOCGNAME(sizeof(name)), &name );

        port = boron_makePort( ut, pdev, 0, res );
        port->AXIS_COUNT = axes;
        port->BTN_COUNT = buttons;
        port->FD = fd;

        /*
        UIndex n;
        UBlock* blk;
        UCell* val;
        UCell* end;

        ++val;                          // JV_NAME
        ur_makeString( val, name, -1 );

        ++val;                          // JV_STATE
        n = ur_makeBlock( axes + buttons );
        ur_initBlock( val, n, 0 );
        blk = ur_blockPtr( n );
        blk->used = axes + buttons;

        val = blk->ptr.cells;
        end = val + blk->used;
        while( val != end )
        {
            ur_initInt(val, 0);
            ++val;
        }
        */
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_ACCESS, "Cannot open device %s", osDev );
}


static void joy_close( UBuffer* port )
{
    if( port->FD > -1 )
    {
        close( port->FD );
        port->FD = -1;
    }
}


static int joy_read( UThread* ut, UBuffer* port, UCell* dest, int part )
{
    struct js_event je;
    int16_t* cp;
    int events = 0;
    int fd = port->FD;
    (void) ut;
    (void) part;


    if( fd > -1 )
    {
        cp = dest->coord.n;

#define IGNORE_SYNTHETIC
#ifdef IGNORE_SYNTHETIC
read_again:
#endif
        errno = 0;

        if( read(fd, &je, sizeof(je)) > 0 )
        {
#ifdef IGNORE_SYNTHETIC
            if( je.type & JS_EVENT_INIT && errno != EAGAIN )
                goto read_again;
#endif

            if( je.type & JS_EVENT_BUTTON )
            {
                // je.value will be 0 or 1.

                *cp++ = 1;
                *cp++ = je.number;
                *cp++ = je.value;
                //ur_int(val + je.number + axes) = je.value;
            }
            else if( je.type & JS_EVENT_AXIS )
            {
                // je.value will be -32767 to +32767.

                *cp++ = 0;
                *cp++ = je.number;
                *cp++ = je.value;
                //ur_int(val + je.number) = je.value;
            }
            events = 1;
        }

        if( errno && errno != EAGAIN )
        {
            //ur_error( UR_ERR_ACCESS, "" );
            perror( "read joystick" );
        }
    }

    if( events )
    {
        ur_initCoord(dest, 3);
    }
    else
        ur_setId(dest, UT_NONE);
    return UR_OK;
}


static int joy_write( UThread* ut, UBuffer* port, const UCell* data )
{
    (void) port;
    (void) data;
    return ur_error( ut, UR_ERR_ACCESS, "Cannot write to joystick port" );
}


static int joy_seek( UThread* ut, UBuffer* port, UCell* pos, int where )
{
    (void) port;
    (void) pos;
    (void) where;
    return ur_error( ut, UR_ERR_SCRIPT, "Cannot seek on joystick port" );
}


static int joy_waitFD( UBuffer* port )
{
    return port->FD;
}


UPortDevice port_joystick =
{
    joy_open, joy_close, joy_read, joy_write, joy_seek, joy_waitFD, 0
};


//----------------------------------------------------------------------------


UStatus joy_makePort( UThread* ut, int number, UCell* res )
{
    UCell tmp;
    ur_setId(&tmp, UT_INT);
    ur_int(&tmp) = number;
    return joy_open( ut, &port_joystick, &tmp, 0, res );
}


#include "boron-gl.h"
#include "gl_atoms.h"

extern UStatus boron_doVoid( UThread* ut, const UCell* blkC );


#define MAX_AXIS    6
#define MAX_BUTTONS 16

typedef struct
{
    uint8_t axis[ MAX_AXIS ];
    uint8_t press[ MAX_BUTTONS ];
    uint8_t release[ MAX_BUTTONS ];
}
JoystickMap;


/*
  \param spec   Block! cell of specification or previously created map.
*/
UStatus joy_makeMap( UThread* ut, const UCell* spec, UCell* res )
{
    const uint8_t joyMapTypes[2] = { UT_BLOCK, UT_BINARY };
    UBlockIt bi;
    UBuffer* blk;
    const UCell* action;
    UCell* cell;
    JoystickMap* map;
    UIndex buf[2];
    UAtom atom;
    uint8_t* entry;
    int testCount;
    int n;


    ur_blockIt( ut, &bi, spec );
    testCount = bi.end - bi.it;

    // Check if spec is a previously constructed map.
    // Should probably have an identifier to verify it as a JoystickMap.
    if( testCount && ur_is(bi.it, UT_BINARY) )
    {
        *res = *spec;
        return UR_OK;
    }

    blk = ur_generate( ut, 2, buf, joyMapTypes );   // gc!

    {
    UBuffer* bin = ur_buffer( buf[1] );
    ur_binReserve( bin, sizeof(JoystickMap) );
    bin->used = sizeof(JoystickMap);
    map = (JoystickMap*) bin->ptr.b;
    memset( map, 0, sizeof(JoystickMap) );
    }

    if( testCount % 3 )
        goto parse_error;

    ur_arrReserve( blk, testCount + 1 );
    cell = ur_blkAppendNew( blk, UT_BINARY );
    ur_setSeries(cell, buf[1], 0);

    ur_foreach( bi )
    {
        if( ! ur_is(bi.it, UT_WORD) )
            goto parse_error;
        switch( atom = ur_atom(bi.it) )
        {
            case UR_ATOM_AXIS:
                entry = map->axis;
                break;
            case UR_ATOM_PRESS:
                entry = map->press;
                break;
            case UR_ATOM_RELEASE:
                entry = map->release;
                break;
            default:
                goto parse_error;
        }

        ++bi.it;
        if( ! ur_is(bi.it, UT_INT) )
            goto parse_error;
        n = ur_int(bi.it);
        if( n < 0 || n >= MAX_BUTTONS ||
            (atom == UR_ATOM_AXIS && n >= MAX_AXIS) )
            goto parse_error;
        entry += n;

        ++bi.it;
        switch( ur_type(bi.it) )
        {
            case UT_WORD:
                if( ! (action = ur_wordCell( ut, bi.it )) )
                    return UR_THROW;
                if( ur_is(action, UT_BLOCK) )
                    goto push_block;
                break;
            case UT_OPTION:             // /same
                if( blk->used < 1 )
                    goto parse_error;
                *entry = blk->used - 1;
                break;
            case UT_BLOCK:
                action = bi.it;
push_block:
                *entry = blk->used;
                ur_blkPush( blk, action );
                break;
            default:
                goto parse_error;
        }
    }

    ur_setId(res, UT_BLOCK);
    ur_setSeries(res, buf[0], 0 );
    return UR_OK;

parse_error:
    return ur_error( ut, UR_ERR_SCRIPT, "Invalid joystick-map rule" );
}


static UStatus _jsDispatch( UThread* ut, const UBuffer* blk, int fd )
{
    struct js_event je;
    const JoystickMap* map;
    const UCell* cell;
    const UCell* act;
    UCell* value;
    int index;

    cell = blk->ptr.cell;
    if( ! ur_is(cell, UT_BINARY) )
        return UR_OK;

    value = ur_ctxCell(ur_threadContext(ut), glEnv.guiValueIndex);
    map = (JoystickMap*) ur_buffer(cell->series.buf)->ptr.b;

    while( read(fd, &je, sizeof(je)) > 0 )
    {
#ifdef IGNORE_SYNTHETIC
        if( je.type & JS_EVENT_INIT )
            continue;
#endif
        index = 0;

        if( je.type & JS_EVENT_BUTTON )
        {
            if( je.number < MAX_BUTTONS )
            {
                // je.value will be 0 or 1.
                if( je.value )
                    index = map->press[ je.number ];
                else
                    index = map->release[ je.number ];

                ur_setId(value, UT_INT);
                ur_int(value) = je.value;
            }
        }
        else if( je.type & JS_EVENT_AXIS )
        {
            if( je.number < MAX_AXIS )
            {
                // je.value will be -32767 to +32767.
                index = map->axis[ je.number ];

                ur_setId(value, UT_DOUBLE);
                ur_double(value) = ((double) je.value) / 32767.0;
            }
        }

        if( index )
        {
            act = cell + index;
            if( ur_is(act, UT_BLOCK) )
            {
                if( boron_doVoid(ut, act) != UR_OK )
                    return UR_THROW;
            }
        }
    }

    if( errno != EAGAIN )
        return ur_error( ut, UR_ERR_INTERNAL, "joystick read %d", errno );

    return UR_OK;
}


UStatus joy_dispatch( UThread* ut, const UCell* portC, const UCell* mapC )
{
    const UBuffer* port = ur_buffer(portC->series.buf);
    return _jsDispatch( ut, ur_buffer(mapC->series.buf), port->FD );
}

#endif
