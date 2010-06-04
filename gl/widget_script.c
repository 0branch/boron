/*
  Boron OpenGL GUI
  Copyright 2008-2010 Karl Robillard

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
  A script widget is a widget which is entirely defined with a Boron context.
  It is meant for prototyping and quick demos.  Serious applications should
  probably be using a widget implemented in C.
*/


#include "boron-gl.h"
#include "gl_atoms.h"
#include "draw_prog.h"
#include "os.h"


enum ContextIndex
{
    CI_AREA,
    CI_EVENT,
    CI_KEY_DOWN,
    CI_KEY_UP,
    CI_MOUSE_MOVE,
    CI_MOUSE_DOWN,
    CI_MOUSE_UP,
    CI_MOUSE_WHEEL,
    CI_CLOSE,
    CI_RESIZE,
    CI_JOYSTICK,
    CI_DRAW,
    CI_COUNT
};


typedef struct
{
    GWidget wid;
    UIndex ctxN;
}
ScriptWidget;

#define EX_PTR  ScriptWidget* ep = (ScriptWidget*) wp


extern CFUNC_PUB(uc_key_code);

static void remapKeys( UThread* ut, const UCell* cell )
{
    if( ur_is(cell, UT_BLOCK) && (! ur_isShared(cell->series.buf)) )
    {
        UBlockIterM bi;
        ur_blkSliceM( ut, &bi, cell );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_BLOCK) )
                continue;
            uc_key_code( ut, bi.it, bi.it );
        }
    }
}


GWidgetClass wclass_script;
extern int boron_doBlock( UThread* ut, const UCell* ec, UCell* res );

static GWidget* swidget_make( UThread* ut, UBlockIter* bi )
{
    if( (bi->end - bi->it) > 1 )
    {
        ++bi->it;
        if( ur_is(bi->it, UT_BLOCK) )
        {
            UAtom atoms[ CI_COUNT ];
            UBuffer* blk;
            UBuffer* ctx;
            ScriptWidget* wp;
            int i;

            ur_internAtoms( ut,
                            "area event key-down key-up mouse-move mouse-down\n"
                            "mouse-up mouse-wheel close resize joystick\n"
                            "draw",
                            atoms );

            wp = (ScriptWidget*)
                    gui_allocWidget( sizeof(ScriptWidget), &wclass_script );
            wp->ctxN = ur_makeContext( ut, CI_COUNT );

            ctx = ur_buffer( wp->ctxN );
            for( i = 0; i < CI_COUNT; ++i )
                ur_ctxAddWordI( ctx, atoms[i] );
            ur_ctxSort( ctx );

            blk = ur_bufferSerM( bi->it );
            if( ! blk )
            {
cleanup:
                memFree( wp );
                return 0;
            }
            ur_bind( ut, blk, ctx, UR_BIND_THREAD );

            if( boron_doBlock( ut, bi->it, boron_result(ut) ) != UR_OK )
                goto cleanup;

            ctx = ur_buffer( wp->ctxN );    // Re-aquire
            remapKeys( ut, ur_ctxCell( ctx, CI_KEY_DOWN ) );
            remapKeys( ut, ur_ctxCell( ctx, CI_KEY_UP ) );

            ++bi->it;
            return (GWidget*) wp;
        }
    }
    ur_error( ut, UR_ERR_SCRIPT, "script-widget expected block" );
    return 0;
}


static void swidget_free( GWidget* wp )
{
    memFree( wp );
}


static void swidget_mark( UThread* ut, GWidget* wp )
{
    ur_markBlkN( ut, ((ScriptWidget*) wp)->ctxN );
}


static int mapButton( int code )
{
    switch( code )
    {
        case GLV_BUTTON_LEFT:   return 1;
        case GLV_BUTTON_RIGHT:  return 2;
        case GLV_BUTTON_MIDDLE: return 3;
    }
    return 0;
}


static const UCell* selectKeyHandler( UThread* ut, const UCell* cell, int code )
{
    if( ur_is(cell, UT_BLOCK) )
    {
        UBlockIter bi;

        ur_blkSlice( ut, &bi, cell );
        if( (bi.end - bi.it) & 1 )
            --bi.end;

        // TODO: Sort key-handler block so a binary search can be used here.
        while( bi.it != bi.end )
        {
            if( ur_int(bi.it) == code )
                return bi.it + 1;
            bi.it += 2;
        }
    }
    return 0;
}


static void swidget_dispatch( UThread* ut, GWidget* wp,
                              const GLViewEvent* event )
{
    EX_PTR;
    UCell* val;
    const UCell* cell;
    const UBuffer* ctx;

#define CCELL(N)     ur_ctxCell(ctx,N)

    ctx = ur_buffer( ep->ctxN );
    val = ur_ctxCell( ctx, CI_EVENT );

    switch( event->type )
    {
/*
        case GLV_EVENT_FOCUS_IN:
            ur_setId(val, UT_LOGIC);
            ur_logic(val) = 1;
            break;

        case GLV_EVENT_FOCUS_OUT:
            ur_setId(val, UT_LOGIC);
            ur_logic(val) = 0;
            break;
*/
        case GLV_EVENT_RESIZE:
            ur_setId(val, UT_COORD);
            val->coord.len  = 4;
            val->coord.n[0] = 0;
            val->coord.n[1] = 0;
            val->coord.n[2] = event->x;
            val->coord.n[3] = event->y;

            cell = CCELL( CI_RESIZE );
            break;

        case GLV_EVENT_CLOSE:
            ur_setId(val, UT_NONE);

            cell = CCELL( CI_CLOSE );
            break;

        case GLV_EVENT_BUTTON_DOWN:
            ur_setId(val, UT_INT);
            ur_int(val) = mapButton( event->code );

            cell = CCELL( CI_MOUSE_DOWN );
            break;

        case GLV_EVENT_BUTTON_UP:
            ur_setId(val, UT_INT);
            ur_int(val) = mapButton( event->code );
/*
            iv[ INPUT_CODE ].integer = mapButton( event->code );
            iv[ INPUT_MX ].integer   = event->x;
            iv[ INPUT_MY ].integer   = gView->height - event->y - 1;
*/
            cell = CCELL( CI_MOUSE_UP );
            break;

        case GLV_EVENT_MOTION:
            // coord elements: x, y, dx, dy
            ur_setId(val, UT_COORD);
            val->coord.len  = 5;
            val->coord.n[0] = event->x - wp->area.x;
            //val->coord.n[1] = (ui->rootH - event->y - 1) - wp->y;
            val->coord.n[1] = event->y - wp->area.y;
            val->coord.n[2] = glEnv.mouseDeltaX;
            val->coord.n[3] = glEnv.mouseDeltaY;
            val->coord.n[4] = event->state;

            cell = CCELL( CI_MOUSE_MOVE );
            break;

        case GLV_EVENT_WHEEL:
            ur_setId(val, UT_INT);
            ur_int(val) = event->y;

            cell = CCELL( CI_MOUSE_WHEEL );
            break;

        case GLV_EVENT_KEY_DOWN:
            cell = CCELL( CI_KEY_DOWN );
            goto key_handler;

        case GLV_EVENT_KEY_UP:
            cell = CCELL( CI_KEY_UP );
key_handler:
            ur_setId(val, UT_INT);
            ur_int(val) = event->code;

            cell = selectKeyHandler( ut, cell, event->code );
            if( ! cell )
                return;
            break;

        default:
            return;
    }

    if( ur_is(cell, UT_BLOCK) )
    {
        gui_doBlock( ut, cell );
    }
}


static void swidget_sizeHint( GWidget* wp, GSizeHint* size )
{
    (void) wp;

    size->minW    = 16;
    size->minH    = 16;
    size->maxW    = 0;
    size->maxH    = 0;
    size->weightX = 2;
    size->weightY = 2;
    size->policyX = GW_EXPANDING;
    size->policyY = GW_EXPANDING;
}


static void swidget_layout( UThread* ut, GWidget* wp )
{
    EX_PTR;
    UBuffer* ctx;
    UCell* cell;
    UCell* val;

    ctx = ur_buffer( ep->ctxN );
    cell = ur_ctxCell( ctx, CI_RESIZE );
    //if( ur_is(cell, UT_BLOCK) )
    if( ur_is(cell, UT_FUNC) )
    {
        // boron_call( ut, cell, blkC, boron_result(ut) );
        val = ur_ctxCell( ctx, CI_EVENT );

        ur_setId(val, UT_COORD);
        val->coord.len  = 4;
        val->coord.n[0] = wp->area.x;
        val->coord.n[1] = wp->area.y;
        val->coord.n[2] = wp->area.w;
        val->coord.n[3] = wp->area.h;

        gui_doBlock( ut, cell );
    }
}


static void swidget_render( GUI* ui, GWidget* wp )
{
    EX_PTR;
    UBuffer* ctx;
    UCell* cell;
    UThread* ut = ui->ut;

    ctx = ur_buffer( ep->ctxN );
    cell = ur_ctxCell( ctx, CI_DRAW );
    if( ur_is(cell, UT_DRAWPROG) )
    {
        DPState ds;
        ur_initDrawState( &ds );
        ur_runDrawProg( ut, &ds, cell->series.buf );
    }
}


static int swidget_select( GWidget* wp, UAtom atom, UCell* res )
{
    (void) wp;
    (void) atom;
    ur_setId(res, UT_NONE);
    return UR_OK;
}


GWidgetClass wclass_script =
{
    "script-widget",
    swidget_make,       swidget_free,       swidget_mark,
    swidget_dispatch,   swidget_sizeHint,   swidget_layout,
    swidget_render,     swidget_select,
    0, 0
};


// EOF
