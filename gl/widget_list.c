/*
  Boron OpenGL GUI
  Copyright 2011,2019 Karl Robillard

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
    UIndex    headerBlkN;
    UIndex    dataBlkN;
    UIndex    actionN;
    UIndex    dp[2];
    UIndex    selRow;
    uint16_t  colWidth;
    uint16_t  colCount;
    //int8_t    selCol;
    uint8_t   itemHeight;
    uint8_t   charWidth;
    uint8_t   sizeCW;
    uint8_t   sizeCH;
    //GWidget* scrollBar;
}
GList;

#define EX_PTR  GList* ep = (GList*) wp


/*-wid-
    list    headers      items   [char-size]  [action]
            block!/int!  block!  coord!       block!
*/
static const uint8_t listw_args[] =
{
    GUIA_ARGM, 2, UT_BLOCK, UT_INT,
    GUIA_ARGW, 1, UT_BLOCK,
    GUIA_OPT,     UT_COORD,
    GUIA_OPT,     UT_BLOCK,
    GUIA_END
};

static GWidget* listw_make( UThread* ut, UBlockIter* bi,
                            const GWidgetClass* wclass, GWidget* parent )
{
    GList* ep;
    int colCount;
    const UCell* arg[4];

    if( ! gui_parseArgs( ut, bi, wclass, listw_args, arg ) )
        return 0;

    ep = (GList*) gui_allocWidget( sizeof(GList), wclass, parent );

    ep->dp[0]      = ur_makeDrawProg( ut );
    ep->selRow     = -1;
    //ep->selCol     = -1;
    //ep->itemHeight = ep->charWidth = 0;
    ep->dataBlkN   = arg[1]->series.buf;

    if( arg[2] )
    {
        const int16_t* pn = arg[2]->coord.n;
        ep->sizeCW = pn[0];
        ep->sizeCH = pn[1];
    }

    if( arg[3] )
        ep->actionN = arg[3]->series.buf;

    if( ur_is(arg[0], UT_BLOCK) )
    {
        ep->headerBlkN = arg[0]->series.buf;
        colCount = ur_bufferE( ep->headerBlkN )->used;
    }
    else
    {
        ep->headerBlkN = UR_INVALID_BUF;
        colCount = ur_int(arg[0]);
    }

    // Sanity check column count.
    if( colCount < 1 )
        colCount = 1;
    else if( colCount > 16 )
        colCount = 16;
    ep->colCount = colCount;

    return (GWidget*) ep;
}


static void listw_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBlkN( ut, ep->headerBlkN );
    ur_markBlkN( ut, ep->dataBlkN );
    ur_markBlkN( ut, ep->actionN );
    ur_markDrawProg( ut, ep->dp[0] );
}


static int listw_validRow( UThread* ut, GList* ep, unsigned int row )
{
    if( ep->dataBlkN > 0 )
    {
        UBuffer* blk  = ur_buffer( ep->dataBlkN );
        if( row < (unsigned int) (blk->used / ep->colCount) )
            return 1;
    }
    return 0;
}


static void listw_layout( GWidget* );
static int listw_select( GWidget*, UAtom, UCell* );

/*
  Set new selRow value.

  \param index     Valid index not equal to current selRow.
*/
static void listw_selectRow( UThread* ut, GList* ep, UIndex index )
{
    ep->selRow = index;
    listw_layout( &ep->wid );
    if( ep->actionN )
    {
        listw_select( &ep->wid, UR_ATOM_VALUE, gui_value(ut) );
        gui_doBlockN( ut, ep->actionN );
    }
}


static void listw_selectNextItem( UThread* ut, GWidget* wp )
{
    EX_PTR;
    UIndex n = ep->selRow + 1;
    if( listw_validRow( ut, ep, n ) )
        listw_selectRow( ut, ep, n );
}


static void listw_selectPrevItem( UThread* ut, GWidget* wp )
{
    EX_PTR;
    if( ep->selRow > 0 )
        listw_selectRow( ut, ep, ep->selRow - 1 );
}


//#define CCELL(N)     (cblk->ptr.cell + N)

static void listw_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
/*
    UCell* val;
    UCell* cell;
    UBuffer* cblk;

    if( ! wp->cell[0].ctx.valBlk )
        return;
    cblk = ur_blockPtr( wp->cell[0].ctx.valBlk );
    val = ur_s_next(UR_TOS);
*/

    switch( ev->type )
    {
/*
        case GLV_EVENT_FOCUS_IN:
            ur_initType(val, UT_LOGIC);
            ur_logic(val) = 1;
            break;

        case GLV_EVENT_FOCUS_OUT:
            ur_initType(val, UT_LOGIC);
            ur_logic(val) = 0;
            break;

        case GLV_EVENT_RESIZE:
            break;
*/
        case GLV_EVENT_BUTTON_DOWN:
        {
            UIndex row = ((wp->area.y + wp->area.h) - ev->y) / ep->itemHeight;
            if( ep->headerBlkN != UR_INVALID_BUF )
                --row;
            if( row != ep->selRow && listw_validRow( ut, ep, row ) )
                listw_selectRow( ut, ep, row );
            gui_setKeyFocus( wp );
            //printf( "KR row %d\n", row );
        }
            break;
/*
        case GLV_EVENT_BUTTON_UP:
            ur_initType(val, UT_INT);
            ur_int(val) = mapButton( ev->code );
            cell = CCELL( CI_TWIDGETPROTO_MOUSE_UP );
            break;

        case GLV_EVENT_MOTION:
            // coord elements: x, y, dx, dy
            ur_initCoord(val, 5);
            val->coord.n[0] = ev->x - wp->x;
            //val->coord.n[1] = (ui->rootH - ev->y - 1) - wp->y;
            val->coord.n[1] = ev->y - wp->y;
            val->coord.n[2] = glEnv.mouseDeltaX;
            val->coord.n[3] = glEnv.mouseDeltaY;
            val->coord.n[4] = ev->state;

            cell = CCELL( CI_TWIDGETPROTO_MOUSE_MOVE );
            break;
*/
        case GLV_EVENT_WHEEL:
            if( ev->y < 0 )
                listw_selectNextItem( ut, wp );
            else
                listw_selectPrevItem( ut, wp );
            break;

        case GLV_EVENT_KEY_DOWN:
            switch( ev->code )
            {
                case KEY_Up:
                    listw_selectPrevItem( ut, wp );
                    return;
                case KEY_Down:
                    listw_selectNextItem( ut, wp );
                    return;
            }
            // Fall through...

        case GLV_EVENT_KEY_UP:
            gui_ignoreEvent( ev );
            break;

        case GUI_EVENT_WINDOW_CREATED:
            wp->flags |= GW_UPDATE_LAYOUT;
            break;
    }
}


static void listw_calcMetrics( UThread* ut, GList* ep, int autoExpandCol )
{
    static const char* wstr = "w";
    TexFont* tf = ur_texFontV( ut, glEnv.guiStyle + CI_STYLE_LIST_FONT );
    if( tf )
    {
        const uint8_t* str = (const uint8_t*) wstr;
        ep->itemHeight = txf_lineSpacing( tf ) + 2;
        ep->charWidth  = txf_width( tf, str, str+1 );
    }

    if( ep->sizeCW )
        ep->colWidth = (ep->sizeCW * ep->charWidth) / ep->colCount;
    else
        ep->colWidth = autoExpandCol ? ep->wid.area.w / ep->colCount : 120;
}


static void listw_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    UThread* ut = glEnv.guiUT;
    int rowCount = 0;


    if( ! ep->itemHeight )
        listw_calcMetrics( ut, ep, 0 );

    if( ep->headerBlkN != UR_INVALID_BUF )
        ++rowCount;

    if( ep->sizeCW )
    {
        size->minW    = ep->sizeCW * ep->charWidth;
        size->minH    = (ep->sizeCH + rowCount) * ep->itemHeight;
        size->maxW    = size->minW;
        size->maxH    = size->minH;
        size->weightX =
        size->weightY = GW_WEIGHT_FIXED;
        size->policyX =
        size->policyY = GW_POL_FIXED;
    }
    else
    {
        UBuffer* blk = ur_buffer( ep->dataBlkN );
        rowCount += blk->used / ep->colCount;

        size->minW    = ep->colCount * ep->colWidth;
        size->minH    = ep->itemHeight * rowCount;
        size->maxW    = GW_MAX_DIM;
        size->maxH    = GW_MAX_DIM;
        size->weightX = GW_WEIGHT_STD;
        size->weightY = GW_WEIGHT_STD + GW_WEIGHT_STD / 2;
        size->policyX = GW_POL_EXPANDING;
        size->policyY = GW_POL_EXPANDING;
    }
}


static void listw_layout( GWidget* wp )
{
    UCell* rc;
    UCell* it;
    const UBuffer* blk;
    int row, rowCount;
    int col, colCount;
    int itemY;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    UIndex strN = 0;
    EX_PTR;
    DPCompiler* save;
    DPCompiler dpc;


    listw_calcMetrics( ut, ep, 1 );

    itemY = wp->area.y + wp->area.h - ep->itemHeight;


    // Compile draw list for visible items.

    save = ur_beginDP( &dpc );


    // Background

    rc = style + CI_STYLE_LIST_BG;
    if( ur_is(rc, UT_BLOCK) )
    {
        gui_initRectCoord( style + CI_STYLE_AREA, wp, UR_ATOM_RECT );
        ur_compileDP( ut, rc, 1 );
    }


    // Header

    colCount = ep->colCount;

    if( ep->headerBlkN != UR_INVALID_BUF )
    {
        blk = ur_bufferE( ep->headerBlkN );
        it  = blk->ptr.cell;

        for( col = 0; col < colCount; ++col, ++it )
        {
            rc = style + CI_STYLE_LABEL;
            if( ur_is(it, UT_STRING) )
                *rc = *it;

            rc = style + CI_STYLE_AREA;
            ur_initCoord(rc, 4);
            rc->coord.n[0] = wp->area.x + (col * ep->colWidth);
            rc->coord.n[1] = itemY;
            rc->coord.n[2] = ep->colWidth;
            rc->coord.n[3] = ep->itemHeight;

            rc = style + CI_STYLE_LIST_HEADER;
            if( ur_is(rc, UT_BLOCK) )
                ur_compileDP( ut, rc, 1 );
        }
        itemY -= ep->itemHeight;
    }


    // Items

    if( ep->dataBlkN != UR_INVALID_BUF )
    {
        blk = ur_buffer( ep->dataBlkN );
        it  = blk->ptr.cell;
        rowCount = blk->used / colCount;

        for( row = 0; row < rowCount; ++row )
        {
            for( col = 0; col < colCount; ++col, ++it )
            {
                rc = style + CI_STYLE_LABEL;
                if( ur_is(it, UT_STRING) )
                {
                    *rc = *it;
                }
                else
                {
                    UBuffer* str;

                    if( ! strN )
                        strN = ur_makeString( ut, UR_ENC_LATIN1, 32 );

                    ur_initSeries( rc, UT_STRING, strN );

                    str = ur_buffer( strN );
                    str->used = 0;
                    ur_toStr( ut, it, str, 0 );
                }

                rc = style + CI_STYLE_AREA;
                ur_initCoord(rc, 4);
                rc->coord.n[0] = wp->area.x + (col * ep->colWidth);
                rc->coord.n[1] = itemY;
                rc->coord.n[2] = ep->colWidth;
                rc->coord.n[3] = ep->itemHeight;

                rc = style + ((row == ep->selRow) ?
                                    CI_STYLE_LIST_ITEM_SELECTED :
                                    CI_STYLE_LIST_ITEM);
                if( ur_is(rc, UT_BLOCK) )
                    ur_compileDP( ut, rc, 1 );
            }
            itemY -= ep->itemHeight;
        }
    }

    ur_endDP( ut, ur_buffer(ep->dp[0]), save );
}


static void listw_render( GWidget* wp )
{
    EX_PTR;

    ur_runDrawProg( glEnv.guiUT, ep->dp[0] );
}


static int listw_select( GWidget* wp, UAtom atom, UCell* res )
{
    if( atom == UR_ATOM_VALUE /*selection*/ )
    {
        UIndex i;
        EX_PTR;

        if( ep->colCount && (ep->selRow > -1) )
        {
            i = ep->selRow * ep->colCount;
            ur_setId( res, UT_BLOCK );
            ur_setSlice( res, ep->dataBlkN, i, i + ep->colCount );
        }
        else
        {
            ur_setId( res, UT_NONE );
        }
        return UR_OK;
    }
    return gui_areaSelect( wp, atom, res );
}


static UStatus listw_set( UThread* ut, GWidget* wp, const UCell* val )
{
    EX_PTR;

    if( ur_is(val, UT_BLOCK) )
    {
        const UBuffer* buf = ur_bufferSer(val);
        int i = val->series.it;
        if( i < 0 )
            i = 0;
        else if( i >= buf->used )
            i = buf->used - 1;
        ep->dataBlkN = val->series.buf;
        listw_selectRow( ut, ep, i );
    }
    else if( ur_is(val, UT_NONE) )
    {
        ep->dataBlkN = UR_INVALID_BUF;
        ep->selRow = -1;
        listw_layout( &ep->wid );
    }
    return UR_OK;
}


GWidgetClass wclass_list =
{
    "list",
    listw_make,         widget_free,        listw_mark,
    listw_dispatch,     listw_sizeHint,     listw_layout,
    listw_render,       listw_select,       listw_set,
    0, 0
};


// EOF
