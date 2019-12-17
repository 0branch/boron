/*
  Boron OpenGL GUI
  Copyright 2019 Karl Robillard

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


typedef struct
{
    GWidget   wid;
    GWidget*  owner;
    UIndex    dataBlkN;
    UIndex    labelN;
    UIndex    bgDraw;
    UIndex    selDraw;
    uint16_t  selItem;
    uint16_t  selMax;
    uint16_t  selRendered;
    uint16_t  itemHeight;
    uint8_t   marginX;
    uint8_t   marginY;
}
GMenu;

#define EX_PTR  GMenu* ep = (GMenu*) wp
#define ITEM_UNSET  0xffff
#define DEFAULT_ITEM_HEIGHT  12


/*-wid-
    menu    items
            block!
*/
static const uint8_t menu_args[] =
{
    GUIA_ARG,   UT_BLOCK,
    GUIA_END
};

static GWidget* menu_make( UThread* ut, UBlockIter* bi,
                           const GWidgetClass* wclass )
{
    GMenu* ep;
    UBuffer* str;
    const UCell* arg[2];

    if( ! gui_parseArgs( ut, bi, wclass, menu_args, arg ) )
        return 0;

    ep = (GMenu*) gui_allocWidget( sizeof(GMenu), wclass );
    ep->dataBlkN = arg[0]->series.buf;
    ep->selItem  = arg[0]->series.it;
    ep->selRendered = ITEM_UNSET;

    str = ur_genBuffers( ut, 3, &ep->labelN );  // Sets labelN, bgDraw, selDraw.
    ur_strInit( str, UR_ENC_UTF8, 0 );
    dprog_init( ur_buffer(ep->bgDraw) );
    dprog_init( ur_buffer(ep->selDraw) );

    return (GWidget*) ep;
}


void menu_present( GWidget* wp, GWidget* owner )
{
    EX_PTR;

    ep->owner = owner;
    ep->selRendered = ITEM_UNSET;
    wp->flags = (wp->flags & ~GW_HIDDEN) | GW_UPDATE_LAYOUT;
}


extern void gui_unlink( GWidget* wp );

static void menu_hide( GWidget* wp )
{
    EX_PTR;

    ep->owner = NULL;
    wp->flags |= GW_HIDDEN;
    gui_ungrabMouse( wp );
    gui_unlink( wp );
}


static void menu_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBlkN( ut, ep->dataBlkN );
    ur_markBuffer( ut, ep->labelN );
    ur_markDrawProg( ut, ep->bgDraw );
    ur_markDrawProg( ut, ep->selDraw );
}


static void menu_updateSelection( UThread* ut, GMenu* ep )
{
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    const UBuffer* blk;
    UBuffer* str;
    DPCompiler* save;
    DPCompiler dpc;

    blk = ur_buffer( ep->dataBlkN );
    str = ur_buffer( ep->labelN );
    str->used = 0;
    ur_toText( ut, blk->ptr.cell + ep->selItem, str );

    rc = style + CI_STYLE_LABEL;
    ur_initSeries( rc, UT_STRING, ep->labelN );

    rc = style + CI_STYLE_AREA;
    ur_initCoord(rc, 4);
    rc->coord.n[ 0 ] = ep->wid.area.x + ep->marginX;
    rc->coord.n[ 1 ] = ep->wid.area.y + ep->marginY +
                        ((ep->selMax - 1 - ep->selItem) * ep->itemHeight);
    rc->coord.n[ 2 ] = ep->wid.area.w - ep->marginX * 2;
    rc->coord.n[ 3 ] = ep->itemHeight;

    rc = style + CI_STYLE_MENU_ITEM_SELECTED;
    if( ur_is(rc, UT_BLOCK) )
    {
        save = ur_beginDP( &dpc );
        ur_compileDP( ut, rc, 1 );
        ur_endDP( ut, ur_buffer( ep->selDraw ), save );
    }
}


static void menu_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
    (void) ut;

    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_UP:
            if( ev->code == GLV_BUTTON_LEFT )
            {
                if( ep->selItem != ITEM_UNSET )
                {
                    GLViewEvent sel;

                    sel.type = GUI_EVENT_MENU_SELECTION;
                    sel.code = ep->selItem;
                    sel.x    = ev->x;
                    sel.y    = ev->y;

                    ep->owner->wclass->dispatch( ut, ep->owner, &sel );
                }
                menu_hide( wp );
            }
            break;

        case GLV_EVENT_MOTION:
            if( gui_widgetContains( wp, ev->x, ev->y ) )
            {
                int n = ((int) (wp->area.y + wp->area.h) - ev->y) /
                        ep->itemHeight;
                if( n >= 0 && n < ep->selMax )
                    ep->selItem = n;
            }
            break;
#if 0
        case GLV_EVENT_WHEEL:
            if( ev->y < 0 )
                choice_selectNextItem( ut, ep );
            else
                choice_selectPrevItem( ut, ep );
            break;

        case GLV_EVENT_KEY_DOWN:
            switch( ev->code )
            {
                case KEY_Up:
                    choice_selectPrevItem( ut, ep );
                    return;
                case KEY_Down:
                    choice_selectNextItem( ut, ep );
                    return;
            }
            // Fall through...

        case GLV_EVENT_KEY_UP:
            gui_ignoreEvent( ev );
            break;
#endif
    }
}


static void menu_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    UThread* ut = glEnv.guiUT;
    UCell* style = glEnv.guiStyle;
    const GRect* orect = &ep->owner->area;
    const TexFont* tf;
    int itemCount;
    UCell* rc;


    rc = style + CI_STYLE_MENU_MARGIN;
    if( ur_is(rc, UT_COORD) )
    {
        ep->marginX = rc->coord.n[0];
        ep->marginY = rc->coord.n[1];
    }
    else
    {
        ep->marginX = 8;
        ep->marginY = 8;
    }

    tf = ur_texFontV( ut, style + CI_STYLE_LIST_FONT );
    ep->itemHeight = tf ? txf_lineSpacing( tf ) + 2 : DEFAULT_ITEM_HEIGHT;

    itemCount = ur_buffer( ep->dataBlkN )->used;

    size->minW    =
    size->maxW    = orect->w + ep->marginX * 2;
    size->minH    =
    size->maxH    = ep->itemHeight * itemCount + ep->marginY * 2;
    size->weightX =
    size->weightY = GW_WEIGHT_FIXED;
    size->policyX =
    size->policyY = GW_POL_FIXED;
}


static void menu_layout( GWidget* wp )
{
    EX_PTR;
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    DPCompiler* save;
    DPCompiler dpc;
    UThread* ut = glEnv.guiUT;

    {
    GSizeHint hint;
    const GRect* orect = &ep->owner->area;

    menu_sizeHint( wp, &hint );

    wp->area.x = orect->x - ep->marginX;
    wp->area.y = orect->y - hint.minH;
    wp->area.w = hint.minW;
    wp->area.h = hint.minH;
    }

    // Keep menu on screen.
    if( wp->area.y < 0 )
        wp->area.y = 0;

    save = ur_beginDP( &dpc );

    // Set draw list variables.

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );

    // Compile draw lists.

    rc = style + CI_STYLE_MENU_BG;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );

    {
    UBlockIt bi;
    UBuffer str;
    int px;
    const UBuffer* blk = ur_bufferE( ep->dataBlkN );
    const TexFont* tf  = ur_texFontV( ut, style + CI_STYLE_LIST_FONT );
    if( tf )
    {
        px = wp->area.x + 12;
        dpc.penY = wp->area.y + wp->area.h;

        ur_strInit( &str, UR_ENC_UTF8, 0 );

        bi.it  = blk->ptr.cell;
        bi.end = bi.it + blk->used;
        ur_foreach( bi )
        {
            dpc.penX = px;
            dpc.penY -= ep->itemHeight;

            str.used = 0;
            ur_toText( ut, bi.it, &str );
            dp_drawText( &dpc, tf, str.ptr.b, str.ptr.b + str.used );
        }

        ur_strFree( &str );
    }
    ep->selMax = blk->used;
    }

    ur_endDP( ut, ur_buffer(ep->bgDraw), save );
}


static void menu_render( GWidget* wp )
{
    EX_PTR;

    if( ! (glEnv.guiStyle = gui_style( glEnv.guiUT )) )
        return;

    if( wp->flags & GW_UPDATE_LAYOUT )
    {
        wp->flags &= ~GW_UPDATE_LAYOUT;
        menu_layout( wp );
    }

    ur_runDrawProg( glEnv.guiUT, ep->bgDraw );

    if( ep->selItem != ITEM_UNSET )
    {
        if( ep->selRendered != ep->selItem )
        {
            ep->selRendered = ep->selItem;
            menu_updateSelection( glEnv.guiUT, ep );
        }
        ur_runDrawProg( glEnv.guiUT, ep->selDraw );
    }
}


GWidgetClass wclass_menu =
{
    "menu",
    menu_make,          widget_free,        menu_mark,
    menu_dispatch,      menu_sizeHint,      menu_layout,
    menu_render,        gui_areaSelect,     widget_setNul,
    0, GW_HIDDEN
};


// EOF
