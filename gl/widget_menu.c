/*
  Boron OpenGL GUI
  Copyright 2019,2020 Karl Robillard

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
#include "glh.h"
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
#define DEFAULT_ITEM_HEIGHT  12
#define DO_UNSET        GW_FLAG_USER1
#define WAS_SELECTED    GW_FLAG_USER2
#define hasAction   user16
#define POS_X(wp)   ((int16_t*) &wp->user32)[0]
#define POS_Y(wp)   ((int16_t*) &wp->user32)[1]


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
                           const GWidgetClass* wclass, GWidget* parent )
{
    GMenu* ep;
    UBuffer* str;
    const UCell* arg[2];

    if( ! gui_parseArgs( ut, bi, wclass, menu_args, arg ) )
        return 0;

    ep = (GMenu*) gui_allocWidget( sizeof(GMenu), wclass, parent );
    ep->dataBlkN = arg[0]->series.buf;
    ep->selItem  = arg[0]->series.it;
    ep->selRendered = GUI_ITEM_UNSET;

    str = ur_genBuffers( ut, 3, &ep->labelN );  // Sets labelN, bgDraw, selDraw.
    ur_strInit( str, UR_ENC_UTF8, 0 );
    dprog_init( ur_buffer(ep->bgDraw) );
    dprog_init( ur_buffer(ep->selDraw) );

    return (GWidget*) ep;
}


void menu_setData( GWidget* wp, UIndex blkN )
{
    EX_PTR;

    // menu_sizeHint() will reset hasAction & selMax.
    wp->flags |= GW_UPDATE_LAYOUT;

    ep->dataBlkN = blkN;
    ep->selItem = GUI_ITEM_UNSET;
    ep->selMax = 0;
    ep->selRendered = GUI_ITEM_UNSET;
}


void menu_present( GWidget* wp, GWidget* owner, int px, int py, uint16_t sel )
{
    EX_PTR;
    int f;

    ep->owner = owner;
    ep->selItem = sel;
    ep->selRendered = GUI_ITEM_UNSET;
    POS_X(wp) = px;
    POS_Y(wp) = py;

    f = wp->flags & ~(GW_HIDDEN | DO_UNSET | WAS_SELECTED);
    if( sel == GUI_ITEM_UNSET )
        f |= DO_UNSET;
    wp->flags = f | GW_UPDATE_LAYOUT;
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
    if( ep->labelN )
        ur_markBuffer( ut, ep->labelN );
    ur_markDrawProg( ut, ep->bgDraw );
    ur_markDrawProg( ut, ep->selDraw );
}


static const UCell* menu_selectedCell( UThread* ut, const GMenu* ep )
{
    if( ep->wid.hasAction )
    {
        UBlockIt bi;
        int n = -1;

        ur_blockItN( ut, &bi, ep->dataBlkN );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_BLOCK) )
                continue;
            if( ++n == ep->selItem )
                return bi.it;
        }
        return NULL;
    }
    return ur_bufferE(ep->dataBlkN)->ptr.cell + ep->selItem;
}


static void menu_updateSelection( UThread* ut, GMenu* ep )
{
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    UBuffer* str;

    str = ur_buffer( ep->labelN );
    str->used = 0;
    ur_toText( ut, menu_selectedCell(ut, ep), str );

    rc = style + CI_STYLE_LABEL;
    ur_initSeries( rc, UT_STRING, ep->labelN );

    rc = style + CI_STYLE_AREA;
    ur_initCoord(rc, 4);
    rc->coord.n[ 0 ] = ep->wid.area.x + ep->marginX;
    rc->coord.n[ 1 ] = ep->wid.area.y + ep->marginY +
                        ((ep->selMax - 1 - ep->selItem) * ep->itemHeight);
    rc->coord.n[ 2 ] = ep->wid.area.w - ep->marginX * 2;
    rc->coord.n[ 3 ] = ep->itemHeight;

    rc = style + (ep->wid.flags & DO_UNSET ? CI_STYLE_MENU_ITEM_SELECTED
                                           : CI_STYLE_CMENU_ITEM_SELECTED);
    if( ur_is(rc, UT_BLOCK) )
    {
        ur_compileDrawProg( ut, rc, ep->selDraw );
    }
}


static void menu_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
    int n;
    (void) ut;

    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            if( ! gui_widgetContains( wp, ev->x, ev->y ) )
                menu_hide( wp );
            break;

        case GLV_EVENT_BUTTON_UP:
            if( ev->code == GLV_BUTTON_LEFT ||
                ev->code == GLV_BUTTON_RIGHT )
            {
activate:
                if( ep->selItem != GUI_ITEM_UNSET )
                {
                    GLViewEvent sel;

                    sel.type = GUI_EVENT_MENU_SELECTION;
                    sel.code = ep->selItem;
                    sel.x    = ev->x;
                    sel.y    = ev->y;

                    ep->owner->wclass->dispatch( ut, ep->owner, &sel );

                    if( wp->hasAction )
                    {
                        const UCell* cell = menu_selectedCell(ut, ep);
                        if( cell && ur_is(cell+1, UT_BLOCK) )
                            gui_doBlockN( ut, cell[1].series.buf );
                    }
                }
                if( wp->flags & WAS_SELECTED )
                    menu_hide( wp );
            }
            break;

        case GLV_EVENT_MOTION:
            if( gui_widgetContains( wp, ev->x, ev->y ) )
            {
                n = ((int) (wp->area.y + wp->area.h) - ev->y) / ep->itemHeight;
                if( n >= 0 && n < ep->selMax )
                {
                    ep->selItem = n;
                    wp->flags |= WAS_SELECTED;
                }
            }
            else if( wp->flags & DO_UNSET )
            {
                ep->selItem = GUI_ITEM_UNSET;
            }
            break;

        case GLV_EVENT_KEY_DOWN:
            switch( ev->code )
            {
                case KEY_Up:
                    n = ep->selItem;
                    if( n == GUI_ITEM_UNSET || n == 0 )
                        n = ep->selMax;
                    ep->selItem = n - 1;
                    wp->flags |= WAS_SELECTED;
                    break;

                case KEY_Down:
                    n = ep->selItem;
                    if( n == GUI_ITEM_UNSET || n == (ep->selMax - 1) )
                        ep->selItem = 0;
                    else
                        ep->selItem = n + 1;
                    wp->flags |= WAS_SELECTED;
                    break;

                case KEY_Return:
                    goto activate;

                case KEY_Escape:
                    menu_hide( wp );
                    break;
            }
            break;
    }
}


// Return approximate pixel width of word or string value.
int menu_textWidth( UThread* ut, const TexFont* tf, const UCell* cell )
{
    TexFontGlyph* glyph = txf_glyph( tf, 'M' );
    int gw = glyph ? glyph->width : 9;

    if( ur_is(cell, UT_WORD) )
        return strlen( ur_wordCStr(cell) ) * gw;
    if( ur_isStringType( ur_type(cell) ) )
    {
        USeriesIter si;
        ur_seriesSlice( ut, &si, cell );
        return (si.end - si.it) * gw;
    }
    return 0;
}


static void menu_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    UThread* ut = glEnv.guiUT;
    UCell* style = glEnv.guiStyle;
    const TexFont* tf;
    int itemCount;
    int width = (wp->flags & DO_UNSET) ? 0 : ep->owner->area.w;
    UCell* rc;
    UBlockIt bi;


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

    itemCount = 0;
    wp->hasAction = 0;
    ur_blockItN( ut, &bi, ep->dataBlkN );
    ur_foreach( bi )
    {
        if( ur_is(bi.it, UT_BLOCK) )
        {
            wp->hasAction = 1;
        }
        else
        {
            ++itemCount;
            if( wp->flags & DO_UNSET )
            {
                int iw = menu_textWidth(ut, tf, bi.it);
                if( width < iw )
                    width = iw;
            }
        }
    }
    ep->selMax = itemCount;

    size->minW    =
    size->maxW    = width + ep->marginX * 2;
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
    const TexFont* tf;

    {
    GSizeHint hint;
    menu_sizeHint( wp, &hint );

    wp->area.x = POS_X(wp) - ep->marginX;
    wp->area.y = POS_Y(wp) - hint.minH;
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

    rc = style + (wp->flags & DO_UNSET ? CI_STYLE_MENU_BG : CI_STYLE_CMENU_BG);
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc );

    tf = ur_texFontV( ut, style + CI_STYLE_LIST_FONT );
    if( tf )
    {
        UBuffer str;
        UBlockIt bi;
        int px;

        px = wp->area.x + 12;
        dpc.penY = wp->area.y + wp->area.h;

        ur_strInit( &str, UR_ENC_UTF8, 0 );

        ur_blockItN( ut, &bi, ep->dataBlkN );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_BLOCK) )
                continue;

            dpc.penX = px;
            dpc.penY -= ep->itemHeight;

            str.used = 0;
            ur_toText( ut, bi.it, &str );
            dp_drawText( &dpc, tf, str.ptr.b, str.ptr.b + str.used );
        }

        ur_strFree( &str );
    }

    ur_endDP( ut, ur_buffer(ep->bgDraw), save );
}


static void menu_render( GWidget* wp )
{
    GLint saveVao;
    EX_PTR;

    if( ! (glEnv.guiStyle = gui_style( glEnv.guiUT )) )
        return;

    glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &saveVao );

    if( wp->flags & GW_UPDATE_LAYOUT )
    {
        wp->flags &= ~GW_UPDATE_LAYOUT;
        menu_layout( wp );
    }

    ur_runDrawProg( glEnv.guiUT, ep->bgDraw );

    if( ep->selItem != GUI_ITEM_UNSET )
    {
        if( ep->selRendered != ep->selItem )
        {
            ep->selRendered = ep->selItem;
            menu_updateSelection( glEnv.guiUT, ep );
        }
        ur_runDrawProg( glEnv.guiUT, ep->selDraw );
    }

    glBindVertexArray( saveVao );
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
