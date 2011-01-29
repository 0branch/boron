/*
  Boron OpenGL Module
  Copyright 2005-2010 Karl Robillard

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


#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <GL/glv_keys.h>
#include "os.h"
#include "glh.h"
#include "boron-gl.h"
#include "urlan_atoms.h"
#include "gl_atoms.h"
#include "audio.h"
#include "glid.h"
#include "math3d.h"
#include "draw_prog.h"

#if 0
#include <time.h>
#define PERF_CLOCK  1
#endif


#define gView           glEnv.view

#define colorU8ToF(n)   (((GLfloat) n) / 255.0f)

#define MOUSE_UNSET     -9999


extern UPortDevice port_joystick;
struct GLEnv glEnv;


TexFont* ur_texFontV( UThread* ut, const UCell* cell )
{
    if( ur_is(cell, UT_FONT) )
        return (TexFont*) ur_buffer( ur_fontTF(cell) )->ptr.v;
    return 0;
}


static void eventHandler( GLView* view, GLViewEvent* event )
{
    struct GLEnv* env = (struct GLEnv*) view->user;
    GWidget* wp = env->eventWidget;

    switch( event->type )
    {
        /*
        case GLV_EVENT_RESIZE:
            env->view_wd     = (double) view->width;
            env->view_hd     = (double) view->height;
            env->view_aspect = env->view_wd / env->view_hd;
            break;
        */
        case GLV_EVENT_CLOSE:
            if( ! wp )
            {
                boron_throwWord( env->guiUT, UR_ATOM_QUIT );
                UR_GUI_THROW;   // Ignores any later events.
                return;
            }
            break;

        case GLV_EVENT_FOCUS_IN:
            // Unset prevMouseX to prevent large delta jump.
            env->prevMouseX = MOUSE_UNSET;
            break;
        /*
        case GLV_EVENT_FOCUS_OUT:
            break;
        */
        case GLV_EVENT_MOTION:
            // Set mouse deltas here so no one else needs to calculate them.
            if( env->prevMouseX == MOUSE_UNSET )
            {
                //printf( "KR mouse-move reset\n" );
                env->mouseDeltaX = env->mouseDeltaY = 0;
            }
            else
            {
                env->mouseDeltaX = event->x - env->prevMouseX;
                env->mouseDeltaY = env->prevMouseY - event->y;
            }
            env->prevMouseX = event->x;
            env->prevMouseY = event->y;
            break;
        /*
        case GLV_EVENT_BUTTON_DOWN:
        case GLV_EVENT_BUTTON_UP:
        case GLV_EVENT_WHEEL:
        case GLV_EVENT_KEY_DOWN:
        case GLV_EVENT_KEY_UP:
        */
    }

    if( wp )
        wp->wclass->dispatch( env->guiUT, wp, event );
}


#if 0
void getVector( int count, UCell* val, GLdouble* vec )
{
    UCell* end = val + count;
    while( val != end )
    {
        if( val->type == OT_DECIMAL )
            *vec++ = orDecimal(val);
        else if( val->type == OT_INTEGER )
            *vec++ = (GLdouble) orInt(val);
        else
            *vec++ = 0.0;
        ++val;
    }
}


static GLuint buildTexture( OImage* img, int mipmap )
{
    GLuint id;
    GLenum format;
    GLint  comp;

    switch( img->format )
    {
        case UR_IMG_GRAY:
            format = GL_LUMINANCE;
            comp   = 1;
            //format = GL_LUMINANCE_ALPHA;
            //comp   = GL_LUMINANCE_ALPHA;
            break;

        case UR_IMG_RGB:
            format = GL_RGB;
            comp   = 3;
            break;

        case UR_IMG_RGBA:
            format = GL_RGBA;
            comp   = 4;
            break;

        default:
            return 0;
    }

    glGenTextures( 1, &id );
    glBindTexture( GL_TEXTURE_2D, id );

    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    if( mipmap )
    {
        gluBuild2DMipmaps( GL_TEXTURE_2D, comp, img->width, img->height,
                           format, GL_UNSIGNED_BYTE, img + 1 );
    }
    else
    {
        glTexImage2D( GL_TEXTURE_2D, 0, comp, img->width, img->height, 0,
                      format, GL_UNSIGNED_BYTE, img + 1 );
    }

    glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

    return id;
}


extern void orLoadNative( UCell* a1 );
extern TextureResource* createTexture();

GLuint gxLoadTexture( UCell* val )
{
    TextureResource* tex;
    uint32_t key;
    UCell* result;

    assert( orIsString(val->type) );

    key = gxHashString( val );
    tex = gxTexture( key );
    if( tex )
        return tex->glTexId;

    tex = createTexture();
    if( tex )
    {
        resd_add( &gEnv.resd, &tex->res, key );

        result = orTOS;
        orCopyV( result, *val );
        orLoadNative( result );

        if( result->type == OT_IMAGE )
        {
            OBinary* bin = orSTRING(result);
            if( bin->buf )
            {
                tex->imgHold = orHold( OT_BINARY, orBinaryN(bin) );
                tex->glTexId = buildTexture( (OImage*) bin->buf, 0 );

                return tex->glTexId;
            }
        }
    }
    return 0;
}
#endif


static void pickMode( const GLViewMode* md, void* data )
{
    GLViewMode* smd = (GLViewMode*) data;

    if( (md->width == smd->width) &&
        (md->height == smd->height) &&
        (md->refreshRate >= smd->refreshRate) )
    {
        smd->id = md->id;
        smd->refreshRate = md->refreshRate;
    }
}


/*-cf-
    display
        size    coord!
        /fullscreen
    return: unset!
*/
CFUNC( cfunc_display )
{
#define OPT_DISPLAY_FULLSCREEN  0x01
    GLViewMode mode;
    (void) ut;

    if( gView )
    {
        if( ur_is(a1, UT_COORD) )
        {
            mode.id     = GLV_MODEID_WINDOW;
            mode.width  = a1->coord.n[0];
            mode.height = a1->coord.n[1];

            if( CFUNC_OPTIONS & OPT_DISPLAY_FULLSCREEN )
            {
                mode.refreshRate = 0;
                glv_queryModes( pickMode, &mode );
            }

            glv_changeMode( gView, &mode );
        }
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


#ifdef KR_TODO
// (font text -- coord)
CFUNC( uc_text_size )
{
    uint8_t* cpA;
    uint8_t* cpB;
    TexFont* tf;
    UCell* res = ur_s_prev( tos );

    if( ur_stringSlice( ut, tos, &cpA, &cpB ) )
    {
        tf = ur_texFontV( ut, res );
        if( tf )
        {
            UR_S_DROP;
            ur_setId( res, UT_COORD );
            res->coord.len     = 2;
            res->coord.elem[0] = txf_width( tf, (uint8_t*) cpA,
                                                (uint8_t*) cpB );
            res->coord.elem[1] = txf_lineSpacing( tf );
            return UR_OK;
        }
    }
    return ur_error( ut, UR_ERR_TYPE, "text-size expected font! string!" );
}
#endif


/*-cf-
    handle-events
        widget  none!/widget!
        /wait
    return: unset!
*/
CFUNC( uc_handle_events )
{
    (void) ut;

    glEnv.eventWidget = ur_is(a1, UT_WIDGET) ? ur_widgetPtr(a1) : 0;

    if( CFUNC_OPTIONS & 1 )
        glv_waitEvent( gView );

    glv_handleEvents( gView );
    if( glEnv.guiThrow )
    {
        glEnv.guiThrow = 0;
        // Restore handler removed by UR_GUI_THROW.
        glv_setEventHandler( gView, eventHandler );
        return UR_THROW;
    }

    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    clear-color 
        color       decimal!/coord!/vec3!
    return: unset!
    group: gl
    Calls glClearColor().
*/
CFUNC( uc_clear_color )
{
    (void) ut;
    (void) res;

    if( ur_is(a1, UT_COORD) )
    {
        GLclampf col[4];

        col[0] = colorU8ToF( a1->coord.n[0] );
        col[1] = colorU8ToF( a1->coord.n[1] );
        col[2] = colorU8ToF( a1->coord.n[2] );
        col[3] = (a1->coord.len > 3) ? colorU8ToF(a1->coord.n[3]) : 0.0f;

        glClearColor( col[0], col[1], col[2], col[3] );
    }
    else if( ur_is(a1, UT_VEC3) )
    {
        GLclampf* col = a1->vec3.xyz;
        glClearColor( col[0], col[1], col[2], 0.0f );
    }
    else if( ur_is(a1, UT_DECIMAL) )
    {
        GLclampf col = ur_decimal(a1);
        glClearColor( col, col, col, 0.0f );
    }
    return UR_OK;
}


/*-cf-
    display-swap
    return: unset!
    group: gl
*/
CFUNC( uc_display_swap )
{
    (void) ut;
    (void) a1;
    (void) res;

    glv_swapBuffers( gView );

    /*
    glDepthMask( GL_TRUE );
    glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    */

    return UR_OK;
}


/*-cf-
    display-area
    return: coord! or none
    Get display rectangle (0, 0, width, height).
*/
CFUNC( uc_display_area )
{
    (void) a1;
    (void) ut;
    if( gView )
    {
        ur_setId(res, UT_COORD);
        res->coord.len = 4;
        res->coord.n[0] = 0;
        res->coord.n[1] = 0;
        res->coord.n[2] = gView->width;
        res->coord.n[3] = gView->height;
    }
    else
    {
        ur_setId(res, UT_NONE);
    }
    return UR_OK;
}


/*-cf-
    display-snapshot
    return: raster!
    Create raster from current display pixels.
*/
CFUNC( uc_display_snap )
{
    UBuffer* bin;
    GLint vp[ 4 ];  // x, y, w, h
    (void) a1;

    glGetIntegerv( GL_VIEWPORT, vp );
    bin = ur_makeRaster( ut, UR_RAST_RGB, vp[2], vp[3], res );
    if( bin->ptr.b )
    {
        // Grab front buffer or we are likely to grab a blank screen
        // (since key input is done after glClear).
        glReadBuffer( GL_FRONT );
        glReadPixels( vp[0], vp[1], vp[2], vp[3], GL_RGB, GL_UNSIGNED_BYTE,
                      ur_rastElem(bin) );
    }
    return UR_OK;
}


/*-cf-
    display-cursor
        enable  logic!
    return: unset!
*/
CFUNC( uc_display_cursor )
{
    (void) ut;
    glv_showCursor( gView, ur_int(a1) );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    key-repeat
        enable  logic!
    return: unset!
*/
CFUNC( uc_key_repeat )
{
    (void) ut;
    glv_filterRepeatKeys( gView, ur_int(a1) );
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


static int strnequ( const char* sa, const char* sb, int n )
{
    const char* end = sa + n;
    while( sa != end )
    {
        if( *sa++ != *sb++ )
            return 0;
    }
    return 1;
}


static int _keyCode( const char* cp, int len )
{
    int code = 0;

    if( len == 1 )
    {
        switch( *cp )
        {
            case '0':  code = KEY_0; break;
            case '1':  code = KEY_1; break;
            case '2':  code = KEY_2; break;
            case '3':  code = KEY_3; break;
            case '4':  code = KEY_4; break;
            case '5':  code = KEY_5; break;
            case '6':  code = KEY_6; break;
            case '7':  code = KEY_7; break;
            case '8':  code = KEY_8; break;
            case '9':  code = KEY_9; break;

            case 'a':  code = KEY_a; break;
            case 'b':  code = KEY_b; break;
            case 'c':  code = KEY_c; break;
            case 'd':  code = KEY_d; break;
            case 'e':  code = KEY_e; break;
            case 'f':  code = KEY_f; break;
            case 'g':  code = KEY_g; break;
            case 'h':  code = KEY_h; break;
            case 'i':  code = KEY_i; break;
            case 'j':  code = KEY_j; break;
            case 'k':  code = KEY_k; break;
            case 'l':  code = KEY_l; break;
            case 'm':  code = KEY_m; break;
            case 'n':  code = KEY_n; break;
            case 'o':  code = KEY_o; break;
            case 'p':  code = KEY_p; break;
            case 'q':  code = KEY_q; break;
            case 'r':  code = KEY_r; break;
            case 's':  code = KEY_s; break;
            case 't':  code = KEY_t; break;
            case 'u':  code = KEY_u; break;
            case 'v':  code = KEY_v; break;
            case 'w':  code = KEY_w; break;
            case 'x':  code = KEY_x; break;
            case 'y':  code = KEY_y; break;
            case 'z':  code = KEY_z; break;

            case ',':  code = KEY_Comma;      break;
            case '.':  code = KEY_Period;     break;
            case '/':  code = KEY_Slash;      break;
            case ';':  code = KEY_Semicolon;  break;
            case '\'': code = KEY_Apostrophe; break;
            case '[':  code = KEY_Bracket_L;  break;
            case ']':  code = KEY_Bracket_R;  break;
            case '-':  code = KEY_Minus;      break;
            case '=':  code = KEY_Equal;      break;
            case '\\': code = KEY_Backslash;  break;
            case '`':  code = KEY_Grave;      break;
            case '~':  code = KEY_Grave;      break;
        }
    }
    else if( len == 2 )
    {
        if( strnequ( cp, "up", 2 ) ) code = KEY_Up;
        else if( *cp == 'f' )
        {
            switch( cp[1] )
            {
                case '1': code = KEY_F1; break;
                case '2': code = KEY_F2; break;
                case '3': code = KEY_F3; break;
                case '4': code = KEY_F4; break;
                case '5': code = KEY_F5; break;
                case '6': code = KEY_F6; break;
                case '7': code = KEY_F7; break;
                case '8': code = KEY_F8; break;
                case '9': code = KEY_F9; break;
            }
        }
    }
    else if( len == 3 )
    {
             if( strnequ( cp, "spc", 3 ) ) code = KEY_Space;
        else if( strnequ( cp, "esc", 3 ) ) code = KEY_Escape;
        else if( strnequ( cp, "tab", 3 ) ) code = KEY_Tab;
        else if( strnequ( cp, "end", 3 ) ) code = KEY_End;
        else if( strnequ( cp, "del", 3 ) ) code = KEY_Delete;
        else if( strnequ( cp, "f10", 3 ) ) code = KEY_F10;
        else if( strnequ( cp, "f11", 3 ) ) code = KEY_F11;
        else if( strnequ( cp, "f12", 3 ) ) code = KEY_F12;
    }
    else
    {
             if( strnequ( cp, "down",     4 ) ) code = KEY_Down;
        else if( strnequ( cp, "back",     4 ) ) code = KEY_Back_Space;
        else if( strnequ( cp, "left",     4 ) ) code = KEY_Left;
        else if( strnequ( cp, "right",    5 ) ) code = KEY_Right;
        else if( strnequ( cp, "home",     4 ) ) code = KEY_Home;
        else if( strnequ( cp, "return",   6 ) ) code = KEY_Return;
        else if( strnequ( cp, "insert",   6 ) ) code = KEY_Insert;
        else if( strnequ( cp, "pg-up",    5 ) ) code = KEY_Page_Up;
        else if( strnequ( cp, "pg-down",  7 ) ) code = KEY_Page_Down;
        else if( strnequ( cp, "num-lock", 8 ) ) code = KEY_Num_Lock;
        else if( strnequ( cp, "print",    5 ) ) code = KEY_Print;
        else if( strnequ( cp, "pause",    5 ) ) code = KEY_Pause;
        else if( *cp == 'k' )
        {
             if( strnequ( cp, "kp-8",     4 ) ) code = KEY_KP_Up;
        else if( strnequ( cp, "kp-5",     4 ) ) code = KEY_KP_Begin;
        else if( strnequ( cp, "kp-4",     4 ) ) code = KEY_KP_Left;
        else if( strnequ( cp, "kp-6",     4 ) ) code = KEY_KP_Right;
        else if( strnequ( cp, "kp-7",     4 ) ) code = KEY_KP_Home;
        else if( strnequ( cp, "kp-2",     4 ) ) code = KEY_KP_Down;
        else if( strnequ( cp, "kp-9",     4 ) ) code = KEY_KP_Page_Up;
        else if( strnequ( cp, "kp-3",     4 ) ) code = KEY_KP_Page_Down;
        else if( strnequ( cp, "kp-1",     4 ) ) code = KEY_KP_End;
        else if( strnequ( cp, "kp-0",     4 ) ) code = KEY_KP_Insert;
        else if( strnequ( cp, "kp-ins",   6 ) ) code = KEY_KP_Insert;
        else if( strnequ( cp, "kp-del",   6 ) ) code = KEY_KP_Delete;
        else if( strnequ( cp, "kp-enter", 8 ) ) code = KEY_KP_Enter;
        else if( strnequ( cp, "kp-div",   6 ) ) code = KEY_KP_Divide;
        else if( strnequ( cp, "kp-mul",   6 ) ) code = KEY_KP_Multiply;
        else if( strnequ( cp, "kp-add",   6 ) ) code = KEY_KP_Add;
        else if( strnequ( cp, "kp-sub",   6 ) ) code = KEY_KP_Subtract;
        /*
        else if( strnequ( cp, "kp-sub",   6 ) ) code = KEY_KP_Separator;
        else if( strnequ( cp, "kp-enter", 6 ) ) code = KEY_KP_Decimal;
        else if( strnequ( cp, "kp-enter", 6 ) ) code = KEY_KP_Equal;
        */
        }
    }

    return code;
}


/*-cf-
    key-code
        key  char!/word!
    return: code int!

    Maps key to window system key code.
*/
CFUNC_PUB( uc_key_code )
{
    int code;
    (void) ut;

    if( ur_is(a1, UT_CHAR) )
    {
        char key[2];
        key[0] = ur_int(a1);
        code = _keyCode( key, 1 );
    }
    else if( ur_isWordType( ur_type(a1) ) )
    {
        const char* str = ur_wordCStr( a1 );
        code = _keyCode( str, strLen(str) );
    }
    else
    {
        code = 0;
    }

    ur_setId(res, UT_INT);
    ur_int(res) = code;
    return UR_OK;
}


#if 0
static void _queryModes( const GLViewMode* mode, void* user )
{
    UCell* val;
    OBlock* blk = (OBlock*) user;

    val = orAppendValue( blk, OT_INTEGER, mode->id );
    val->flags |= OR_FLAG_EOL;

    val = orAppendValue( blk, OT_PAIR, mode->width );
    val->pair[1] = mode->height;

    orAppendValue( blk, OT_INTEGER, mode->refreshRate );
    orAppendValue( blk, OT_INTEGER, mode->depth );
}


CFUNC( displayModesNative )
{
    OBlock* blk;
    (void) a1;

    blk = orMakeBlock( 4 * 32 );
    glv_queryModes( _queryModes, blk );
    orResultBLOCK( orBlockN(blk) );
}


CFUNC( clearColorNative )
{
    switch( a1->type )
    {
        case OT_TUPLE:
        {
            GLclampf col[4];

            col[0] = colorU8ToF( a1->tuple[0] );
            col[1] = colorU8ToF( a1->tuple[1] );
            col[2] = colorU8ToF( a1->tuple[2] );
            col[3] = (a1->argc > 3) ? colorU8ToF( a1->tuple[3] ) : 0.0f;

            glClearColor( col[0], col[1], col[2], col[3] );

            gEnv.state.clear = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
        }
            break;

        default:
            gEnv.state.clear = GL_DEPTH_BUFFER_BIT;
            break;
    }
}


#define RELEASE(h)  if( orHoldIsValid(h) ) orRelease(h)

CFUNC( execNative )
{
#ifdef PERF_CLOCK
    clock_t t1, t1e;
#endif
    struct ExecState es;

    es.renderBlkN = 0;
    es.widgetHold = OR_INVALID_HOLD;

    gEnv.widgetCtxN = 0;

    if( a1->type == OT_OBJECT )
    {
        if( ! prepWidgetHandler( &a1->ctx, &es ) )
            return;
    }

    gEnv.running = 1;
    while( 1 )
    {
        glDepthMask( GL_TRUE );
        //glClear( GL_DEPTH_BUFFER_BIT );
        glClear( gEnv.state.clear );

#ifdef PERF_CLOCK
        t1 = clock();
#endif
        glv_handleEvents( gView );
        if( ! gEnv.running )
            break;

        if( es.renderBlkN )
        {
            orEvalBlock( orBLOCKS + es.renderBlkN, 0 );
            if( gxHandleError() )
                break;     // Don't change display if error occurs.
        }

#ifdef PERF_CLOCK
        t1e = clock() - t1;
        printf( "clock %g\n",
                ((double) t1e) / CLOCKS_PER_SEC );
        //printf( "clock %ld\n", t1e );
#endif

        glv_swapBuffers( gView );
    }

    RELEASE( es.widgetHold );

    //orResultUNSET;
}
#endif


/*-cf-
    play
        sound   int!/string!/file!
    return: unset!
    group: audio
*/
CFUNC( uc_play )
{
    if( ur_is(a1, UT_INT) )
    {
        aud_playSound( a1 );
    }
    else if( ur_isStringType( ur_type(a1) ) )
    {
        aud_playMusic( boron_cstr( ut, a1, 0 ) );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    stop
        sound   word!
    return: unset!
    group: audio
*/
CFUNC( uc_stop )
{
    (void) ut;

#if 0
    if( ur_is(a1, UT_INT) )
    {
        aud_stopSound( a1 );
    }
    else
#endif
    if( ur_is(a1, UT_WORD) )
    {
        aud_stopMusic();
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    show
        widget  widget!
    return: unset!
*/
CFUNC( uc_show )
{
    (void) ut;
    if( ur_is(a1, UT_WIDGET) )
    {
#ifdef KR_TODO
        UCell* inv;
        GWidgetId id = ur_int(a1);
        GWidget* wp = gui_widgetPtr( &glEnv.gui, id );
        if( wp )
        {
            gui_show( &glEnv.gui, wp, 1 );

            inv = UR_CALL_CELL;
            if( inv && (ur_sel(inv) == UR_ATOM_FOCUS) )
            {
                gui_setKeyFocus( &glEnv.gui, id );
                gui_setMouseFocus( &glEnv.gui, id );
            }
        }
#endif
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    hide
        widget  widget!
    return: unset!
*/
CFUNC( uc_hide )
{
    (void) ut;
    if( ur_is(a1, UT_WIDGET) )
    {
#ifdef KR_TODO
        GWidget* wp = gui_widgetPtr( &glEnv.gui, ur_int(a1) );
        if( wp )
            gui_show( &glEnv.gui, wp, 0 );
#endif
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


static int _convertUnits( UThread* ut, const UCell* a1, UCell* res,
                          double conv )
{
    double n;
    if( ur_is(a1, UT_DECIMAL) )
        n = ur_decimal(a1);
    else if( ur_is(a1, UT_INT) )
        n = (double) ur_int(a1);
    else
        return ur_error( ut, UR_ERR_TYPE,
                         "Unit conversion expected int!/decimal!");

    ur_setId(res, UT_DECIMAL);
    ur_decimal(res) = n * conv;
    return UR_OK;
}


/*-cf-
    to-degrees
        rad    int!/decimal!
    return: degrees
    group: math
*/
CFUNC( cfunc_to_degrees )
{
    return _convertUnits( ut, a1, res, 180.0/UR_PI );
}


/*-cf-
    to-radians
        deg    int!/decimal!
    return: radians
    group: math
*/
CFUNC( cfunc_to_radians )
{
    return _convertUnits( ut, a1, res, UR_PI/180.0 );
}


/*-cf-
    limit
        number int!/decimal!
        min    int!/decimal!
        max    int!/decimal!
    return: Number clamped to min and max.
    group: math
*/
CFUNC( cfunc_limit )
{
    (void) ut;

    if( ur_is(a1, UT_DECIMAL) )
    {
        double m;
        double n = ur_decimal(a1);

        m = ur_decimal(a1+1);
        if( n < m )
        {
            n = m;
        }
        else
        {
            m = ur_decimal(a1+2);
            if( n > m )
                n = m;
        }

        ur_setId(res, UT_DECIMAL);
        ur_decimal(res) = n;
        return UR_OK;
    }
    else if( ur_is(a1, UT_INT) )
    {
        int m;
        int n = ur_int(a1);

        m = ur_int(a1+1);
        if( n < m )
        {
            n = m;
        }
        else
        {
            m = ur_int(a1+2);
            if( n > m )
                n = m;
        }

        ur_setId(res, UT_INT);
        ur_int(res) = n;
        return UR_OK;
    }

    return ur_error(ut, UR_ERR_TYPE, "limit expected int!/decimal!");
}


/*-cf-
    look-at
        matrix vector!
        dir    vec3!
    return: Transformed matrix.
    group: math
*/
CFUNC( cfunc_look_at )
{
    UCell* a2 = a1 + 1;
    UBuffer* mat;
    float* right;
    float* up;
    float* zv;

    if( ur_is(a1, UT_VECTOR) && ur_is(a2, UT_VEC3) )
    {
        mat = ur_bufferSerM( a1 );
        if( ! mat )
            return UR_THROW;

        right = mat->ptr.f;
        up    = mat->ptr.f + 4;
        zv    = mat->ptr.f + 8;

        zv[0] = zv[4] - a2->vec3.xyz[0];
        zv[1] = zv[5] - a2->vec3.xyz[1];
        zv[2] = zv[6] - a2->vec3.xyz[2];
        ur_normalize( zv );

        up[0] = 0.0;
        up[1] = 1.0;
        up[2] = 0.0;

        ur_cross( up, zv, right );
        ur_normalize( right );

        // Recompute up to make perpendicular to right & zv.
        ur_cross( zv, right, up );
        ur_normalize( up );

        *res = *a1;
        return UR_OK;
    }

    return ur_error( ut, UR_ERR_TYPE, "look-at expected vector! and vec3!" );
}


/*
   res may be v1 or v2.

   \return non-zero if successful.
*/
static int _lerpCells( const UCell* v1, const UCell* v2, double frac,
                       UCell* res )
{
#define INTERP(R,A,B)   R = A + (B - A) * frac;
#define V3(cell,n)      cell->vec3.xyz[n]

    if( ur_type(v1) == ur_type(v2) )
    {
        if( frac < 0.0 )
            frac = 0.0;
        else if( frac > 1.0 )
            frac = 1.0;

        if( ur_is(v1, UT_DECIMAL) )
        {
            ur_setId(res, UT_DECIMAL);
            INTERP( ur_decimal(res), ur_decimal(v1), ur_decimal(v2) );
            return 1;
        }
        else if( ur_is(v1, UT_VEC3) )
        {
            ur_setId(res, UT_VEC3);
            INTERP( V3(res,0), V3(v1,0), V3(v2,0) );
            INTERP( V3(res,1), V3(v1,1), V3(v2,1) );
            INTERP( V3(res,2), V3(v1,2), V3(v2,2) );
            return 1;
        }
        else if( ur_is(v1, UT_COORD) )
        {
            int i;
            int len = v1->coord.len;
            if( v2->coord.len < len )
                len = v2->coord.len;
            for( i = 0; i < len; ++i )
            {
                INTERP( res->coord.n[i], v1->coord.n[i], v2->coord.n[i] );
            }
            ur_setId(res, UT_COORD);
            res->coord.len = len;
            return 1;
        }
    }
    return 0;
}


#define LERP_MSG    "lerp expected two similar decimal!/coord!/vec3! values"

/*-cf-
    lerp
        value1      decimal!/coord!/vec3!
        value2      decimal!/coord!/vec3!
        fraction    decimal!
    return: Interpolated value.
    group: math
*/
CFUNC( cfunc_lerp )
{
    UCell* a2   = a1 + 1;
    UCell* frac = a1 + 2;

    if( ur_is(frac, UT_DECIMAL) )
    {
        if( _lerpCells( a1, a2, ur_decimal(frac), res ) )
            return UR_OK;
        return ur_error( ut, UR_ERR_TYPE, LERP_MSG );
    }
    return ur_error( ut, UR_ERR_TYPE, "lerp expected decimal! fraction" );
}


/*
  cv & res may be the same.

  Return UR_OK/UR_THROW.
*/
static int _curveValue( UThread* ut, const UCell* cv, UCell* res, double t )
{
    UBlockIter bi;
    const UCell* v1;
    int len;
    double t1, d;

    ur_blkSlice( ut, &bi, cv );
    len = bi.end - bi.it;
    if( len & 1 )
    {
        --len;
        --bi.end;
    }
    if( len )
    {
        v1 = 0;
        while( bi.it != bi.end )
        {
            // Assume decimal! for speed.
            if( t <= ur_decimal(bi.it) )
            {
                if( ! v1 )
                {
                    *res = bi.it[1];
                    return UR_OK;
                }

                t1 = ur_decimal(v1);
                d  = ur_decimal(bi.it) - t1;
                if( ! d )
                    goto copy;

                if( _lerpCells( v1 + 1, bi.it + 1, (t - t1) / d, res ) )
                    return UR_OK;
                return ur_error( ut, UR_ERR_TYPE, LERP_MSG );
            }
            v1 = bi.it;
            bi.it += 2;
        }
copy:
        *res = v1[1];
    }
    else
    {
        ur_setId( res, UT_NONE );
    }
    return UR_OK;
}


/*-cf-
    curve-at
        curve   block!
        time    int!/decimal!
    return: Value on curve at time.

    The curve block is a sequence of time and value pairs, ordered by time.
*/
CFUNC( cfunc_curve_at )
{
    UCell* a2 = a1 + 1;
    double t;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "curve-value expected block! curve" );

    if( ur_is(a2, UT_DECIMAL) )
        t = ur_decimal(a2);
    else if( ur_is(a2, UT_INT) )
        t = (double) ur_int(a2);
    else
        return ur_error( ut, UR_ERR_TYPE,
                         "curve-value expected int!/decimal! time" );

    return _curveValue( ut, a1, res, t );
}


enum ContextIndexAnimation
{
    CI_ANIM_VALUE = 0,
    CI_ANIM_CURVE,
    CI_ANIM_PERIOD,
    CI_ANIM_TIME,
    CI_ANIM_BEHAVIOR,
    CI_ANIM_CELLS
};


/*
  Return UR_OK/UR_THROW.
*/
static int _animate( UThread* ut, const UCell* acell, double dt )
{
    double period;
    UCell* behav;
    UCell* curve;
    UCell* value;
    UCell* timec;
    UBuffer* ctx;
    UCell* vc;


    if( ! (ctx = ur_bufferSerM( acell )) )
        return UR_THROW;
    if( ctx->used < CI_ANIM_CELLS )
        return ur_error( ut, UR_ERR_SCRIPT, "Invalid animation context" );
    vc = ctx->ptr.cell;

    behav = vc + CI_ANIM_BEHAVIOR;
    if( ur_is(behav, UT_NONE) )     // Return early if inactive
        return UR_OK;

    curve = vc + CI_ANIM_CURVE;
    if( ! ur_is(curve, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "animation curve must be a block!" );

    value = vc + CI_ANIM_VALUE;
    if( ur_is(value, UT_WORD) )
    {
        if( ! (value = ur_wordCellM( ut, value )) )
            return UR_THROW;
    }
    else if( ur_is(value, UT_BLOCK) )
    {
        value = ur_bufferSer(value)->ptr.cell + value->series.it;
    }

    period = ur_decimal(vc + CI_ANIM_PERIOD);
    timec = vc + CI_ANIM_TIME;

    if( ur_is(behav, UT_INT) )
    {
        dt += ur_decimal(timec);
        if( dt > period )
        {
            int repeat = ur_int(behav) - 1;
            if( repeat < 1 ) 
            {
                ur_setId(behav, UT_NONE);
            }
            else
            {
                dt -= period;
                ur_int(behav) = repeat;
            }
        }
cval:
        ur_decimal(timec) = dt;
        if( (period != 1.0) && (period > 0.0) )
            dt /= period;

        return _curveValue( ut, curve, value, dt );
    }
    else if( ur_is(behav, UT_WORD) )
    {
        switch( ur_atom(behav) )
        {
            case UR_ATOM_LOOP:
                dt += ur_decimal(timec);
                if( dt > period )
                    dt -= period;
                goto cval;

            /*
            case UR_ATOM_PING_PONG:
            case UR_ATOM_PONG:
                break;
            */
        }
    }
    return UR_OK;
}


/*-cf-
    animate
        anims   block!/context!   Animation or block of animations
        time    decimal!          Delta time
    return: unset!
*/
CFUNC( cfunc_animate )
{
    double dt;

    if( ! ur_is(a1 + 1, UT_DECIMAL) )
        return ur_error( ut, UR_ERR_TYPE, "animate expected decimal! time" );
    dt = ur_decimal(a1 + 1);

    if( ur_is(a1, UT_CONTEXT) )
    {
        if( ! _animate( ut, a1, dt ) )
            return UR_THROW;
    }
    else if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIter bi;
        ur_blkSlice( ut, &bi, a1 );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_CONTEXT) )
            {
                if( ! _animate( ut, bi.it, dt ) )
                    return UR_THROW;
            }
        }
    }
    else
    {
        return ur_error( ut, UR_ERR_TYPE, "animate expected block!/context!" );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    blit
        dest    raster!
        src     raster!
        pos     coord!  Desitination position
    return: dest
    group: data
*/
CFUNC( cfunc_blit )
{
    const UCell* src = a1 + 1;
    const UCell* pos = a1 + 2;
    uint16_t* rp;
    uint16_t rect[4];

    if( ur_is(a1,  UT_RASTER) &&
        ur_is(src, UT_RASTER) &&
        ur_is(pos, UT_COORD) )
    {
        if( pos->coord.len > 3 )
        {
            rp = (uint16_t*) pos->coord.n;
        }
        else
        {
            rp = rect;
            rp[0] = pos->coord.n[0];
            rp[1] = pos->coord.n[1];
            rp[2] = rp[3] = 0xffff;
        }

        ur_rasterBlit( ur_rastHead(src), 0, ur_rastHeadM(a1), rp );
        *res = *a1;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "blit expected raster! raster! coord!" );
}


/*-cf-
    move-glyphs
        font    font!
        offset  coord!
    return: Modified font.
    group: data
*/
CFUNC( cfunc_move_glyphs )
{
    TexFont* tf;
    UCell* off = a1 + 1;

    if( ur_is(off, UT_COORD) && (tf = ur_texFontV( ut, a1 )) )
    {
        txf_moveGlyphs( tf, off->coord.n[0], off->coord.n[1] );
        *res = *a1;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "move-glyphs expected font! coord!" );
}


/*-cf-
    point-in
        rect    coord!
        point   coord!
    return: logic!
*/
CFUNC( uc_point_in )
{
    UCell* a2 = a1 + 1;
    if( ur_is(a1, UT_COORD) && ur_is(a2, UT_COORD) )
    {
        int inside;
        int16_t* rect = a1->coord.n;
        int16_t* pnt  = a2->coord.n;
        if( pnt[0] < rect[0] ||
            pnt[1] < rect[1] ||
            pnt[0] > (rect[0] + rect[2]) ||
            pnt[1] > (rect[1] + rect[3]) )
            inside = 0;
        else
            inside = 1;
        ur_setId(res, UT_LOGIC);
        ur_int(res) = inside;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "point-in expected 2 coord! values" );
}


/*-cf-
    change-vbo
        buffer  vbo!
        data    vector!
        length  int!/coord! (stride,offset,npv)
    return: unset!
    group: data
*/
CFUNC( uc_change_vbo )
{
    int stride;
    int offset;
    int copyLen;
    int loops;
    UCell* vec = a1 + 1;
    UCell* len = a1 + 2;

    if( ur_is(len, UT_COORD) )
    {
        stride  = len->coord.n[0];
        offset  = len->coord.n[1];
        copyLen = len->coord.n[2];
    }
    else if( ur_is(len, UT_INT) )
    {
        stride  = 0;
        offset  = 0;
        copyLen = ur_int(len);
    }
    else
        goto bad_dt;

    if( ur_is(vec, UT_VECTOR) && //(ur_vectorDT(vec) == UT_DECIMAL) &&
        ur_is(a1, UT_VBO) )
    {
        USeriesIter si;
        UBuffer* vbo = ur_buffer( ur_vboResN(a1) );
        GLuint* buf = vbo_bufIds(vbo);

        ur_seriesSlice( ut, &si, vec );

        // TODO: Need way to check if buf[0] is an GL_ARRAY_BUFFER.

        if( vbo_count(vbo) && (si.it < si.end) && copyLen )
        {
            GLfloat* dst;
            float* src = si.buf->ptr.f + si.it;

            glBindBuffer( GL_ARRAY_BUFFER, buf[0] );
            dst = (GLfloat*) glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
            if( dst )
            {
                dst += offset;
                if( stride )
                {
                    loops = si.end - si.it;
                    switch( copyLen )
                    {
                        case 1:
                            while( loops-- )
                            {
                                dst[0] = *src++; 
                                dst += stride;
                            }
                            break;

                        case 2:
                            loops /= 2;
                            while( loops-- )
                            {
                                dst[0] = *src++; 
                                dst[1] = *src++; 
                                dst += stride;
                            }
                            break;

                        case 3:
                            loops /= 3;
                            while( loops-- )
                            {
                                dst[0] = *src++; 
                                dst[1] = *src++; 
                                dst[2] = *src++; 
                                dst += stride;
                            }
                            break;

                        default:
                        {
                            GLfloat* it;
                            GLfloat* end;

                            loops /= copyLen;
                            while( loops-- )
                            {
                                it  = dst;
                                end = it + copyLen;
                                while( it != end )
                                    *it++ = *src++; 
                                dst += stride;
                            }
                        }
                            break;
                    }
                }
                else
                {
                    memCpy( dst, src, sizeof(float) * copyLen );
                }

                glUnmapBuffer( GL_ARRAY_BUFFER );
            }
        }
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }

bad_dt:

    return ur_error( ut, UR_ERR_TYPE,
                     "change-vbo expected vbo! vector! int!/coord!" );
}


// Should 'gl-extensions, etc. be replaced with a single 'gl-info call that
// returns a context holding version, extensions, max-textures, etc.?

/*-cf-
    gl-extensions
    return: string!
    group: gl
*/
CFUNC( uc_gl_extensions )
{
    UBuffer* buf = ur_makeStringCell( ut, UR_ENC_LATIN1, 0, res );
    (void) a1;
    ur_strAppendCStr( buf, (char*) glGetString( GL_EXTENSIONS ) );
    return UR_OK;
}


/*-cf-
    gl-max-textures
    return: GL_MAX_TEXTURE_UNITS int!
    group: gl
*/
CFUNC( uc_gl_max_textures )
{
    GLint count;
    (void) ut;
    (void) a1;

    glGetIntegerv( GL_MAX_TEXTURE_UNITS, &count );

    ur_setId(res, UT_INT);
    ur_int(res) = count;
    return UR_OK;
}


#if 0
#if 1
extern float perlinNoise2D( float x, float y, int octaves, float persist );
#define PReal   float
#else
extern double perlinNoise2D( double x, double y, double alpha, double beta,
                             int n );
#define PReal   double
#endif

// (raster octaves persist -- raster)
CFUNC( uc_perlin )
{
    RasterHead* rh;
    int octaves;
    PReal persist;
    UCell* octc = ur_s_prev(tos);
    UCell* rasc = ur_s_prev(octc);

    if( ur_is(rasc, UT_RASTER) &&
        ur_is(octc, UT_INT) &&
        ur_is(tos, UT_DECIMAL) )
    {
        PReal x, y, w, h, n;
        int in;
        uint8_t* ep;

        octaves = ur_int(octc);
        if( octaves < 1 )
            octaves = 1;
        else if( octaves > 4 )
            octaves = 4;

        persist = ur_decimal(tos);
        if( persist < 0.05f )
            persist = 0.05f;
        else if( persist > 1.0f )
            persist = 1.0f;

        rh = ur_rastHead(rasc);
        assert( rh );

        w  = (PReal) rh->width;
        h  = (PReal) rh->height;
        ep = (uint8_t*) (rh + 1);

        switch( rh->format )
        {
            case UR_RAST_GRAY:
                break;

            case UR_RAST_RGB:
                for( y = 0.0f; y < h; y += 1.0f )
                {
                    for( x = 0.0f; x < w; x += 1.0f )
                    {
                        n = perlinNoise2D( x * 0.1, y * 0.1, octaves, persist );
                      //n = perlinNoise2D( x/w, y/h, persist, 2, octaves );

                        //printf( "%g\n", n );
                        in = (int) ((n + 1.0f) * 127.0f);
                        *ep++ = in;
                        *ep++ = in;
                        *ep++ = in;
                    }
                    //printf( "\n" );
                }
                break;

            case UR_RAST_RGBA:
                break;
        }

        
        UR_S_DROPN(2);
        return;
    }

    return ur_error( UR_ERR_TYPE, "perlin expected raster! int! decimal!" );
}
#endif


extern const char* _framebufferStatus();

/*-cf-
    shadowmap
        size    coord!
    return: framebuffer
    group: gl
*/
CFUNC( cfunc_shadowmap )
{
    if( ur_is(a1, UT_COORD) )
    {
        GLuint fboName;
        GLuint texName;
        GLint depthBits;
        const char* err;


        glGetIntegerv( GL_DEPTH_BITS, &depthBits );
        //printf( "KR depthBits %d\n", depthBits );

        texName = glid_genTexture();
        glBindTexture( GL_TEXTURE_2D, texName );
        glTexImage2D( GL_TEXTURE_2D, 0,
                      (depthBits == 16) ? GL_DEPTH_COMPONENT16 :
                                          GL_DEPTH_COMPONENT24,
                      a1->coord.n[0], a1->coord.n[1], 0,
                      GL_DEPTH_COMPONENT,
                      //GL_UNSIGNED_INT,
                      GL_UNSIGNED_BYTE,
                      NULL );

        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                                        GL_COMPARE_R_TO_TEXTURE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC,
                                        GL_LEQUAL );
        //glTexParameteri( GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE,
        //                 GL_INTENSITY );


        fboName = glid_genFramebuffer();
        glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, fboName );
        glFramebufferTexture2DEXT( GL_FRAMEBUFFER_EXT,
                 GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, texName, 0 );
        glDrawBuffer( GL_NONE );
        glReadBuffer( GL_NONE );

        if( (err = _framebufferStatus()) )
            return ur_error( ut, UR_ERR_INTERNAL, err );

        glBindFramebufferEXT( GL_FRAMEBUFFER_EXT, 0 );

        ur_setId(res, UT_FBO);
        ur_fboId(res)    = fboName;
        ur_fboRenId(res) = 0;
        ur_fboTexId(res) = texName;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "shadowmap expected coord!" );
}


/*-cf-
   draw
       dprog    draw-prog!/widget!
   return: unset!
*/
CFUNC( cfunc_draw )
{
    (void) res;
    if( ur_is(a1, UT_DRAWPROG) )
    {
        DPState ds;
        ur_initDrawState( &ds );
        return ur_runDrawProg( ut, &ds, a1->series.buf );
    }
    else if( ur_is(a1, UT_WIDGET) )
    {
        //gui_render( &glEnv.gui, ur_int(tos) );
    }
    return UR_OK;
}


static char _bootScript[] =
#include "boot.c"
    ;

/*-hf- draw-list
        blk
   return: draw-prog!
   Same as make draw-prog! blk.
*/
/*-hf- load-shader
        file
    return: shader!
    group: io
*/
/*-hf- load-texture
        file    PNG filename
        /mipmap
        /clamp
    return: texture!
    group: io
*/
/*-hf- load-wav
        file
    return: Audio-sample context.
    group: audio, io
*/
/*-hf- make-matrix
        pos     vec3!
    return: vector!
    Create matrix with position initialized.
*/


extern CFUNC_PUB( cfunc_buffer_audio );
extern CFUNC_PUB( cfunc_load_png );
extern CFUNC_PUB( cfunc_save_png );
//extern CFUNC_PUB( cfunc_particle_sim );


// Intern commonly used atoms.
static void _createFixedAtoms( UThread* ut )
{
#define FA_COUNT    61
    UAtom atoms[ FA_COUNT ];

    ur_internAtoms( ut,
        "add size text wait close\n"
        "width height area rect raster texture\n"
        "gui-style elem focus resize key-down key-up\n"
        "mouse-move mouse-up mouse-down mouse-wheel\n"
        "ambient diffuse specular pos shader vertex normal fragment\n"
        "default dynamic static stream left right center\n"
        "rgb rgba depth clamp repeat nearest linear\n"
        "min mag mipmap gray\n"
        "burn color trans sprite\n"
        "once ping-pong pong\n"
        "collide fall integrate attach anchor binary action",
        atoms );

#ifdef DEBUG
    if( atoms[0] != UR_ATOM_ADD || atoms[5] != UR_ATOM_WIDTH ||
        atoms[FA_COUNT - 1] != UR_ATOM_ACTION )
    {
        int i;
        for( i = 0; i < FA_COUNT; ++i )
            printf( "KR %d %s\n", atoms[i], ur_atomCStr(ut, atoms[i]) );
    }
#endif

    assert( atoms[0] == UR_ATOM_ADD );
    assert( atoms[1] == UR_ATOM_SIZE );
    assert( atoms[2] == UR_ATOM_TEXT );
    assert( atoms[3] == UR_ATOM_WAIT );
    assert( atoms[4] == UR_ATOM_CLOSE );
    assert( atoms[5] == UR_ATOM_WIDTH );
    assert( atoms[FA_COUNT - 1] == UR_ATOM_ACTION );
}


static void _createDrawOpTable( UThread* ut )
{
#define DA_COUNT    53
    UAtom atoms[ DA_COUNT ];
    UAtomEntry* ent;
    UBuffer* buf;
    UIndex bufN;
    int i;

    ur_internAtoms( ut,
        "nop end clear enable disable\n"
        "call solid model decal image\n"
        "particle color colors verts normals\n"
        "uvs attrib points lines line-strip\n"
        "tris tri-strip tri-fan quads quad_strip\n"
        "sphere box quad camera light\n"
        "lighting push pop translate rotate\n"
        "scale font text shader uniform\n"
        "framebuffer shadow-begin shadow-end samples-query samples-begin\n"
        "buffer depth-test blend cull color-mask\n"
        "depth-mask point-size point-sprite",
        atoms );

    bufN = ur_makeBinary( ut, DA_COUNT * sizeof(UAtomEntry) );
    ur_hold( bufN );

    buf = ur_buffer( bufN );
    ent = ur_ptr(UAtomEntry, buf);
    for( i = 0; i < DA_COUNT; ++i )
    {
        ent[i].atom = atoms[i];
        ent[i].index = i;
    }
    ur_atomsSort( ent, 0, DA_COUNT - 1 );

    glEnv.drawOpTable = ent;
}


#include "gl_types.c"
#include "math3d.c"


extern GWidgetClass wclass_script;
extern GWidgetClass wclass_hbox;
extern GWidgetClass wclass_vbox;
extern GWidgetClass wclass_window;
extern GWidgetClass wclass_button;
extern GWidgetClass wclass_checkbox;


UThread* boron_makeEnvGL( UDatatype** dtTable, unsigned int dtCount )
{
#ifdef __linux__
    static char joyStr[] = "joystick";
#endif
    UThread* ut;
    const GLubyte* gstr;

    {
    UDatatype* table[ UT_MAX - UT_GL_COUNT ];
    unsigned int i;

    for( i = 0; i < (sizeof(gl_types) / sizeof(UDatatype)); ++i )
        table[i] = gl_types + i;
    for( dtCount += i; i < dtCount; ++i )
        table[i] = *dtTable++;

    ut = boron_makeEnv( table, dtCount );
    }
    if( ! ut )
        return 0;

#if 0
    printf( "sizeof(UCellRasterFont) %ld\n", sizeof(UCellRasterFont) );
    printf( "sizeof(GLuint) %ld\n", sizeof(GLuint) );
#endif
    assert( sizeof(GLuint) == 4 );

    glEnv.maxTextureUnits = 0;
    glEnv.view = 0;
    glEnv.guiUT = ut;
    glEnv.prevMouseX = MOUSE_UNSET;
    glEnv.prevMouseY = MOUSE_UNSET;
    glEnv.guiThrow = 0;

    _createFixedAtoms( ut );
    _createDrawOpTable( ut );


#define addCFunc(func,spec)    boron_addCFunc(ut, func, spec)

    addCFunc( cfunc_draw,        "draw prog" );
    addCFunc( uc_play,           "play n" );
    addCFunc( uc_stop,           "stop n" );
    addCFunc( uc_show,           "show wid" );
    addCFunc( uc_hide,           "hide wid" );
    //addCFunc( uc_text_size,      "text-size" );
    addCFunc( uc_handle_events,  "handle-events wid /wait" );
    addCFunc( uc_clear_color,    "clear-color color" );
    addCFunc( uc_display_swap,   "display-swap" );
    addCFunc( uc_display_area,   "display-area" );
    addCFunc( uc_display_snap,   "display-snapshot" );
    addCFunc( uc_display_cursor, "display-cursor" );
    addCFunc( uc_key_repeat,     "key-repeat" );
    addCFunc( uc_key_code,       "key-code" );
    addCFunc( cfunc_load_png,    "load-png f" );
    addCFunc( cfunc_save_png,    "save-png f rast" );
    addCFunc( cfunc_buffer_audio,"buffer-audio a" );
    addCFunc( cfunc_display,     "display size /fullscreen" );
    addCFunc( cfunc_to_degrees,  "to-degrees n" );
    addCFunc( cfunc_to_radians,  "to-radians n" );
    addCFunc( cfunc_limit,       "limit n min max" );
    addCFunc( cfunc_look_at,     "look-at a b" );
    addCFunc( cfunc_lerp,        "lerp a b f" );
    addCFunc( cfunc_curve_at,    "curve-at a b" );
    addCFunc( cfunc_animate,     "animate a time" );
    /*
#ifdef TIMER_GROUP
    addCFunc( uc_timer_group,    "timer-group" );
    addCFunc( uc_add_timer,      "add-timer" );
    addCFunc( uc_check_timers,   "check-timers" );
#endif
    */
    addCFunc( cfunc_blit,        "blit a b pos" );
    addCFunc( cfunc_move_glyphs, "move-glyphs f pos" );
    addCFunc( uc_point_in,       "point-in" );
    addCFunc( uc_change_vbo,     "change-vbo a b n" );
    addCFunc( uc_gl_extensions,  "gl-extensions" );
    addCFunc( uc_gl_max_textures,"gl-max-textures" );
    /*
    addCFunc( cfunc_particle_sim,"particle-sim vec prog n" );
    //addCFunc( uc_perlin,         "perlin" );
    */
    addCFunc( cfunc_shadowmap,   "shadowmap size" );
    addCFunc( cfunc_dot,         "dot a b" );
    addCFunc( cfunc_cross,       "cross a b" );
    addCFunc( cfunc_normalize,   "normalize vec" );
    addCFunc( cfunc_project_point, "project-point a b pnt" );


    if( ! boron_doCStr( ut, _bootScript, sizeof(_bootScript) - 1 ) )
        return UR_THROW;

#ifdef __linux__
    boron_addPortDevice( ut, &port_joystick,
                         ur_internAtom(ut, joyStr, joyStr + 8) );
#endif

#ifndef NO_AUDIO
    {
    char* var = getenv( "BORON_GL_AUDIO" );
    if( ! var || (var[0] != '0') )
        aud_startup();
    }
#endif

    gView = glv_create( GLV_ATTRIB_DOUBLEBUFFER | GLV_ATTRIB_MULTISAMPLE );
    if( ! gView )
    {
        fprintf( stderr, "glv_create() failed\n" );
cleanup:
        boron_freeEnv( ut );
        return 0;
    }

    gstr = glGetString( GL_VERSION );
    if( gstr[0] < '2' )
    {
        glv_destroy( gView );
        gView = 0;
        fprintf( stderr, "OpenGL 2.0 required\n" );
        goto cleanup;
    }

    glGetIntegerv( GL_MAX_TEXTURE_UNITS, &glEnv.maxTextureUnits );

    gView->user = &glEnv;
    glv_setTitle( gView, "Boron-GL" );
    glv_setEventHandler( gView, eventHandler );
    //glv_showCursor( gView, 0 );

    glid_startup();
    //gui_init( &glEnv.gui, ut );

    ur_strInit( &glEnv.tmpStr, UR_ENC_LATIN1, 0 );
    ur_ctxInit( &glEnv.widgetClasses, 0 );
    ur_arrInit( &glEnv.rootWidgets, sizeof(GWidget*), 0 );

    {
    GWidgetClass* classes[ 6 ];

    classes[0] = &wclass_script;
    classes[1] = &wclass_hbox;
    classes[2] = &wclass_vbox;
    classes[3] = &wclass_window;
    classes[4] = &wclass_button;
    classes[5] = &wclass_checkbox;

    ur_addWidgetClasses( classes, 6 );
    }

    return ut;
}


void boron_freeEnvGL( UThread* ut )
{
    if( ut )
    {
        ur_strFree( &glEnv.tmpStr );
        ur_ctxFree( &glEnv.widgetClasses );
        {
        GWidget** it  = (GWidget**) glEnv.rootWidgets.ptr.v;
        GWidget** end = it + glEnv.rootWidgets.used;
        while( it != end )
            gui_freeWidget( *it++ );
        ur_arrFree( &glEnv.rootWidgets );
        }

        //gui_cleanup( &glEnv.gui );
        glid_shutdown();
#ifndef NO_AUDIO
        aud_shutdown();
#endif
        glv_destroy( gView );
        boron_freeEnv( ut );
    }
}


void ur_addWidgetClasses( GWidgetClass** classTable, int count )
{
    UBuffer* ctx = &glEnv.widgetClasses;
    UBuffer atoms;
    int i;
    int index;

    {
    UBuffer* str = &glEnv.tmpStr;
    str->used = 0;
    ur_arrInit( &atoms, sizeof(UAtom), count );
    for( i = 0; i < count; ++i )
    {
        if( i )
            ur_strAppendChar( str, ' ' );
        ur_strAppendCStr( str, classTable[ i ]->name );
    }
    ur_strTermNull( str );
    ur_internAtoms( glEnv.guiUT, str->ptr.c, atoms.ptr.u16 );
    }

    ur_ctxReserve( ctx, ctx->used + count );
    for( i = 0; i < count; ++i )
    {
        GWidgetClass* wc = *classTable++;
        wc->nameAtom = atoms.ptr.u16[ i ];
        index = ur_ctxAddWordI( ctx, wc->nameAtom );
        ((GWidgetClass**) ctx->ptr.v)[ index ] = wc;
    }
    ur_ctxSort( ctx );

    ur_arrFree( &atoms );
}


GWidgetClass* ur_widgetClass( UAtom name )
{
    int i = ur_ctxLookup( &glEnv.widgetClasses, name );
    if( i < 0 )
        return 0;
    return ((GWidgetClass**) glEnv.widgetClasses.ptr.v)[ i ];
}


// EOF
