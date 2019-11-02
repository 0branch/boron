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


#include "boron-gl.h"
#include "draw_prog.h"
#include "os.h"
#include "gl_atoms.h"


typedef struct
{
    GWidget wid;
    UIndex  textN;
}
GLabel;

#define EX_PTR  GLabel* ep = (GLabel*) wp


/*-wid-
    label   text
            string!
*/
static const uint8_t label_args[] =
{
    GUIA_ARGW, 1, UT_STRING,
    GUIA_END
};

static GWidget* label_make( UThread* ut, UBlockIter* bi,
                            const GWidgetClass* wclass )
{
    GLabel* ep;
    const UCell* arg;

    if( ! gui_parseArgs( ut, bi, wclass, label_args, &arg ) )
        return 0;

    ep = (GLabel*) gui_allocWidget( sizeof(GLabel), wclass );
    ep->wid.flags |= GW_NO_INPUT;
    ep->textN = arg->series.buf;
    return (GWidget*) ep;
}


static void label_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;
    if( ep->textN > UR_INVALID_BUF )    // Also acts as (! ur_isShared(n))
        ur_markBuffer( ut, ep->textN );
}


static void label_sizeHint( GWidget* wp, GSizeHint* size )
{
    TexFont* tf;
    UBuffer* str;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    EX_PTR;

    size->minW    = 0;
    size->maxW    = 100;
    size->weightX = 1;
    size->weightY = 1;
    size->policyX = GW_POL_FIXED;
    size->policyY = GW_POL_FIXED;

    tf = ur_texFontV( ut, style + CI_STYLE_CONTROL_FONT );
    if( tf && ep->textN != UR_INVALID_BUF )
    {
        str = ur_buffer( ep->textN );
        if( ! ur_strIsUcs2(str) )
        {
            int textDim[2];
            txf_pixelSize( tf, str->ptr.b, str->ptr.b + str->used, textDim );

            size->minW = textDim[0];
            if( size->maxW < size->minW )
                size->maxW = size->minW;

            size->minH = size->maxH = textDim[1] + 8;
            return;
        }
    }

    size->minH = size->maxH = 0;
}


static void label_layout( GWidget* wp )
{
    UCell* rc;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    EX_PTR;


    // Set draw list variables.

    rc = style + CI_STYLE_LABEL;
    ur_initSeries( rc, UT_STRING, ep->textN );

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );


    // Compile draw list.

    rc = style + CI_STYLE_LABEL_DL;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );
}


static int label_select( GWidget* wp, UAtom atom, UCell* res )
{
    EX_PTR;
    if( atom == UR_ATOM_TEXT )
    {
        ur_initSeries(res, UT_STRING, ep->textN);
        return UR_OK;
    }
    return gui_areaSelect( wp, atom, res );
}


GWidgetClass wclass_label =
{
    "label",
    label_make,         widget_free,        label_mark,
    widget_dispatch,    label_sizeHint,     label_layout,
    widget_renderNul,   label_select,
    0, 0
};


// EOF
