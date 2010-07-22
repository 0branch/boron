/*
  Copyright 2010 Karl Robillard

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


static int joy_open( UThread* ut, UBuffer* port, const UCell* from, int opt )
{
    const char* osDev = 0;
    int fd;
    (void) opt;

    if( ur_is(from, UT_STRING) )
        osDev = boron_cstr( ut, from, 0 );
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

        if( read(fd, &je, sizeof(struct js_event)) > 0 )
        {
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

        if( errno != EAGAIN )
        {
            //ur_error( UR_ERR_ACCESS, "" );
            perror( "read joystick" );
        }
    }

    if( events )
    {
        ur_setId(dest, UT_COORD);
        dest->coord.len = 3;
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


UPortDevice port_joystick =
{
    joy_open, joy_close, joy_read, joy_write, joy_seek
};


#endif
