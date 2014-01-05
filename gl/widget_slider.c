/*
  Boron OpenGL GUI
  Copyright 2012 Karl Robillard

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


#include <GL/glv_keys.h>
#include "boron-gl.h"
#include "draw_prog.h"
#include "os.h"
#include "gl_atoms.h"


enum SliderState
{
    BTN_STATE_UP,
    BTN_STATE_DOWN
};

enum SliderOrient
{
    HSLIDER,
    VSLIDER
};

struct ValueI
{
    int32_t val, min, max;
};

struct ValueF
{
    float val, min, max;
};

typedef struct
{
    GWidget  wid;
    union
    {
        struct ValueF f;
        struct ValueI i;
    } data;
    UIndex   actionN;
    //DPSwitch dpSwitch;
    int16_t  knobWidth;
    int16_t  tx;
    DPSwitch dpTrans;
    uint8_t  state;
    uint8_t  held;
    uint8_t  orient;
    uint8_t  dataType;
}
GSlider;        

#define EX_PTR  GSlider* ep = (GSlider*) wp


static void slider_setValue( GSlider* ep, const UCell* cell )
{
    if( ep->dataType == UT_INT )
    {
        if( ur_is(cell, UT_INT) )
        {
            struct ValueI* da = &ep->data.i;
            int32_t n = ur_int(cell);
            if( n < da->min )
                n = da->min;
            else if( n > da->max )
                n = da->max;
            da->val = n;
        }
    }
    else
    {
        if( ur_is(cell, UT_DECIMAL) )
        {
            struct ValueF* da = &ep->data.f;
            float n = (float) ur_decimal(cell);
            if( n < da->min )
                n = da->min;
            else if( n > da->max )
                n = da->max;
            da->val = n;
        }
    }
}


/*-wid-
    slider  range           initial-value   [action]
            coord!/vec3!    int!/decimal!   block!

    Range is minimum,maximum.
*/
static const uint8_t slider_args[] =
{
    GUIA_ARGM, 2, UT_COORD, UT_VEC3,
    GUIA_ARGW, 2, UT_INT,   UT_DECIMAL,
    GUIA_OPT,     UT_BLOCK,
    GUIA_END
};

static GWidget* slider_make( UThread* ut, UBlockIter* bi,
                             const GWidgetClass* wclass )
{
    GSlider* ep;
    const UCell* arg[3];

    if( ! gui_parseArgs( ut, bi, wclass, slider_args, arg ) )
        return 0;

    ep = (GSlider*) gui_allocWidget( sizeof(GSlider), wclass );
    ep->state = BTN_STATE_UP;
    if( ur_is(arg[0], UT_COORD) )
    {
        ep->data.i.val =
        ep->data.i.min = arg[0]->coord.n[0];
        ep->data.i.max = arg[0]->coord.n[1];
        ep->dataType = UT_INT;
    }
    else
    {
        ep->data.f.val =
        ep->data.f.min = arg[0]->vec3.xyz[0];
        ep->data.f.max = arg[0]->vec3.xyz[1];
        ep->dataType = UT_DECIMAL;
    }

    slider_setValue( ep, arg[1] );

    // Optional action block.
    if( arg[2] )
        ep->actionN = arg[2]->series.buf;

    return (GWidget*) ep;
}


static void slider_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;
    //ur_markBuffer( ut, ep->labelN );
    ur_markBlkN( ut, ep->actionN );
}


/*
static void slider_setState( UThread* ut, GSlider* ep, int state )
{
    (void) ut;

    if( state != ep->state )
    {
        ep->state = state;

        if( ep->dpSwitch )
        {
            UIndex resN = gui_parentDrawProg( &ep->wid );
            if( resN != UR_INVALID_BUF )
                ur_setDPSwitch( ut, resN, ep->dpSwitch, state );
        }
    }
}
*/


static int slider_adjust( GSlider* ep, int adj )
{
    int nx = ep->tx + adj;
    int moveW = ep->wid.area.w - ep->knobWidth;

    if( nx < 0 )
        nx = 0;
    else if( nx > moveW )
        nx = moveW;
    if( ep->tx != nx )
    {
        ep->tx = nx;

        if( ep->dataType == UT_INT )
        {
            struct ValueI* da = &ep->data.i;
            da->val = da->min +
                      (ep->tx * (da->max - da->min) / moveW );
        }
        else
        {
            struct ValueF* da = &ep->data.f;
            da->val = da->min +
                      (ep->tx * (da->max - da->min) / moveW );
        }

        return 1;
    }
    return 0;
}


static void slider_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
    //printf( "KR button %d\n", ev->type );

    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            if( ev->code == GLV_BUTTON_LEFT )
            {
                //slider_setState( ut, ep, BTN_STATE_DOWN );
                ep->held = 1;
                gui_grabMouse( wp, 1 );
            }
            break;

        case GLV_EVENT_BUTTON_UP:
            if( ev->code == GLV_BUTTON_LEFT )
            {
                int pressed = (ep->state == BTN_STATE_DOWN);
                gui_ungrabMouse( wp );
                //slider_setState( ut, ep, BTN_STATE_UP );
                ep->held = 0;
                if( pressed && gui_widgetContains( wp, ev->x, ev->y ) )
                    goto activate;
            }
            break;

        case GLV_EVENT_MOTION:
            if( ep->held )
            {
                if( glEnv.mouseDeltaX )
                {
                    if( slider_adjust( ep, glEnv.mouseDeltaX ) )
                        goto trans;
                }
            }
            break;

        case GLV_EVENT_KEY_DOWN:
            if( ev->code == KEY_Left )
            {
                if( slider_adjust( ep, -2 ) )
                    goto trans;
            }
            else if( ev->code == KEY_Right )
            {
                if( slider_adjust( ep, 2 ) )
                    goto trans;
            }
            else if( ev->code == KEY_Home )
            {
                if( slider_adjust( ep, -9999 ) )
                    goto trans;
            }
            else if( ev->code == KEY_End )
            {
                if( slider_adjust( ep, 9999 ) )
                    goto trans;
            }
            // Fall through...

        case GLV_EVENT_KEY_UP:
            gui_ignoreEvent( ev );
            break;

        case GLV_EVENT_FOCUS_IN:
            break;

        case GLV_EVENT_FOCUS_OUT:
            //slider_setState( ut, ep, BTN_STATE_UP );
            break;
    }
    return;

trans:
    if( ep->dpTrans )
    {
        UIndex resN = gui_parentDrawProg( wp );
        if( resN != UR_INVALID_BUF )
            ur_setTransXY( ut, resN, ep->dpTrans, (float) ep->tx, 0.0f );
    }
activate:
    if( ep->actionN )
        gui_doBlockN( ut, ep->actionN );
}


static void slider_sizeHint( GWidget* wp, GSizeHint* size )
{
    UCell* rc;
    EX_PTR;

    rc = glEnv.guiStyle + CI_STYLE_SLIDER_SIZE;
    if( ur_is(rc, UT_COORD) )
    {
        size->minW = rc->coord.n[0];
        size->minH = rc->coord.n[1];
    }
    else
    {
        size->minW = 20;
        size->minH = 20;
    }

    if( ep->orient == HSLIDER )
    {
        size->minW = 100;
        size->maxW = GW_MAX_DIM;
        size->maxH = size->minH;
        size->weightX = 2;
        size->weightY = 1;
        size->policyX = GW_EXPANDING;
        size->policyY = GW_FIXED;
    }
    else
    {
        size->minH = 100;
        size->maxW = size->minW;
        size->maxH = GW_MAX_DIM;
        size->weightX = 1;
        size->weightY = 2;
        size->policyX = GW_FIXED;
        size->policyY = GW_EXPANDING;
    }
}


static void slider_layout( GWidget* wp )
{
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    UThread* ut = glEnv.guiUT;
    EX_PTR;
    int moveW;
    //int horiz = (ep->orient == HSLIDER);

    if( ! gDPC )
        return;

    // Get slider knob width.

    rc = style + CI_STYLE_SLIDER_SIZE;
    ep->knobWidth = rc->coord.n[0];

    moveW = ep->wid.area.w - ep->knobWidth;
    if( ep->dataType == UT_INT )
    {
        ep->tx = (moveW * (ep->data.i.val - ep->data.i.min)) /
                 (ep->data.i.max - ep->data.i.min);
    }
    else
    {
        ep->tx = (int16_t) ((moveW * (ep->data.f.val - ep->data.f.min)) /
                            (ep->data.f.max - ep->data.f.min));
    }


    // Set draw list variables.

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );


    // Compile draw lists.

    //ep->dpSwitch = dp_beginSwitch( gDPC, 2 );

    rc = style + CI_STYLE_SLIDER_GROOVE;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );
    //dp_endCase( gDPC, ep->dpSwitch );

#if 1
    rc = style + CI_STYLE_SLIDER;
        //(horiz ? CI_STYLE_SLIDER_H : CI_STYLE_SLIDER_V);
    if( ur_is(rc, UT_BLOCK) )
    {
        ep->dpTrans = dp_beginTransXY( gDPC, ep->tx, 0.0f );
        ur_compileDP( ut, rc, 1 );
        dp_endTransXY( gDPC );
    }
    //dp_endCase( gDPC, ep->dpSwitch );
#endif

    //dp_endSwitch( gDPC, ep->dpSwitch, ep->state );
}


static int slider_select( GWidget* wp, UAtom atom, UCell* res )
{
    EX_PTR;
    if( atom == UR_ATOM_VALUE )
    {
        ur_setId(res, ep->dataType);
        if( ep->dataType == UT_INT )
            ur_int(res) = ep->data.i.val;
        else
            ur_decimal(res) = ep->data.f.val;
        return UR_OK;
    }
    return gui_areaSelect( wp, atom, res );
}


GWidgetClass wclass_slider =
{
    "slider",
    slider_make,        widget_free,        slider_mark,
    slider_dispatch,    slider_sizeHint,    slider_layout,
    widget_renderNul,   slider_select,
    0, 0
};


// EOF
