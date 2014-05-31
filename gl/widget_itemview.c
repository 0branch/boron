/*
  Boron OpenGL GUI
  Copyright 2014 Karl Robillard

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
#include "glh.h"
#include "boron-gl.h"
#include "draw_ops.h"
#include "draw_prog.h"
#include "os.h"
#include "gl_atoms.h"

extern void dp_toString( UThread* ut, const UCell* cell, UBinaryIter* bi );


/*
    Item view render specification.

      ; Same as widget layout
        'hbox opt coord! block!
      | 'vbox opt coord! block!
      | 'grid coord! opt coord! block!

      ; Same as draw-prog
      | 'image ...
      | 'text ... string!
*/


typedef struct
{
    GWidget   wid;

    // Double-buffered drawing.
    UBuffer   fc[2];            // glMultiDrawArrays first & count.
    GLuint    vbo[2];
    int       vboSize[2];       // Byte size.

    UIndex    dataBlkN;
    UIndex    layoutBlkN;
    int       itemCount;
    uint16_t  itemWidth;
    uint16_t  itemHeight;

    UIndex    bindCtxN;
    UIndex    selRow;
    uint8_t   colCount;
    int8_t    selCol;
    uint8_t   modVbo;
    uint8_t   updateMethod;
}
GItemView;

#define EX_PTR  GItemView* ep = (GItemView*) wp

#define ALOC_POS    0
#define ALOC_UV     1


enum UpdateMethod
{
    IV_UPDATE_NEVER,    // Items are guarranteed to not change.
    IV_UPDATE_ALL,      // Rebuild attributes for all items before each render.
    IV_UPDATE_2         // Rebuild attributes for two items before each render.
};


//----------------------------------------------------------------------------


static const uint8_t iv_args_image[] =
{
    GUIA_ARGW, 1, UT_COORD,
    GUIA_ARGW, 2, UT_COORD, UT_PAREN,
    GUIA_END
};

static const uint8_t iv_args_font[] =
{
    GUIA_ARGW, 1, UT_FONT,
    GUIA_END
};

static const uint8_t iv_args_text[] =
{
    GUIA_ARGW, 1, UT_COORD,
    GUIA_ARGW, 3, UT_STRING, UT_INT, UT_DECIMAL,
    GUIA_END
};

typedef struct
{
    GLfloat* attr;
    GLuint   drawFirst;
    GLsizei  drawCount;
    UIndex   fontN;     // TexFont binary
    GLfloat  penX;
    GLfloat  penY;
}
DrawContext;


#define FETCH_ARGS(spec) \
    if( ! gui_parseArgs( ut, bi, wp->wclass, spec, arg ) ) \
        return UR_THROW

/*
  Updates DrawContext attr & drawCount.

  Return UR_OK/UR_THROW.
*/
int itemview_parse( DrawContext* dc, UThread* ut, UBlockIter* bi, GWidget* wp )
{
    const UCell* arg[2];
    UAtom atom;
    int opcode;
    float x, y;

    while( bi->it != bi->end )
    {
        if( ur_is(bi->it, UT_WORD) )
        {
            atom = ur_atom( bi->it );
            opcode = ur_atomsSearch( glEnv.drawOpTable, DOP_COUNT, atom );
            switch( opcode )
            {
                case DOP_IMAGE:
                    FETCH_ARGS( iv_args_image );
                    //printf( "KR IV image\n" );
                    break;

                case DOP_FONT:
                    FETCH_ARGS( iv_args_font );
                    dc->fontN = ur_fontTF( arg[0] );
                    break;

                case DOP_TEXT:
                    FETCH_ARGS( iv_args_text );

                    x = dc->penX + (float) arg[0]->coord.n[0];
                    y = dc->penY + (float) arg[0]->coord.n[1];

                {
                    DrawTextState dts;
                    UBinaryIter si;
                    int glyphCount;

                    //printf( "KR IV text\n" );
                    dp_toString( ut, arg[1], &si );
                    vbo_drawTextInit( &dts,
                                      (TexFont*) ur_buffer( dc->fontN )->ptr.v,
                                      x, y );
                    dts.emitTris = 1;
                    glyphCount = vbo_drawText( &dts, dc->attr + 3, dc->attr, 5,
                                               si.it, si.end );
                    /*
                    while( si.it != si.end )
                        putchar( *si.it++ );
                    putchar( '\n' );
                    */
                    dc->attr      += glyphCount * 6 * 5;
                    dc->drawCount += glyphCount * 6;
                }
                    break;

                default:
                    goto invalid_op;
            }
        }
        else
        {
            goto invalid_op;
        }
    }
    return UR_OK;

invalid_op:

    return ur_error( ut, UR_ERR_SCRIPT, "Invalid item-view instruction" );
}


#define iterateAll(bi,blkN) \
    bi.buf = ur_buffer( blkN ); \
    bi.it  = bi.buf->ptr.cell; \
    bi.end = bi.it + bi.buf->used


static void itemview_rebuildAttr( UThread* ut, GItemView* ep,
                                  const UBuffer* items, int page )
{
#define TRI_ATTR_BYTES  (3 * 5 * sizeof(GLfloat))
    DrawContext dc;
    UBlockIter bi;
    UBlockIter layout;
    UBuffer* fcBuf;
    const UCell* loStart;
    UCell* cell;
    float* attr;
    int bsize;


    // Reserve 400 triangles for each item.
    ur_binReserve( &glEnv.tmpBin, items->used * 400 * TRI_ATTR_BYTES );
    attr = glEnv.tmpBin.ptr.f;

    // Reserve first & count arrays.  fcBuf->used tracks the number of items,
    // which is half the number of integers used for both arrays.
    fcBuf = &ep->fc[ page ];
    ur_arrReserve( fcBuf, items->used * 2 );
    fcBuf->used = 0;

    dc.attr = attr;
    dc.drawFirst = 0;

#if 0
    dc.penX = ep->wid.area.x;
    dc.penY = ep->wid.area.y + ep->wid.area.h;
#else
    dc.penX = 0.0;
    dc.penY = ep->wid.area.h;
#endif

    // Setup the data item and layout iterators.
    bi.it  = items->ptr.cell;
    bi.end = bi.it + items->used;

    layout.buf = ur_buffer( ep->layoutBlkN );
    loStart    = layout.buf->ptr.cell;
    layout.end = loStart + layout.buf->used;

    ur_foreach( bi )
    {
        // Set item word to current data value.
        cell = ur_ctxCell( ur_buffer( ep->bindCtxN ), 0 );
        *cell = *bi.it;

        dc.drawCount = 0;
        dc.penY -= ep->itemHeight;

        layout.it = loStart;
        if( ! itemview_parse( &dc, ut, &layout, (GWidget*) ep ) )
        {
            // Clear exceptions.
            UBuffer* buf = ur_errorBlock(ut);
            buf->used = 0;

            printf( "KR itemview_parse failed\n" );
        }

        printf( "KR fc %d,%d\n", dc.drawFirst, dc.drawCount );

        fcBuf->ptr.i[ fcBuf->used ] = dc.drawFirst;
        fcBuf->ptr.i[ fcBuf->used + items->used ] = dc.drawCount;
        ++fcBuf->used;

        dc.drawFirst += dc.drawCount;
    }

    bsize = dc.drawFirst * TRI_ATTR_BYTES;
    if( ep->vboSize[ page ] < bsize )
    {
        ep->vboSize[ page ] = bsize;
        glBufferData( GL_ARRAY_BUFFER, bsize, attr, GL_STATIC_DRAW );
    }
    else
    {
        float* abuf = (float*) glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
        if( abuf )
        {
            memCpy( abuf, attr, bsize );
            glUnmapBuffer( GL_ARRAY_BUFFER );
        }
    }
}


//----------------------------------------------------------------------------


/*-wid-
    item-view   items    layout
                block!   block!
*/
static const uint8_t itemview_args[] =
{
    GUIA_ARGW, 1, UT_BLOCK,
    GUIA_ARGW, 1, UT_BLOCK,
    GUIA_END
};

void itemview_free( GWidget* wp );

static GWidget* itemview_make( UThread* ut, UBlockIter* bi,
                               const GWidgetClass* wclass )
{
    GItemView* ep;
    UIndex itemBlkN;
    const UCell* arg[2];
    //int ok;

    if( ! gui_parseArgs( ut, bi, wclass, itemview_args, arg ) )
        return 0;

    ep = (GItemView*) gui_allocWidget( sizeof(GItemView), wclass );

    assert( sizeof(GLint) == sizeof(GLsizei) );
    ur_arrInit( ep->fc + 0, sizeof(GLint), 0 );
    ur_arrInit( ep->fc + 1, sizeof(GLint), 0 );

    glGenBuffers( 2, ep->vbo );
    ep->vboSize[0] = ep->vboSize[1] = 0;

    ep->selCol     = -1;
    ep->selRow     = -1;
    ep->itemCount  = 0;
    ep->itemWidth  = 0;
    ep->itemHeight = 0;
    ep->modVbo     = 0;
    ep->updateMethod = IV_UPDATE_2;
    ep->layoutBlkN = UR_INVALID_BUF;
    ep->bindCtxN   = UR_INVALID_BUF;

    itemBlkN = arg[0]->series.buf;
    if( ur_isShared( itemBlkN ) )
        ep->dataBlkN  = UR_INVALID_BUF;
    else
        ep->dataBlkN  = itemBlkN;

    //--------------------------------------------
    // Parse layout block

    {
    UBlockIter bi;

    ur_blkSlice( ut, &bi, arg[1] );
    if( (bi.end - bi.it) < 2 )
        goto bad_layout;

    if( ur_is(bi.it, UT_COORD) )
    {
        // Fixed layout.
        static char itemStr[] = "item";
        UBuffer* blk;
        UBuffer* ctx;

        ep->itemWidth  = bi.it->coord.n[0];
        ep->itemHeight = bi.it->coord.n[1];
        printf( "KR item dim %d,%d\n", ep->itemWidth, ep->itemHeight );
        ++bi.it;

        if( (bi.it == bi.end) || ! ur_is(bi.it, UT_BLOCK) )
            goto bad_layout;
        if( ! (blk = ur_bufferSerM( bi.it )) )
            goto fail;
        ep->layoutBlkN = bi.it->series.buf;

        ep->bindCtxN = ur_makeContext( ut, 1 );
        ctx = ur_buffer( ep->bindCtxN );
        ur_ctxAppendWord( ctx, ur_internAtom( ut, itemStr, itemStr + 4 ) );
        ur_bind( ut, blk, ctx, UR_BIND_THREAD );
    }
#ifdef ITEMVIEW_BOX_LAYOUT
    else
    {
        // Automatic layout.
        glEnv.guiParsingItemView = 1;
        ok = gui_makeWidgets( ut, arg[1], &ep->wid, 0 );
        glEnv.guiParsingItemView = 0;
        if( ! ok )
            goto fail;
        //ep->wid.child->flags |= GW_HIDDEN;
    }
#endif
    }

    return (GWidget*) ep;

bad_layout:

    ur_error( ut, UR_ERR_SCRIPT, "Invalid item-view layout" );

fail:

    itemview_free( (GWidget*) ep );
    return 0;
}


void itemview_free( GWidget* wp )
{
    EX_PTR;

    ur_arrFree( ep->fc + 0 );
    ur_arrFree( ep->fc + 1 );
    glDeleteBuffers( 2, ep->vbo );
    widget_free( wp );
}


static void itemview_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBlkN( ut, ep->dataBlkN );
    ur_markBlkN( ut, ep->layoutBlkN );
    ur_markCtxN( ut, ep->bindCtxN );
}


static int itemview_validRow( UThread* ut, GItemView* ep, unsigned int row )
{
#ifdef ITEM_HEADER
    if( (ep->headerBlkN > 0) && (ep->dataBlkN > 0) )
    {
        UBuffer* hblk = ur_buffer( ep->headerBlkN );
        UBuffer* blk  = ur_buffer( ep->dataBlkN );
        if( (hblk->used) && (row < (unsigned int) (blk->used / hblk->used)) )
            return 1;
    }
#else
    if( ep->dataBlkN > 0 )
    {
        UBuffer* blk = ur_buffer( ep->dataBlkN );
        if( row < (unsigned int) blk->used )
            return 1;
    }
#endif
    return 0;
}


static void itemview_layout( GWidget* );

static void itemview_selectNextItem( UThread* ut, GWidget* wp )
{
    EX_PTR;
    if( itemview_validRow( ut, ep, ep->selRow + 1 ) )
    {
        ++ep->selRow;
        itemview_layout( wp );
    }
}


static void itemview_selectPrevItem( GWidget* wp )
{
    EX_PTR;
    if( ep->selRow > 0 )
    {
        --ep->selRow;
        itemview_layout( wp );
    }
}


//#define CCELL(N)     (cblk->ptr.cell + N)

static void itemview_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
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
            int row = (((wp->area.y + wp->area.h) - ev->y) / ep->itemHeight)-1;
            if( row != ep->selRow )
            {
                if( itemview_validRow( ut, ep, row ) )
                {
                    ep->selRow = row;
                    itemview_layout( wp );
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
                itemview_selectNextItem( ut, wp );
            else
                itemview_selectPrevItem( wp );
            break;

        case GLV_EVENT_KEY_DOWN:
            switch( ev->code )
            {
                case KEY_Up:
                    itemview_selectPrevItem( wp );
                    return;
                case KEY_Down:
                    itemview_selectNextItem( ut, wp );
                    return;
            }
            // Fall through...

        case GLV_EVENT_KEY_UP:
            gui_ignoreEvent( ev );
            break;
    }
}


static void itemview_calcItemHeight( UThread* ut, GItemView* ep )
{
    /*
    TexFont* tf;
    tf = ur_texFontV( ut, glEnv.guiStyle + CI_STYLE_LIST_FONT );
    if( tf )
        ep->itemHeight = txf_lineSpacing( tf ) + 2;
    */
}


#define MAX_DIM     0x7fff
#define MIN_COLW    120

static void itemview_sizeHint( GWidget* wp, GSizeHint* size )
{
    //UBuffer* blk;
    EX_PTR;
    UThread* ut  = glEnv.guiUT;


    if( ! ep->itemHeight )
        itemview_calcItemHeight( ut, ep );

#ifdef ITEM_HEADER
    blk = ur_buffer( ep->headerBlkN );
    colCount = blk->used;
#endif

    size->minW    = ep->itemWidth;
    size->minH    = ep->itemHeight * 3;
    size->maxW    = MAX_DIM;
    size->maxH    = MAX_DIM;
    size->weightX = 2;
    size->weightY = 3;
    size->policyX = GW_EXPANDING;
    size->policyY = GW_EXPANDING;
}


static void itemview_layout( GWidget* wp )
{
    /*
    UCell* rc;
    UCell* it;
    UBuffer* blk;
    int row, rowCount;
    int col, colCount;
    */
    int itemY;
    //UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    //UIndex strN = 0;
    EX_PTR;
    /*
    DPCompiler* save;
    DPCompiler dpc;
    */


#ifdef ITEM_HEADER
    if( ep->headerBlkN <= 0 )
        return;
#endif
    if( ep->dataBlkN <= 0 )
        return;

    itemview_calcItemHeight( ut, ep );

    itemY = wp->area.y + wp->area.h - ep->itemHeight;


#if 0
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
#endif
}


static void itemview_render( GWidget* wp )
{
    EX_PTR;
    UBuffer* fcBuf;
    int n;


    // Alternate vertex array buffers.
    n = ep->modVbo;
    ep->modVbo = n ^ 1;

    fcBuf = &ep->fc[ n ];
    glBindBuffer( GL_ARRAY_BUFFER, ep->vbo[ n ] );

    if( ep->updateMethod == IV_UPDATE_2 )
    {
        UThread* ut = glEnv.guiUT;
        const UBuffer* items = ur_buffer( ep->dataBlkN );
        if( items->used != fcBuf->used )
        {
            if( items->used )
                itemview_rebuildAttr( ut, ep, items, n );
            else
                fcBuf->used = 0;
        }
        else
        {
            // Update two items...
        }
    }

    if( fcBuf->used )
    {
        // FIXME: Scissor is not affected by transforms (e.g. window dragging)!
        glScissor( wp->area.x, wp->area.y, wp->area.w, wp->area.h );
        glEnable( GL_SCISSOR_TEST );
#if 0
        glEnableVertexAttribArray( ALOC_POS );
        glEnableVertexAttribArray( ALOC_UV );
        glVertexAttribPointer( ALOC_POS, 3, GL_FLOAT, GL_FALSE, 5, NULL + 0 );
        glVertexAttribPointer( ALOC_UV,  2, GL_FLOAT, GL_FALSE, 5, NULL + 3 );
#else
        glEnableClientState( GL_VERTEX_ARRAY );
        glEnableClientState( GL_TEXTURE_COORD_ARRAY );
        glVertexPointer  ( 3, GL_FLOAT, 5 * sizeof(GLfloat), NULL + 0 );
        glTexCoordPointer( 2, GL_FLOAT, 5 * sizeof(GLfloat),
                           NULL + 3 * sizeof(GLfloat) );
#endif
        glPushMatrix();
        glTranslatef( (GLfloat) wp->area.x, (GLfloat) wp->area.y, 0.0f );
        glMultiDrawArrays( GL_TRIANGLES,
                           (GLint*)    fcBuf->ptr.i,
                           (GLsizei*) (fcBuf->ptr.i + fcBuf->used),
                           fcBuf->used );
        glPopMatrix();
        glDisable( GL_SCISSOR_TEST );
    }
}


static int itemview_select( GWidget* wp, UAtom atom, UCell* res )
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


GWidgetClass wclass_itemview =
{
    "item-view",
    itemview_make,      itemview_free,       itemview_mark,
    itemview_dispatch,  itemview_sizeHint,   itemview_layout,
    itemview_render,    itemview_select,
    0, 0
};


// EOF
