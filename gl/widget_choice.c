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
#include "glh.h"
#include "boron-gl.h"
#include "draw_prog.h"
#include "os.h"
#include "gl_atoms.h"


typedef struct
{
    GWidget   wid;
    UIndex    dataBlkN;
    UIndex    actionN;
    UIndex    labelN;
    UIndex    labelDraw;
    uint16_t  selItem;
}
GChoice;

#define EX_PTR  GChoice* ep = (GChoice*) wp
#define FLAG_UPDATE_LABEL   GW_FLAG_USER1


/*-wid-
    choice  options  [action]
            block!   block!
*/
static const uint8_t choice_args[] =
{
    GUIA_ARGW, 1, UT_BLOCK,
    GUIA_OPT,     UT_BLOCK,
    GUIA_END
};

static GWidget* choice_make( UThread* ut, UBlockIter* bi,
                             const GWidgetClass* wclass, GWidget* parent )
{
    GChoice* ep;
    UBuffer* str;
    const UCell* arg[2];

    if( ! gui_parseArgs( ut, bi, wclass, choice_args, arg ) )
        return 0;

    ep = (GChoice*) gui_allocWidget( sizeof(GChoice), wclass, parent );
    ep->dataBlkN = arg[0]->series.buf;
    if( arg[1] )
        ep->actionN = arg[1]->series.buf;

    str = ur_genBuffers( ut, 2, &ep->labelN );  // Sets labelN & labelDraw.
    ur_strInit( str, UR_ENC_LATIN1, 0 );
    dprog_init( ur_buffer(ep->labelDraw) );

    return (GWidget*) ep;
}


static void choice_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBlkN( ut, ep->dataBlkN );
    ur_markBlkN( ut, ep->actionN );
    if( ep->labelN )
        ur_markBuffer( ut, ep->labelN );
    ur_markDrawProg( ut, ep->labelDraw );
}


static int choice_validRow( UThread* ut, GChoice* ep, uint16_t row )
{
    if( ep->dataBlkN > 0 )
    {
        UBuffer* blk  = ur_buffer( ep->dataBlkN );
        if( row < (uint16_t) blk->used )
            return 1;
    }
    return 0;
}


static void choice_updateLabel( UThread* ut, GChoice* ep )
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
    gui_initRectCoord( rc, &ep->wid, UR_ATOM_RECT );

    rc = style + CI_STYLE_CHOICE_ITEM;
    if( ur_is(rc, UT_BLOCK) )
    {
        save = ur_beginDP( &dpc );
        ur_compileDP( ut, rc, 1 );
        ur_endDP( ut, ur_buffer( ep->labelDraw ), save );
    }
}


/*
  Set new selItem value.

  \param index     Valid index not equal to current selItem.
*/
static void choice_selectItem( UThread* ut, GChoice* ep, uint16_t index )
{
    ep->selItem = index;
    ep->wid.flags |= FLAG_UPDATE_LABEL;
    if( ep->actionN )
    {
        const UBuffer* blk = ur_buffer( ep->dataBlkN );
        *gui_value(ut) = blk->ptr.cell[ ep->selItem ];

        gui_doBlockN( ut, ep->actionN );
    }
}


static void choice_selectNextItem( UThread* ut, GChoice* ep )
{
    uint16_t n = ep->selItem + 1;
    if( choice_validRow( ut, ep, n ) )
        choice_selectItem( ut, ep, n );
}


static void choice_selectPrevItem( UThread* ut, GChoice* ep )
{
    if( ep->selItem > 0 )
        choice_selectItem( ut, ep, ep->selItem - 1 );
}


static void choice_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            if( ev->code == GLV_BUTTON_LEFT )
            {
                gui_showMenu( wp, wp->area.x, wp->area.y, ep->dataBlkN,
                              ep->selItem );
                //gui_setKeyFocus( wp );
            }
            break;

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

        case GUI_EVENT_MENU_SELECTION:
            if( ep->selItem != ev->code )
                choice_selectItem( ut, ep, ev->code );
            break;
    }
}


extern int menu_textWidth( UThread*, const TexFont*, const UCell* );

static void choice_sizeHint( GWidget* wp, GSizeHint* size )
{
    EX_PTR;
    UThread* ut = glEnv.guiUT;
    UCell* style = glEnv.guiStyle;
    UCell* rc;


    rc = style + CI_STYLE_CHOICE_SIZE;
    if( ur_is(rc, UT_COORD) )
    {
        size->minW = rc->coord.n[0];
        size->minH = rc->coord.n[1];
        size->maxW = rc->coord.n[2];
    }
    else
    {
        size->minW = 32;
        size->minH = 20;
        size->maxW = 100;
    }

    size->maxH    = size->minH;
    size->weightX = GW_WEIGHT_STD;
    size->weightY = GW_WEIGHT_FIXED;
    size->policyX = GW_POL_WEIGHTED;
    size->policyY = GW_POL_FIXED;

    {
    UBlockIt bi;
    const TexFont* tf = ur_texFontV( ut, style + CI_STYLE_LIST_FONT );
    ur_blockItN( ut, &bi, ep->dataBlkN );
    ur_foreach( bi )
    {
        int iw = menu_textWidth(ut, tf, bi.it) + 16;
        if( size->minW < iw )
            size->minW = iw;
    }
    }

    if( size->maxW < size->minW )
        size->maxW = size->minW;
}


static void choice_layout( GWidget* wp )
{
    UCell* rc;
    UCell* style = glEnv.guiStyle;

    if( ! gDPC )
        return;

    // Set draw list variables.

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );


    // Compile draw lists.
    rc = style + CI_STYLE_CHOICE;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( glEnv.guiUT, rc, 1 );

    wp->flags |= FLAG_UPDATE_LABEL;
}


static void choice_render( GWidget* wp )
{
    GLint saveVao;
    EX_PTR;

    // Here we compile & run an isolated draw-prog for the label.

    glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &saveVao );

    if( wp->flags & FLAG_UPDATE_LABEL )
    {
        wp->flags &= ~FLAG_UPDATE_LABEL;

        assert( 0 == gDPC );
        choice_updateLabel( glEnv.guiUT, ep );
    }

    ur_runDrawProg( glEnv.guiUT, ep->labelDraw );
    glBindVertexArray( saveVao );
}


static int choice_select( GWidget* wp, UAtom atom, UCell* res )
{
    if( atom == UR_ATOM_VALUE )
    {
        EX_PTR;

        //if( ep->selItem > -1 )
        {
#if 1
            UThread* ut = glEnv.guiUT;
            const UBuffer* blk = ur_buffer( ep->dataBlkN );
            *res = blk->ptr.cell[ ep->selItem ];
#else
            UIndex i = ep->selItem;
            ur_setId( res, UT_BLOCK );
            ur_setSeries( res, ep->dataBlkN, i );
#endif
        }
        //else
        //    ur_setId( res, UT_NONE );
        return UR_OK;
    }
    return gui_areaSelect( wp, atom, res );
}


static UStatus choice_set( UThread* ut, GWidget* wp, const UCell* val )
{
    USeriesIter si;
    UIndex i;
    EX_PTR;

#define SERIES_DT(dt)   ((const USeriesType*) (ut->types[ dt ]))

    si.buf = ur_buffer( ep->dataBlkN );
    si.it  = 0;
    si.end = si.buf->used;

    i = SERIES_DT( UT_BLOCK )->find( ut, &si, val, 0 );
    if( i >= 0 && ep->selItem != i )
        choice_selectItem( ut, ep, i );
    return UR_OK;
}


GWidgetClass wclass_choice =
{
    "choice",
    choice_make,        widget_free,        choice_mark,
    choice_dispatch,    choice_sizeHint,    choice_layout,
    choice_render,      choice_select,      choice_set,
    0, FLAG_UPDATE_LABEL
};


// EOF
