/*
  Boron OpenGL GUI
  Copyright 2012,2014 Karl Robillard

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


#include <glv_keys.h>
#include "boron-gl.h"
#include "draw_prog.h"
#include "os.h"
#include "gl_atoms.h"

GWidgetClass wclass_scrollbar;


enum SliderState
{
    BTN_STATE_UP,
    BTN_STATE_DOWN
};

enum SliderOrient
{
    HORIZONTAL,
    VERTICAL
};

struct Value
{
    float val, min, max;
    float vsize;            // Scroll-bar only.
};

typedef struct
{
    GWidget  wid;
    struct Value data;
    UIndex   actionN;
    int16_t  knobLen;
    uint16_t td;
    int16_t  dragOff;
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
    struct Value* da = &ep->data;
    float n;

    if( ur_is(cell, UT_INT) )
        n = (float) ur_int(cell);
    else if( ur_is(cell, UT_DOUBLE) )
        n = (float) ur_double(cell);
    else
        n = 0.0;

    if( n < da->min )
        n = da->min;
    else if( n > da->max )
        n = da->max;
    da->val = n;
}


/*-wid-
    slider  [orient]    range           initial-value   [action]
            word!       coord!/vec3!    int!/double!    block!

    Range is minimum,maximum.
*/
static const uint8_t slider_args[] =
{
    GUIA_OPT,     UT_WORD,
    GUIA_ARGM, 2, UT_COORD, UT_VEC3,
    GUIA_ARGW, 2, UT_INT,   UT_DOUBLE,
    GUIA_OPT,     UT_BLOCK,
    GUIA_END
};

static GWidget* slider_make( UThread* ut, UBlockIter* bi,
                             const GWidgetClass* wclass )
{
    GSlider* ep;
    const UCell* arg[4];

    if( ! gui_parseArgs( ut, bi, wclass, slider_args, arg ) )
        return 0;

    ep = (GSlider*) gui_allocWidget( sizeof(GSlider), wclass );
    ep->state = BTN_STATE_UP;

    // Optional orientaion.
    if( arg[0] )
    {
        const char* word = ur_atomCStr( ut, arg[0]->word.atom );
        ep->orient = (word[0] == 'h') ? HORIZONTAL : VERTICAL;
    }
    else if( wclass == &wclass_scrollbar )
    {
        ep->orient = VERTICAL;
    }

    if( ur_is(arg[1], UT_COORD) )
    {
        ep->data.val =
        ep->data.min = (float) arg[1]->coord.n[0];
        ep->data.max = (float) arg[1]->coord.n[1];
        ep->data.vsize = (float) arg[1]->coord.n[2];
        ep->dataType = UT_INT;
    }
    else
    {
        ep->data.val =
        ep->data.min = arg[1]->vec3.xyz[0];
        ep->data.max = arg[1]->vec3.xyz[1];
        ep->data.vsize = arg[1]->vec3.xyz[2];
        ep->dataType = UT_DOUBLE;
    }

    slider_setValue( ep, arg[2] );

    // Optional action block.
    if( arg[3] )
        ep->actionN = arg[3]->series.buf;

    return (GWidget*) ep;
}


static void slider_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;
    ur_markBlkN( ut, ep->actionN );
}


static uint16_t slider_knobTrans( GSlider* ep, float movef, float range )
{
    struct Value* da = &ep->data;
    float relv = da->val - da->min;

    if( ep->dataType == UT_INT )
        relv = floor(relv + 0.5);
    return (uint16_t) (movef * relv / range);
}


static int slider_place( GSlider* ep, int px )
{
    int nx, moveW;

    if( ep->orient == HORIZONTAL )
    {
        nx = px - ep->wid.area.x;
        moveW = ep->wid.area.w - ep->knobLen;
    }
    else
    {
#ifdef YTOP
        nx = px - ep->wid.area.y;
#else
        nx = ep->wid.area.y + ep->wid.area.h - px;
#endif
        moveW = ep->wid.area.h - ep->knobLen;
    }

    if( nx < 0 )
        nx = 0;
    else if( nx > moveW )
        nx = moveW;
    if( ep->td != nx )
    {
        struct Value* da = &ep->data;
        float range = da->max - da->min;
        float movef = (float) moveW;

        da->val = da->min + (nx * range / movef);
        ep->td = slider_knobTrans( ep, movef, range );
        //printf( "KR slider adj nx:%d td:%d\n", nx, ep->td );
        return 1;
    }
    return 0;
}


static int slider_adjust( GSlider* ep, float adj )
{
    struct Value* da = &ep->data;
    float n = da->val + adj;

    if( n < da->min )
        n = da->min;
    else if( n > da->max )
        n = da->max;
    if( da->val != n )
    {
        int majorD = (ep->orient == HORIZONTAL) ? ep->wid.area.w
                                                : ep->wid.area.h;
        da->val = n;
        ep->td = slider_knobTrans( ep, (float) (majorD - ep->knobLen),
                                   ep->data.max - ep->data.min );
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
                int pd, rd;

                gui_grabMouse( wp, 1 );

                if( ep->orient == HORIZONTAL )
                {
                    pd = ev->x;
                    rd = ev->x - wp->area.x;
                }
                else
                {
                    pd = ev->y;
#ifdef YTOP
                    rd = ev->y - wp->area.y;
#else
                    rd = wp->area.y + wp->area.h - ev->y;
#endif
                }

                // Do relative movement if the pointer is over the knob.
                if( rd >= ep->td && rd < (ep->td + ep->knobLen) )
                {
                    ep->held = 2;
#ifndef YTOP
                    if( ep->orient == VERTICAL )
                        ep->dragOff = ev->y -(wp->area.y + wp->area.h - ep->td);
                    else
#endif
                        ep->dragOff = rd - ep->td;
                }
                else
                {
                    ep->held = 1;
                    if( slider_place( ep, pd ) )
                        goto trans;
                }
            }
            break;

        case GLV_EVENT_BUTTON_UP:
            if( ev->code == GLV_BUTTON_LEFT )
            {
                int pressed = (ep->state == BTN_STATE_DOWN);
                gui_ungrabMouse( wp );
                ep->held = 0;
                if( pressed && gui_widgetContains( wp, ev->x, ev->y ) )
                    goto activate;
            }
            break;

        case GLV_EVENT_MOTION:
            if( ep->held )
            {
                int pd = (ep->orient == HORIZONTAL) ? ev->x : ev->y;
                if( ep->held == 2 )
                    pd -= ep->dragOff;
                if( slider_place( ep, pd ) )
                    goto trans;
            }
            break;

        case GLV_EVENT_KEY_DOWN:
            if( ev->code == KEY_Left )
            {
                if( slider_adjust( ep, -1.0 ) )
                    goto trans;
            }
            else if( ev->code == KEY_Right )
            {
                if( slider_adjust( ep, 1.0 ) )
                    goto trans;
            }
            else if( ev->code == KEY_Home )
            {
                if( slider_adjust( ep, -9999999.0 ) )
                    goto trans;
            }
            else if( ev->code == KEY_End )
            {
                if( slider_adjust( ep, 9999999.0 ) )
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
        {
            float tx, ty;
            if( ep->orient == HORIZONTAL )
            {
                tx = (float) ep->td;
                ty = 0.0f;
            }
            else
            {
                tx = 0.0f;
#ifdef YTOP
                ty = (float) ep->td;
#else
                ty = (float) (wp->area.h - ep->knobLen - ep->td);
#endif
            }
            ur_setTransXY( ut, resN, ep->dpTrans, tx, ty );
        }
    }
activate:
    if( ep->actionN )
        gui_doBlockN( ut, ep->actionN );
}


static void slider_sizeHint( GWidget* wp, GSizeHint* size )
{
    UCell* rc;
    EX_PTR;
    int isSlider = (wp->wclass != &wclass_scrollbar);

    rc = glEnv.guiStyle + (isSlider ? CI_STYLE_SLIDER_SIZE
                                    : CI_STYLE_SCROLL_SIZE);
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

    if( ep->orient == HORIZONTAL )
    {
        size->minW = 100;
        size->maxW = GW_MAX_DIM;
        size->maxH = size->minH;
        size->weightX = 2;
        size->weightY = 1;
        size->policyX = GW_POL_EXPANDING;
        size->policyY = GW_POL_FIXED;
    }
    else
    {
        size->minH = 100;
        size->maxW = size->minW;
        size->maxH = GW_MAX_DIM;
        size->weightX = 1;
        size->weightY = 2;
        size->policyX = GW_POL_FIXED;
        size->policyY = GW_POL_EXPANDING;
    }
}


static void slider_layout( GWidget* wp )
{
    UCell* rc;
    UCell* area;
    UCell* style = glEnv.guiStyle;
    UThread* ut = glEnv.guiUT;
    EX_PTR;
    int isSlider = (wp->wclass != &wclass_scrollbar);
    int horiz = (ep->orient == HORIZONTAL);
    int majorD = horiz ? wp->area.w : wp->area.h;

    if( ! gDPC )
        return;

    // Set slider knob length.

    if( isSlider )
    {
        rc = style + CI_STYLE_SLIDER_SIZE;
        ep->knobLen = rc->coord.n[0];
    }
    else
    {
        struct Value* da = &ep->data;
        ep->knobLen = (int16_t) (da->vsize * majorD /
                                (da->max - da->min + da->vsize));
    }

    ep->td = slider_knobTrans( ep, (float) (majorD - ep->knobLen),
                               ep->data.max - ep->data.min );


    // Set draw list variables.

    area = style + CI_STYLE_AREA;
    gui_initRectCoord( area, wp, UR_ATOM_RECT );


    // Compile draw lists.

    rc = style + (isSlider ? CI_STYLE_SLIDER_GROOVE : CI_STYLE_SCROLL_BAR);
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );

    rc = style + (isSlider ? CI_STYLE_SLIDER : CI_STYLE_SCROLL_KNOB);
        //(horiz ? CI_STYLE_SLIDER_H : CI_STYLE_SLIDER_V);
    if( ur_is(rc, UT_BLOCK) )
    {
        float tx, ty;
        if( horiz )
        {
            tx = (float) ep->td;
            ty = 0.0f;
            area->coord.n[2] = ep->knobLen;   // Area width.
        }
        else
        {
            tx = 0.0f;
#ifdef YTOP
            ty = (float) ep->td;
#else
            ty = (float) (wp->area.h - ep->knobLen - ep->td);
#endif
            area->coord.n[3] = ep->knobLen;   // Area height.
        }

        ep->dpTrans = dp_beginTransXY( gDPC, tx, ty );
        ur_compileDP( ut, rc, 1 );
        dp_endTransXY( gDPC );
    }
}


static int slider_select( GWidget* wp, UAtom atom, UCell* res )
{
    EX_PTR;
    if( atom == UR_ATOM_VALUE )
    {
        ur_setId(res, ep->dataType);
        if( ep->dataType == UT_INT )
            ur_int(res) = (int32_t) ep->data.val;
        else
            ur_double(res) = ep->data.val;
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


GWidgetClass wclass_scrollbar =
{
    "scroll-bar",
    slider_make,        widget_free,        slider_mark,
    slider_dispatch,    slider_sizeHint,    slider_layout,
    widget_renderNul,   slider_select,
    0, 0
};


// EOF
