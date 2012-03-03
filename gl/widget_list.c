/*
  Boron OpenGL GUI
  Copyright 2011 Karl Robillard

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


typedef struct
{
    GWidget   wid;
    UIndex    headerBlkN;
    UIndex    dataBlkN;
    UIndex    dp[2];
    UIndex    selRow;
    uint8_t   colCount;
    int8_t    selCol;
    uint8_t   itemHeight;
    uint8_t   _pad;
    //GWidget* header;
    //GWidget* scrollBar;
}
GList;

#define EX_PTR  GList* ep = (GList*) wp


static GWidget* listw_make( UThread* ut, UBlockIter* bi,
                            const GWidgetClass* wclass )
{
    if( (bi->end - bi->it) > 2 )
    {
        GList* ep;
        const UCell* arg = bi->it;

        if( ! ur_is(arg + 1, UT_BLOCK) )
            goto bad_arg;
        if( ! ur_is(arg + 2, UT_BLOCK) )
            goto bad_arg;

        ep = (GList*) gui_allocWidget( sizeof(GList), wclass );

        ep->dp[0]      = ur_makeDrawProg( ut );
        ep->selCol     = -1;
        ep->selRow     = -1;
        ep->itemHeight = 0;
        ep->headerBlkN = arg[1].series.buf;
        ep->dataBlkN   = arg[2].series.buf;

        bi->it += 3;
        return (GWidget*) ep;
    }

bad_arg:
    ur_error( ut, UR_ERR_SCRIPT, "list expected header and data blocks" );
    return 0;
}


static void listw_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBlkN( ut, ep->headerBlkN );
    ur_markBlkN( ut, ep->dataBlkN );
    ur_markDrawProg( ut, ep->dp[0] );
}


static int listw_validRow( UThread* ut, GList* ep, unsigned int row )
{
    if( (ep->headerBlkN > 0) && (ep->dataBlkN > 0) )
    {
        UBuffer* hblk = ur_buffer( ep->headerBlkN );
        UBuffer* blk  = ur_buffer( ep->dataBlkN );
        if( (hblk->used) && (row < (unsigned int) (blk->used / hblk->used)) )
            return 1;
    }
    return 0;
}


static void listw_layout( GWidget* );

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
        case GLV_EVENT_CLOSE:
            break;
*/
        case GLV_EVENT_BUTTON_DOWN:
        {
            int row = (((wp->area.y + wp->area.h) - ev->y) / ep->itemHeight)-1;
            if( row != ep->selRow )
            {
                if( listw_validRow( ut, ep, row ) )
                {
                    ep->selRow = row;
                    listw_layout( wp );
                }
            }
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
            ur_initType(val, UT_COORD);
            val->coord.len     = 5;
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
            {
                if( listw_validRow( ut, ep, ep->selRow + 1 ) )
                {
                    ++ep->selRow;
                    listw_layout( wp );
                }
            }
            else
            {
                if( ep->selRow > 0 )
                {
                    --ep->selRow;
                    listw_layout( wp );
                }
            }
            break;
/*
        case GLV_EVENT_KEY_DOWN:
            cell = CCELL( CI_TWIDGETPROTO_KEY_DOWN );
            goto key_handler;

        case GLV_EVENT_KEY_UP:
            cell = CCELL( CI_TWIDGETPROTO_KEY_UP );
key_handler:
            ur_initType(val, UT_INT);
            ur_int(val) = ev->code;

            cell = selectKeyHandler( ut, cell, ev->code );
            if( ! cell )
                return;
            break;
*/
        default:
            return;
    }

/*
    if( ur_is(cell, UT_BLOCK) )
    {
        UR_S_GROW;
        if( UR_EVAL_OK == ur_eval( ut, cell->series.n, 0 ) )
        {
            UR_S_DROP;
        }
    }
*/
}


static void listw_calcItemHeight( UThread* ut, GList* ep )
{
    TexFont* tf;
    tf = ur_texFontV( ut, glEnv.guiStyle + CI_STYLE_LIST_FONT );
    if( tf )
        ep->itemHeight = txf_lineSpacing( tf ) + 2;
}


#define MAX_DIM     0x7fff
#define MIN_COLW    120

static void listw_sizeHint( GWidget* wp, GSizeHint* size )
{
    UBuffer* blk;
    int rowCount;
    int colCount;
    EX_PTR;
    UThread* ut  = glEnv.guiUT;


    if( ! ep->itemHeight )
        listw_calcItemHeight( ut, ep );

    blk = ur_buffer( ep->headerBlkN );
    colCount = blk->used;

    blk = ur_buffer( ep->dataBlkN );
    rowCount = blk->used / colCount;

    size->minW    = colCount * MIN_COLW;
    size->minH    = ep->itemHeight * (rowCount + 1);
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 3;
    size->policyX = GW_EXPANDING;
    size->policyY = GW_EXPANDING;
}


static void listw_layout( GWidget* wp )
{
    UCell* rc;
    UCell* it;
    UBuffer* blk;
    int row, rowCount;
    int col, colCount;
    int itemY;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    UIndex strN = 0;
    EX_PTR;
    DPCompiler* save;
    DPCompiler dpc;


    if( (ep->headerBlkN <= 0) ||(ep->dataBlkN <= 0) )
        return;

    listw_calcItemHeight( ut, ep );

    itemY = wp->area.y + wp->area.h - ep->itemHeight;


    // Compile draw list for visible items.

    save = ur_beginDP( &dpc );
    if( save )
        dpc.shaderProg = save->shaderProg;


    // Header

    blk = ur_buffer( ep->headerBlkN );
    it  = blk->ptr.cell;
    colCount = ep->colCount = blk->used;

    for( col = 0; col < colCount; ++col, ++it )
    {
        rc = style + CI_STYLE_LABEL;
        if( ur_is(it, UT_STRING) )
        {
            *rc = *it;
        }

        rc = style + CI_STYLE_AREA;
        rc->coord.len = 4;
        rc->coord.n[0] = wp->area.x + (col * MIN_COLW);
        rc->coord.n[1] = itemY;
        rc->coord.n[2] = MIN_COLW;
        rc->coord.n[3] = ep->itemHeight;

        rc = style + CI_STYLE_LIST_HEADER;
        if( ur_is(rc, UT_BLOCK) )
            ur_compileDP( ut, rc, 1 );
    }
    itemY -= ep->itemHeight;


    // Items

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

                ur_setId( rc, UT_STRING );
                ur_setSeries( rc, strN, 0 );

                str = ur_buffer( strN );
                str->used = 0;
                ur_toStr( ut, it, str, 0 );
            }

            rc = style + CI_STYLE_AREA;
            rc->coord.len = 4;
            rc->coord.n[0] = wp->area.x + (col * MIN_COLW);
            rc->coord.n[1] = itemY;
            rc->coord.n[2] = MIN_COLW;
            rc->coord.n[3] = ep->itemHeight;

            rc = style + ((row == ep->selRow) ?
                                CI_STYLE_LIST_ITEM_SELECTED :
                                CI_STYLE_LIST_ITEM);
            if( ur_is(rc, UT_BLOCK) )
                ur_compileDP( ut, rc, 1 );
        }
        itemY -= ep->itemHeight;
    }

    ur_endDP( ut, ur_buffer(ep->dp[0]), save );
}


static void listw_render( GWidget* wp )
{
    DPState ds;
    EX_PTR;

    ur_initDrawState( &ds );
    ur_runDrawProg( glEnv.guiUT, &ds, ep->dp[0] );
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
        return 1;
    }
    return gui_areaSelect( wp, atom, res );
}


GWidgetClass wclass_list =
{
    "list",
    listw_make,         widget_free,        listw_mark,
    listw_dispatch,     listw_sizeHint,     listw_layout,
    listw_render,       listw_select,
    0, 0
};


// EOF
