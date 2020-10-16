/*
  Boron OpenGL GUI
  Copyright 2011-2012 Karl Robillard

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
#include <glv_keys.h>
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
    GLuint   vao;
    uint16_t maxChars;
    uint16_t drawn;
    uint16_t dpSwitch;
    uint16_t editInit;
    uint16_t editPos;
    uint16_t editEnd;       // Selected text.
    uint16_t newCursorX;
    uint8_t  state;
}
LineEdit;

#define EX_PTR  LineEdit* ep = (LineEdit*) wp


#define _bitIsSet(mem,n)    (mem[(n)>>3] & 1<<((n)&7))

#define LEDIT_APV           5
#define LEDIT_ATTR_SIZE     (LEDIT_APV * sizeof(GLfloat))
#define LEDIT_VERT_IN_BUF(max)  (LEDIT_APV * 4 * (max + 1))

#define MARGIN_L    8
#define MARGIN_B    6

#define flagged(wf)   (wp->flags & wf)
#define setFlag(wf)   wp->flags |= wf
#define clrFlag(wf)   wp->flags &= ~wf

#define CHANGED         GW_FLAG_USER1
#define NEW_CURSORX     GW_FLAG_USER2
#define CURSOR_MOVED    GW_FLAG_USER3


static void ledit_vbo( UThread* ut, LineEdit* ep )
{
    UBuffer* buf;
    GLuint* gbuf;
    uint16_t* dst;
    int maxChars = ep->maxChars;
    int indexCount = 6 * (maxChars + 1);

    glGenVertexArrays( 1, &ep->vao );
    glBindVertexArray( ep->vao );

    ep->vboN = ur_makeVbo( ut, GL_DYNAMIC_DRAW,
                           LEDIT_VERT_IN_BUF(maxChars), NULL,
                           indexCount, NULL );
    buf = ur_buffer( ep->vboN );
    gbuf = vbo_bufIds(buf);

    glBindBuffer( GL_ARRAY_BUFFER, gbuf[0] );
    glEnableVertexAttribArray( ALOC_VERTEX );
    glEnableVertexAttribArray( ALOC_TEXTURE );
    glVertexAttribPointer( ALOC_VERTEX, 3, GL_FLOAT, GL_FALSE,
                           LEDIT_ATTR_SIZE, NULL + 8 );
    glVertexAttribPointer( ALOC_TEXTURE, 2, GL_FLOAT, GL_FALSE,
                           LEDIT_ATTR_SIZE, NULL + 0 );

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, gbuf[1] );
    dst = (uint16_t*) glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0,
                          sizeof(uint16_t) * indexCount,
                          GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );
    if( dst )
    {
        vbo_initTextIndices( dst, 0, maxChars + 1 );
        glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );
    }
}


/*-wid-
    line-edit   text    [max-length]
                string! int!
*/
static const uint8_t ledit_args[] =
{
    GUIA_ARGW, 1, UT_STRING,
    GUIA_OPT,     UT_INT,
    GUIA_END
};

/*
    line-edit word!/string! <max-chars>
*/
static GWidget* ledit_make( UThread* ut, UBlockIter* bi,
                            const GWidgetClass* wclass, GWidget* parent )
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

    ep = (LineEdit*) gui_allocWidget( sizeof(LineEdit), wclass, parent );
    ep->wid.flags |= CHANGED;

    //ep->state = LEDIT_STATE_DISPLAY;
    ep->strN     = arg[0]->series.buf;
    ep->maxChars = maxChars;
    ledit_vbo( ut, ep );            // gc!

    return (GWidget*) ep;
}


void ledit_free( GWidget* wp )
{
    EX_PTR;
    glDeleteVertexArrays( 1, &ep->vao );
}


static void ledit_mark( UThread* ut, GWidget* wp )
{
    EX_PTR;

    ur_markBuffer( ut, ep->strN );
    ur_markBuffer( ut, ep->filterN );
    if( ep->vboN )
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
        // TODO: Limit data to ep->maxChars.
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


static void ledit_setState( UThread* ut, LineEdit* ep, int state )
{
    if( ep->state != state )
    {
        ep->state = state;
        if( ep->dpSwitch )
        {
            UIndex resN = gui_parentDrawProg( &ep->wid );
            if( resN != UR_INVALID_BUF )
                ur_setDPSwitch( ut, resN, ep->dpSwitch, state );
        }
    }
}


static void ledit_dispatch( UThread* ut, GWidget* wp, const GLViewEvent* ev )
{
    EX_PTR;
    int eraseLen;

    switch( ev->type )
    {
        case GLV_EVENT_BUTTON_DOWN:
            if( (ev->code == GLV_BUTTON_LEFT) ||
                (ev->code == GLV_BUTTON_MIDDLE) )
            {
                ledit_setState( ut, ep, LEDIT_STATE_EDIT );

                if( ev->code == GLV_BUTTON_LEFT )
                {
                    // Don't have access to font here so just notify render.
                    ep->newCursorX = ev->x;
                    setFlag( NEW_CURSORX );
                    gui_grabMouse( wp, 1 );
                }
                else // if( ev->code == GLV_BUTTON_MIDDLE )
                {
                    ledit_paste( ut, ep );
                    setFlag( CHANGED );
                    gui_setKeyFocus( wp );
                }
            }
            break;

        case GLV_EVENT_BUTTON_UP:
            if( ev->code == GLV_BUTTON_LEFT )
                gui_ungrabMouse( wp );
            break;

        case GLV_EVENT_MOTION:
            if( ev->state & GLV_MASK_LEFT )
            {
                // We don't write over newCursorX if NEW_CURSORX hasn't been
                // handled in ledit_render yet.
                if( ! flagged(NEW_CURSORX) )
                {
                    ep->newCursorX = ev->x;
                    setFlag( CURSOR_MOVED );
                }
            }
            break;

        case GLV_EVENT_KEY_DOWN:
            if( ep->strN )
            {
                UBuffer* str = ur_buffer( ep->strN );

                if( ev->state & GLV_MASK_CTRL )
                {
                    if( ev->code == KEY_a )
                    {
                        goto key_home;
                    }
                    else if( ev->code == KEY_k )
                    {
                        // Remove from cursor to end (from Bash).
                        eraseLen = str->used - ep->editPos;
                        if( eraseLen > 0 )
                            goto key_erase;
                    }
                    else if( ev->code == KEY_v )
                    {
                        ledit_paste( ut, ep );
                        goto key_pos;
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
                            goto key_pos;
                        }
                        break;

                    case KEY_Right:
                        if( ep->editPos < str->used )
                        {
                            ++ep->editPos;
                            goto key_pos;
                        }
                        break;

                    case KEY_Home:
key_home:
                        ep->editPos = 0;
key_pos:
                        ep->editEnd = ep->editPos;
                        setFlag( CHANGED );
                        break;

                    case KEY_End:
                        ep->editPos = str->used;
                        goto key_pos;

                    case KEY_Insert:
                        break;

                    case KEY_Delete:
                        if( ep->editEnd > ep->editPos )
                        {
                            eraseLen = ep->editEnd - ep->editPos;
                            goto key_erase;
                        }
                        else if( ep->editPos < str->used )
                        {
key_erase1:
                            eraseLen = 1;
key_erase:
                            ur_arrErase( str, ep->editPos, eraseLen );
                            goto key_pos;
                        }
                        break;

                    case KEY_Back_Space:
                        if( ep->editEnd > ep->editPos )
                        {
                            eraseLen = ep->editEnd - ep->editPos;
                            goto key_erase;
                        }
                        else if( ep->editPos > 0 )
                        {
                            --ep->editPos;
                            goto key_erase1;
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
                                goto key_pos;
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
#ifdef __ANDROID__
            glv_showSoftInput( glEnv.view, 1 );
#endif
            break;

        case GLV_EVENT_FOCUS_OUT:
#ifdef __ANDROID__
            glv_showSoftInput( glEnv.view, 0 );
#endif
            ledit_setState( ut, ep, LEDIT_STATE_DISPLAY );
            break;

        case GUI_EVENT_WINDOW_CREATED:
            setFlag( CHANGED | NEW_CURSORX );
            ledit_vbo( ut, ep );
            break;

        case GUI_EVENT_WINDOW_DESTROYED:
            ep->vboN = 0;
            ep->vao = 0;
            break;
    }
    return;

activate:

    if( ep->codeN )
    {
        ur_initSeries(gui_value(ut), UT_STRING, ep->strN);
        gui_doBlockN( ut, ep->codeN );
    }
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
    size->weightX = GW_WEIGHT_STD;
    size->weightY = GW_WEIGHT_FIXED;
    size->policyX = GW_POL_EXPANDING;
    size->policyY = GW_POL_FIXED;

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


void dp_tgeoInit( DPCompiler* );

static void ledit_layout( GWidget* wp )
{
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    UCell* rc;
    EX_PTR;


    // Set draw list variables.

    rc = style + CI_STYLE_LABEL;
    ur_initSeries( rc, UT_STRING, ep->strN );

    rc = style + CI_STYLE_AREA;
    gui_initRectCoord( rc, wp, UR_ATOM_RECT );


    // Compile draw lists.

    if( ! gDPC )
        return;

    // Make sure the tgeo vertex buffers are bound before the switch.
    // Otherwise only the first case would emit the code to do it. 
    // NOTE: This assumes the button draw programs actually use tgeo.
    dp_tgeoInit( gDPC );

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
        ur_compileDP( ut, rc, 1 );
        gx_endDP( ur_buffer( ep->textResN ), save );

        //dp_compile( &dc, ut, rc->series.n );
    }
#endif
}


static int ledit_charPos( GWidget* wp, const TexFont* tf, const UBuffer* str,
                          int px )
{
    int pos = txf_charAtPixel( tf, str->ptr.b, str->ptr.b + str->used,
                               px - wp->area.x - MARGIN_L );
    if( pos < 0 )
        pos = (px > wp->area.x + MARGIN_L) ? str->used : 0;
    return pos;
}


static void ledit_setCursorVert( LineEdit* ep, const uint8_t* cp, TexFont* tf,
                                 float* attr, float colorIndex )
{
    const float U1 = 1.0f / 512.0f;
    int x = ep->wid.area.x + MARGIN_L;
    int y = ep->wid.area.y + 2;
    float left;
    float right;
    float bottom = (float) y;
    float top = (float) (y + txf_lineSpacing(tf));

    left = (float) (x + txf_width(tf, cp, cp + ep->editPos) - 1);
    if( ep->editEnd > ep->editPos )
        right = (float) (x + txf_width(tf, cp, cp + ep->editEnd));
    else
        right = left + 0.9f;

    *attr++ = U1;           // Tex U
    *attr++ = 0.0f;         // Tex V
    *attr++ = left;         // Pos X
    *attr++ = bottom;       // Pos Y
    *attr++ = colorIndex;   // Pos Z

    *attr++ = U1;
    *attr++ = 0.0f;
    *attr++ = left;
    *attr++ = top;
    *attr++ = colorIndex;

    *attr++ = U1;
    *attr++ = 0.0f;
    *attr++ = right;
    *attr++ = top;
    *attr++ = colorIndex;

    *attr++ = U1;
    *attr++ = 0.0f;
    *attr++ = right;
    *attr++ = bottom;
    *attr   = colorIndex;
}


extern void gr_drawText( TexFont* tf, const char* it, const char* end );

static void ledit_render( GWidget* wp )
{
    DrawTextState ds;
    GLfloat* fdata;
    UBuffer* str;
    TexFont* tf;
    UCell* style = glEnv.guiStyle;
    UThread* ut  = glEnv.guiUT;
    GLint saveVao;
    int pos;
    EX_PTR;

    if( ! ep->strN )
        return;

    tf = ur_texFontV( ut, style + CI_STYLE_EDIT_FONT );
    if( tf )
    {
        str = ur_buffer( ep->strN );

        if( flagged( NEW_CURSORX ) )
        {
            clrFlag( NEW_CURSORX );
            pos = ledit_charPos( wp, tf, str, ep->newCursorX );
            if( pos != ep->editPos )
            {
                ep->editInit = ep->editPos = ep->editEnd = pos;
                setFlag( CHANGED );
            }
        }
        else if( flagged( CURSOR_MOVED ) )
        {
            clrFlag( CURSOR_MOVED );
            pos = ledit_charPos( wp, tf, str, ep->newCursorX );
            if( pos <= ep->editInit && pos != ep->editPos )
            {
                ep->editPos = pos;
                ep->editEnd = ep->editInit;
                setFlag( CHANGED );
            }
            else if( pos >= ep->editInit && pos != ep->editEnd )
            {
                ep->editPos = ep->editInit;
                ep->editEnd = pos;
                setFlag( CHANGED );
            }
        }

        glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &saveVao );
        glBindVertexArray( ep->vao );

        if( flagged( CHANGED ) )
        {
            GLuint* gbuf = vbo_bufIds( ur_buffer(ep->vboN) );
            glBindBuffer( GL_ARRAY_BUFFER, gbuf[0] );

            //printf( "KR pos %d,%d\n", ep->editPos, ep->editEnd );
            clrFlag( CHANGED );

            fdata = (float*) glMapBufferRange( GL_ARRAY_BUFFER, 0,
                            sizeof(float) * LEDIT_VERT_IN_BUF(ep->maxChars),
                            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT );
            if( fdata )
            {
                ledit_setCursorVert(ep, str->ptr.b, tf, fdata,
                                    ep->editPos != ep->editEnd ? 50.0f : 0.0f);
                fdata += LEDIT_APV * 4;

                vbo_drawTextInit( &ds, tf, wp->area.x + MARGIN_L,
                                           wp->area.y + MARGIN_B );

                ep->drawn = 6 * vbo_drawText( &ds, fdata, fdata + 2, LEDIT_APV,
                                          str->ptr.b, str->ptr.b + str->used );
                glUnmapBuffer( GL_ARRAY_BUFFER );
            }
        }

        // Set red -2.0 for shader CLUT mode.
        glUniform4f( ULOC_COLOR, -2.0, 0.0, 0.0, 1.0 );

        {
        char* indices = NULL;
        GLsizei count = ep->drawn;
        if( gui_hasFocus( wp ) & GW_FOCUS_KEY )
            count += 6;
        else
            indices += 6 * sizeof(uint16_t);
        glDrawElements( GL_TRIANGLES, count, GL_UNSIGNED_SHORT, indices );
        }

        glBindVertexArray( saveVao );
    }
}


static int ledit_select( GWidget* wp, UAtom atom, UCell* res )
{
    if( atom == UR_ATOM_TEXT )
    {
        EX_PTR;
        ur_initSeries(res, UT_STRING, ep->strN);
        return UR_OK;
    }
    else if( atom == UR_ATOM_ACTION )
    {
        EX_PTR;
        ur_initSeries(res, UT_BLOCK, ep->codeN);
        return UR_OK;
    }
    return gui_areaSelect( wp, atom, res );
}


static UStatus ledit_set( UThread* ut, GWidget* wp, const UCell* val )
{
    EX_PTR;
    UBuffer* str = ur_buffer( ep->strN );
    str->used = 0;
    ur_toText( ut, val, str );
    if( str->used > ep->maxChars )
        str->used = ep->maxChars;
    setFlag( CHANGED );
    return UR_OK;
}


GWidgetClass wclass_lineedit =
{
    "line-edit",
    ledit_make,         ledit_free,         ledit_mark,
    ledit_dispatch,     ledit_sizeHint,     ledit_layout,
    ledit_render,       ledit_select,       ledit_set,
    0, 0
};


// EOF
