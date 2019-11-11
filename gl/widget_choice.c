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


#include <GL/glv_keys.h>
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
                             const GWidgetClass* wclass )
{
    GChoice* ep;
    const UCell* arg[2];

    if( ! gui_parseArgs( ut, bi, wclass, choice_args, arg ) )
        return 0;

    ep = (GChoice*) gui_allocWidget( sizeof(GChoice), wclass );
    ep->dataBlkN = arg[0]->series.buf;
    if( arg[1] )
        ep->actionN = arg[1]->series.buf;
    ep->labelN = ur_makeString( ut, UR_ENC_LATIN1, 0 );
    ep->labelDraw = ur_makeDrawProg( ut );

    return (GWidget*) ep;
}


static void choice_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBlkN( ut, ep->dataBlkN );
    ur_markBlkN( ut, ep->actionN );
    ur_markBuffer( ut, ep->labelN );
    ur_markDrawProg( ut, ep->labelDraw );
}


static int choice_validRow( UThread* ut, GChoice* ep, unsigned int row )
{
    if( ep->dataBlkN > 0 )
    {
        UBuffer* blk  = ur_buffer( ep->dataBlkN );
        if( row < (unsigned int) blk->used )
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


static void choice_selectNextItem( UThread* ut, GWidget* wp )
{
    EX_PTR;
    if( choice_validRow( ut, ep, ep->selItem + 1 ) )
    {
        ++ep->selItem;
        choice_updateLabel( ut, ep );
        if( ep->actionN )
            gui_doBlockN( ut, ep->actionN );
    }
}


static void choice_selectPrevItem( UThread* ut, GWidget* wp )
{
    EX_PTR;
    if( ep->selItem > 0 )
    {
        --ep->selItem;
        choice_updateLabel( ut, ep );
        if( ep->actionN )
            gui_doBlockN( ut, ep->actionN );
    }
}


static void choice_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            gui_setKeyFocus( wp );
            break;

        case GLV_EVENT_WHEEL:
            if( ev->y < 0 )
                choice_selectNextItem( ut, wp );
            else
                choice_selectPrevItem( ut, wp );
            break;

        case GLV_EVENT_KEY_DOWN:
            switch( ev->code )
            {
                case KEY_Up:
                    choice_selectPrevItem( ut, wp );
                    return;
                case KEY_Down:
                    choice_selectNextItem( ut, wp );
                    return;
            }
            // Fall through...

        case GLV_EVENT_KEY_UP:
            gui_ignoreEvent( ev );
            break;
    }
}


extern void button_textSizeHint( UThread* ut, GSizeHint*, const UBuffer* str,
                                 const UCell* styleFont );

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
    size->weightX = 2;
    size->weightY = 1;
    size->policyX = GW_POL_WEIGHTED;
    size->policyY = GW_POL_FIXED;

    button_textSizeHint( ut, size, ur_buffer(ep->labelN),
                         style + CI_STYLE_LIST_FONT );
}


static void choice_layout( GWidget* wp )
{
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    EX_PTR;
    UThread* ut = glEnv.guiUT;

    if( ! gDPC )
        return;

    // Set draw list variables.

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );


    // Compile draw lists.

    rc = style + CI_STYLE_CHOICE;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );

    choice_updateLabel( ut, ep );
}


static void choice_render( GWidget* wp )
{
    EX_PTR;
    ur_runDrawProg( glEnv.guiUT, ep->labelDraw );
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
            UBuffer* blk = ur_buffer( ep->dataBlkN );
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


GWidgetClass wclass_choice =
{
    "choice",
    choice_make,        widget_free,        choice_mark,
    choice_dispatch,    choice_sizeHint,    choice_layout,
    choice_render,      choice_select,
    0, 0
};


// EOF
