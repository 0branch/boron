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


#include "glh.h"
#include <GL/glv_keys.h>
#include "boron-gl.h"
#include "draw_prog.h"
#include "os.h"
#include "gl_atoms.h"


enum LineEditState
{
    LEDIT_STATE_DISPLAY,
    LEDIT_STATE_EDIT
};

typedef struct
{
    GWidget  wid;
    UIndex   strN;
    UIndex   codeN;
    UIndex   filterN;
    UIndex   vboN;          // Text & cursor
    uint16_t maxChars;
    uint16_t drawn;
    uint16_t dpSwitch;
    uint16_t editPos;
    uint16_t editEndPos;    // Selected text.
    uint16_t newCursorX;
    uint8_t  state;
}
LineEdit;

#define EX_PTR  LineEdit* ep = (LineEdit*) wp


#define _bitIsSet(mem,n)    (mem[(n)>>3] & 1<<((n)&7))

#define LEDIT_APV           4
#define LEDIT_ATTR_SIZE     (LEDIT_APV * sizeof(GLfloat))

#define MARGIN_L    8
#define MARGIN_B    4

#define flagged(wf)   (wp->flags & wf)
#define setFlag(wf)   wp->flags |= wf
#define clrFlag(wf)   wp->flags &= ~wf

#define CHANGED         GW_FLAG_USER1
#define NEW_CURSORX     GW_FLAG_USER2


static UIndex ledit_vbo( UThread* ut, int maxChars )
{
    UBuffer* buf;
    GLuint* gbuf;
    uint16_t* dst;
    UIndex resN;

    resN = ur_makeVbo( ut, GL_DYNAMIC_DRAW,
                       LEDIT_APV * 2 + LEDIT_APV * 4 * maxChars, NULL,
                       2 + 6 * maxChars, NULL );
    buf = ur_buffer( resN );
    gbuf = vbo_bufIds(buf);
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gbuf[1] );
    dst = (uint16_t*) glMapBuffer( GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY );
    if( dst )
    {
        // Cursor verticies.
        dst[0] = 0;
        dst[1] = 1;

        vbo_initTextIndices( dst + 2, 2, maxChars );
        glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );
    }
    return resN;
}


static const uint8_t ledit_args[] =
{
    GUIA_ARGW, UT_STRING,
    GUIA_OPT,  UT_INT,
    GUIA_END
};

/*
    line-edit word!/string! <max-chars>
*/
static GWidget* ledit_make( UThread* ut, UBlockIter* bi,
                            const GWidgetClass* wclass )
{
    LineEdit* ep;
    const UCell* arg[2];
    int maxChars = 32;

    if( ! gui_parseArgs( ut, bi, wclass, ledit_args, arg ) )
        return 0;

    if( ur_isShared( arg[0]->series.buf ) )
    {
        ur_error( ut, UR_ERR_SCRIPT, "line-edit cannot modify shared string" );
        return 0;
    }

    if( arg[1] )
        maxChars = ur_int(arg[1]);

    ep = (LineEdit*) gui_allocWidget( sizeof(LineEdit), wclass );
    ep->wid.flags |= CHANGED;

    //ep->state = LEDIT_STATE_DISPLAY;
    ep->strN     = arg[0]->series.buf;
    ep->maxChars = maxChars;
    ep->vboN     = ledit_vbo( ut, maxChars );

    return (GWidget*) ep;
}


static void ledit_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBuffer( ut, ep->strN );
    ur_markBuffer( ut, ep->filterN );
    ur_markBuffer( ut, ep->vboN );
    ur_markBlkN( ut, ep->codeN );
}


struct PasteData
{
    UThread* ut;
    LineEdit* ep;
};


static void ledit_pasteCB( const char* data, int len, void* user )
{
    struct PasteData* pd = (struct PasteData*) user;
    UThread* ut  = pd->ut;
    LineEdit* ep = pd->ep;
    UBuffer* str = ur_buffer( ep->strN );

    if( str->elemSize == 1 )
    {
        ur_arrExpand( str, ep->editPos, len );
        memcpy( str->ptr.b + ep->editPos, data, len );
        ep->editPos += len;
    }
}


static void ledit_paste( UThread* ut, LineEdit* wd )
{
    struct PasteData pd;
    pd.ut = ut;
    pd.ep = wd;
    glv_clipboardText( glEnv.view, ledit_pasteCB, &pd );
}


static void ledit_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;

    //printf( "KR ledit %d\n", ev->type );
    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            if( (ev->code == GLV_BUTTON_LEFT) ||
                (ev->code == GLV_BUTTON_MIDDLE) )
            {
                ep->state = LEDIT_STATE_EDIT;
                gui_setKeyFocus( wp );

                if( ev->code == GLV_BUTTON_LEFT )
                {
                    // Don't have access to font here so just notify render.
                    ep->newCursorX = ev->x;
                    setFlag( NEW_CURSORX );
                }
                else // if( ev->code == GLV_BUTTON_MIDDLE )
                {
                    ledit_paste( ut, ep );
                    setFlag( CHANGED );
                }
            }
            break;

        case GLV_EVENT_BUTTON_UP:
        case GLV_EVENT_MOTION:
            break;

        case GLV_EVENT_KEY_DOWN:
            if( ep->strN )
            {
                UBuffer* str = ur_buffer( ep->strN );

                if( ev->state & GLV_MASK_CTRL )
                {
                    if( ev->code == KEY_k )
                    {
                        // Remove from cursor to end (from Bash).
                        int len = str->used - ep->editPos;
                        if( len > 0 )
                        {
                            ur_arrErase( str, ep->editPos, len );
                            setFlag( CHANGED );
                        }
                    }
                    else if( ev->code == KEY_v )
                    {
                        ledit_paste( ut, ep );
                        setFlag( CHANGED );
                    }
                    break;
                }

                switch( ev->code )
                {
                    case KEY_Return:
                        goto activate;

                    case KEY_Left:
                        if( ep->editPos > 0 )
                        {
                            --ep->editPos;
                            setFlag( CHANGED );
                        }
                        break;

                    case KEY_Right:
                        if( ep->editPos < str->used )
                        {
                            ++ep->editPos;
                            setFlag( CHANGED );
                        }
                        break;

                    case KEY_Home:
                        ep->editPos = 0;
                        setFlag( CHANGED );
                        break;

                    case KEY_End:
                        ep->editPos = str->used;
                        setFlag( CHANGED );
                        break;

                    case KEY_Insert:
                        break;

                    case KEY_Delete:
                        if( ep->editPos < str->used )
                        {
                            ur_arrErase( str, ep->editPos, 1 );
                            setFlag( CHANGED );
                        }
                        break;

                    case KEY_Back_Space:
                        if( ep->editPos > 0 )
                        {
                            --ep->editPos;
                            ur_arrErase( str, ep->editPos, 1 );
                            setFlag( CHANGED );
                        }
                        break;

                    default:
                        if( str->used < ep->maxChars )
                        {
                            int key = KEY_ASCII(ev);
                            if( key >= ' ' )
                            {
                                if( ep->filterN )
                                {
                                    UBuffer* bin = ur_buffer( ep->filterN );
                                    if( ! _bitIsSet( bin->ptr.b, key ) )
                                        break;
                                }

                                ur_arrExpand( str, ep->editPos, 1 );
                                if( ur_strIsUcs2(str) )
                                    str->ptr.u16[ ep->editPos ] = key;
                                else
                                    str->ptr.b[ ep->editPos ] = key;
                                ++ep->editPos;
                                setFlag( CHANGED );
                            }
                            else
                            {
                                gui_ignoreEvent( ev );
                            }
                        }
                        break;
                }
            }
            break;

        case GLV_EVENT_KEY_UP:
            gui_ignoreEvent( ev );
            break;

        case GLV_EVENT_FOCUS_IN:
        case GLV_EVENT_FOCUS_OUT:
            break;
    }
    return;

activate:

    if( ep->codeN )
        gui_doBlockN( ut, ep->codeN );
}


static void ledit_sizeHint( GWidget* wp, GSizeHint* size )
{
    UCell* rc;
    TexFont* tf;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    EX_PTR;

    rc = style + CI_STYLE_BUTTON_SIZE;
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
    size->policyX = GW_EXPANDING;
    size->policyY = GW_FIXED;

    tf = ur_texFontV( ut, style + CI_STYLE_EDIT_FONT );
    if( tf )
    {
        TexFontGlyph* glyph = txf_glyph( tf, 'W' );
        int width = ep->maxChars * (glyph ? glyph->width : 9);
        if( width > size->minW )
            size->minW += width;
        if( size->maxW < size->minW )
            size->maxW = size->minW;
    }
}


static void ledit_layout( GWidget* wp )
{
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    UCell* rc;
    EX_PTR;


    // Set draw list variables.

    rc = style + CI_STYLE_LABEL;
    ur_setId( rc, UT_STRING );
    ur_setSeries( rc, ep->strN, 0 );

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );


    // Compile draw lists.

    if( ! gDPC )
        return;

    ep->dpSwitch = dp_beginSwitch( gDPC, 2 );

    rc = style + CI_STYLE_EDITOR;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );
    dp_endCase( gDPC, ep->dpSwitch );

    rc = style + CI_STYLE_EDITOR_ACTIVE;
    if( ur_is(rc, UT_BLOCK) )
        ur_compileDP( ut, rc, 1 );
    dp_endCase( gDPC, ep->dpSwitch );

    dp_endSwitch( gDPC, ep->dpSwitch, ep->state );

    setFlag( CHANGED );

#if 0
    rc = style + CI_STYLE_EDITOR_CURSOR;
    if( ur_is(rc, UT_BLOCK) )
    {
        DPCompiler dc;
        DPCompiler* save;

        save = gx_beginDP( &dc );
        if( save )
            dc.shaderProg = save->shaderProg;
        ur_compileDP( ut, rc, 1 );
        gx_endDP( ur_buffer( ep->textResN ), save );

        //dp_compile( &dc, ut, rc->series.n );
    }
#endif
}


extern void gr_drawText( TexFont* tf, const char* it, const char* end );

static void ledit_render( GWidget* wp )
{
    DrawTextState ds;
    GLuint* buf;
    GLfloat* fdata;
    UBuffer* str;
    TexFont* tf;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    EX_PTR;

    if( ! ep->strN )
        return;

    tf = ur_texFontV( ut, style + CI_STYLE_EDIT_FONT );
    if( tf )
    {
        str = ur_buffer( ep->strN );
        buf = vbo_bufIds( ur_buffer(ep->vboN) );

        if( flagged( NEW_CURSORX ) )
        {
            int pos;
            clrFlag( NEW_CURSORX );
            pos = txf_charAtPixel( tf, str->ptr.b, str->ptr.b + str->used,
                                   ep->newCursorX - wp->area.x - MARGIN_L );
            if( pos < 0 )
                pos = (ep->newCursorX > wp->area.x + MARGIN_L) ? str->used : 0;
            if( pos != ep->editPos )
            {
                ep->editPos = pos;
                setFlag( CHANGED );
            }
        }

        glEnableClientState( GL_TEXTURE_COORD_ARRAY );

        glBindBuffer( GL_ARRAY_BUFFER, buf[0] );

        if( flagged( CHANGED ) )
        {
            clrFlag( CHANGED );
            fdata = (float*) glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
            if( fdata )
            {
                // Cursor verticies.
                int xpos = txf_width(tf, str->ptr.b,
                                         str->ptr.b + ep->editPos );

                // FIXME: UVs require black pixels at upper-left of texture.
                fdata[0] = 0.003f;
                fdata[1] = 0.0f;  //95f;
                fdata[2] = (GLfloat) (wp->area.x + xpos + MARGIN_L);
                fdata[3] = (GLfloat) wp->area.y;

                fdata[4] = 0.003f;
                fdata[5] = 0.01f; //998f;
                fdata[6] = fdata[2];
                fdata[7] = fdata[3] + txf_lineSpacing( tf );

                //glBufferSubData(GL_ARRAY_BUFFER, 0, LEDIT_ATTR_SIZE*2, data);

                fdata += LEDIT_APV * 2;

                vbo_drawTextInit( &ds, tf, wp->area.x + MARGIN_L,
                                           wp->area.y + MARGIN_B );

                ep->drawn = 6 * vbo_drawText( &ds, fdata, fdata + 2, LEDIT_APV,
                                          str->ptr.b, str->ptr.b + str->used );
                glUnmapBuffer( GL_ARRAY_BUFFER );
            }
            //printf( "KR ledit draw\n" );
        }

        glTexCoordPointer( 2, GL_FLOAT, LEDIT_ATTR_SIZE, NULL + 0 );
        glVertexPointer  ( 2, GL_FLOAT, LEDIT_ATTR_SIZE, NULL + 8 );
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buf[1] );

        glColor4f( 0.0, 0.0, 0.0, 1.0 );
        glDrawElements( GL_TRIANGLES, ep->drawn, GL_UNSIGNED_SHORT, NULL + 4 );
        if( gui_hasFocus( wp ) & GW_FOCUS_KEY )
            glDrawElements( GL_LINES, 2, GL_UNSIGNED_SHORT, 0 );

        glDisableClientState( GL_TEXTURE_COORD_ARRAY );
    }
}


static int ledit_select( GWidget* wp, UAtom atom, UCell* res )
{
    if( atom == UR_ATOM_TEXT )
    {
        EX_PTR;
        ur_setId(res, UT_STRING);
        ur_setSeries(res, ep->strN, 0);
        return UR_OK;
    }
    else if( atom == UR_ATOM_ACTION )
    {
        EX_PTR;
        ur_setId(res, UT_BLOCK);
        ur_setSeries(res, ep->codeN, 0);
        return UR_OK;
    }
    return gui_areaSelect( wp, atom, res );
}


GWidgetClass wclass_lineedit =
{
    "line-edit",
    ledit_make,         widget_free,        ledit_mark,
    ledit_dispatch,     ledit_sizeHint,     ledit_layout,
    ledit_render,       ledit_select,
    0, 0
};


// EOF
