/*
  Boron OpenGL Module
  Copyright 2005-2013 Karl Robillard

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
#include <glv_keys.h>
#include "os.h"
#include "glh.h"
#include "boron-gl.h"
#include "urlan_atoms.h"
#include "gl_atoms.h"
#include "audio.h"
#include "glid.h"
#include "math3d.h"
#include "draw_prog.h"
#include "quat.h"

#ifdef __ANDROID__
#include "glv_activity.h"
#endif

#if 0
#include <time.h>
#define PERF_CLOCK  1
#endif


#define gView           glEnv.view

#define colorU8ToF(n)   (((GLfloat) n) / 255.0f)

#define MOUSE_UNSET     -9999


extern UPortDevice port_joystick;
struct GLEnv glEnv;


UCell* ur_ctxValueOfType( const UBuffer* ctx, UAtom name, int type )
{
    int n = ur_ctxLookup( ctx, name );
    if( n > -1 )
    {
        UCell* value = ur_ctxCell( ctx, n );
        if( ur_is(value, type) )
            return value;
    }
    return NULL;
}


TexFont* ur_texFontV( UThread* ut, const UCell* cell )
{
    if( ur_is(cell, UT_FONT) )
        return (TexFont*) ur_buffer( ur_fontTF(cell) )->ptr.v;
    return 0;
}


#if defined(DEBUG) && defined(GL_DEBUG_OUTPUT)
#define DEBUG_GL
#endif
#ifdef DEBUG_GL
void _debugGL( GLenum source, GLenum type, GLuint id, GLenum severity,
               GLsizei length, const GLchar* message, const void* userParam )
{
    (void) severity;
    (void) length;
    (void) userParam;

    fprintf( stderr, "GL DEBUG %d:%s 0x%x %s\n",
             source,
             (type == GL_DEBUG_TYPE_ERROR) ? " ERROR" : "",
             id, message );
}


static void enableGLDebug()
{
    // Requires GL_KHR_debug extension
    glEnable( GL_DEBUG_OUTPUT );
    glEnable( GL_DEBUG_OUTPUT_SYNCHRONOUS );
    glDebugMessageControl( GL_DEBUG_SOURCE_API, GL_DONT_CARE,
                           GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE );
    glDebugMessageCallback( _debugGL, NULL );
}
#endif


//#define SIMULATE_APP_RESTART

#if defined(__ANDROID__) || defined(SIMULATE_APP_RESTART)
static void doAppEventHandler( UThread* ut, UAtom handlerName )
{
    UCell* cell;
    cell = ur_ctxValueOfType( ur_threadContext(ut), handlerName, UT_BLOCK );
    if( cell )
        gui_doBlock( ut, cell );    // Does UR_GUI_THROW if needed.
}
#endif


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
                boron_throwWord( env->guiUT, UR_ATOM_QUIT, 0 );
                goto gui_throw;
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
        case GLV_EVENT_BUTTON_DOWN:
        case GLV_EVENT_BUTTON_UP:
            // Convert window system origin from top of window to the bottom.
            event->y = view->height - event->y;
            break;

        case GLV_EVENT_MOTION:
            // Convert window system origin from top of window to the bottom.
            event->y = view->height - event->y;

            // Set mouse deltas here so no one else needs to calculate them.
            if( env->prevMouseX == MOUSE_UNSET )
            {
                //printf( "KR mouse-move reset\n" );
                env->mouseDeltaX = env->mouseDeltaY = 0;
            }
            else
            {
                env->mouseDeltaX = event->x - env->prevMouseX;
                env->mouseDeltaY = event->y - env->prevMouseY;
            }
            env->prevMouseX = event->x;
            env->prevMouseY = event->y;
            break;
        /*
        case GLV_EVENT_WHEEL:
        case GLV_EVENT_KEY_DOWN:
        case GLV_EVENT_KEY_UP:
        */

#ifdef SIMULATE_APP_RESTART
        case GLV_EVENT_KEY_DOWN:
            if( event->code == KEY_Pause )
            {
                glv_setEventHandler(gView, 0);
                glEnv.guiThrow = 22;
                return;
            }
            break;
#endif

#ifdef __ANDROID__
        case GLV_EVENT_APP:
            fprintf( stderr, "GLV_EVENT_APP %d\n", event->code );
            switch( event->code )
            {
            case APP_CMD_INIT_WINDOW:
                gui_signalWindowCreated( view->width, view->height );
                doAppEventHandler( env->guiUT, UR_ATOM_ON_WINDOW_CREATED );
                aud_pauseProcessing( 0 );
                return;

            case APP_CMD_TERM_WINDOW:
                doAppEventHandler( env->guiUT, UR_ATOM_ON_WINDOW_DESTROYED );
                boron_releaseGL( env->guiUT );
                aud_pauseProcessing( 1 );
                return;

            case APP_CMD_SAVE_STATE:
                doAppEventHandler( env->guiUT, UR_ATOM_ON_SAVE_INSTANCE );
                return;
            /*
            case APP_CMD_DESTROY:
                boron_throwWord( env->guiUT, UR_ATOM_QUIT, 0 );
                goto gui_throw;
            */
            }
            break;
#endif
    }

    if( wp )
        wp->wclass->dispatch( env->guiUT, wp, event );
    return;

gui_throw:
    UR_GUI_THROW;   // Ignores any later events.
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
        size        coord!
        /fullscreen
        /title
            text    string!
    return: unset!
*/
CFUNC( cfunc_display )
{
#ifndef __ANDROID__
#define OPT_DISPLAY_FULLSCREEN  0x01
#define OPT_DISPLAY_TITLE       0x02
    GLViewMode mode;

    if( gView )
    {
        if( (CFUNC_OPTIONS & OPT_DISPLAY_TITLE) && ur_is(a1+1, UT_STRING) )
        {
            glv_setTitle( gView, boron_cstr( ut, a1+1, 0 ) );
        }

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
#endif
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


static UBuffer* _userDataDirectory( UThread* ut, UCell* res )
{
#ifndef __ANDROID__
    char* path;
#endif
    UBuffer* str = ur_makeStringCell( ut, UR_ENC_LATIN1, 32, res );
    ur_type(res) = UT_FILE;

#ifdef __ANDROID__
    // NOTE: NativeActivity has a getCacheDir method, but this requires JNI.
    {
    struct android_app* app = (struct android_app*) gView;
    ur_strAppendCStr( str, app->activity->internalDataPath );
    }
#elif defined(__APPLE__)
    if( (path = getenv( "HOME" )) )
        ur_strAppendCStr( str, path );
#elif defined(__linux__)
    if( (path = getenv( "HOME" )) )
        ur_strAppendCStr( str, path );
#elif defined(_WIN32)
    if( (path = getenv( "LOCALAPPDATA" )) )
        ur_strAppendCStr( str, path );
#else
#error "_userDataDirectory must be implemented"
#endif
    return str;
}


/*-cf-
    cache-dir
    return: File! containing path of OS specific cache directory.
*/
/*-cf-
    config-dir
    return: File! containing path of OS specific configuration directory.
*/
CFUNC( cfunc_cache_dir )
{
    UBuffer* str = _userDataDirectory( ut, res );

#if defined(__APPLE__)
    if( str->used )
        ur_strAppendCStr( str, ur_int(a1) ? "/Library/Preferences"
                                          : "/Library/Caches" );
#elif defined(__linux__) && ! defined(__ANDROID__)
    if( str->used )
        ur_strAppendCStr( str, ur_int(a1) ? "/.config" : "/.cache" );
#else
    (void) a1;
#endif

    return UR_OK;
}


/*-cf-
    text-size
        font    font!
        text    string!
    return: coord! width,height.
*/
CFUNC( cfunc_text_size )
{
    if( ur_is(a1, UT_FONT) && ur_is(a1+1, UT_STRING) )
    {
        USeriesIter si;
        int size[2];
        TexFont* tf = ur_texFontV( ut, a1 );
        if( tf )
        {
            ur_seriesSlice( ut, &si, a1+1 );
            txf_pixelSize( tf, si.buf->ptr.b + si.it,
                               si.buf->ptr.b + si.end, size );

            ur_setId(res, UT_COORD);
            res->coord.len = 2;
            res->coord.n[0] = size[0];
            res->coord.n[1] = size[1];
            return UR_OK;
        }
    }
    return ur_error( ut, UR_ERR_TYPE, "text-size expected font! string!" );
}


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
#ifdef SIMULATE_APP_RESTART
        if( glEnv.guiThrow == 22 )
        {
            int w = gView->width;
            int h = gView->height;

            printf( "SIMULATE_APP_RESTART\n" );
            boron_releaseGL( ut );
            glv_destroy( gView );

            gView = glv_create( GLV_ATTRIB_DOUBLEBUFFER |
                                GLV_ATTRIB_MULTISAMPLE |
                                GLV_ATTRIB_DEBUG |
                                GLV_ATTRIB_ES );
#ifdef DEBUG_GL
            enableGLDebug();
#endif
            gView->user = &glEnv;
            glv_resize( gView, w, h );
            glv_show( gView );
            glv_makeCurrent( gView );
            gView->width = w;
            gView->height = h;
            gui_signalWindowCreated( w, h );
            doAppEventHandler( ut, UR_ATOM_ON_WINDOW_CREATED );

            glEnv.guiThrow = 0;
            glv_setEventHandler( gView, eventHandler );

            ur_setId(res, UT_UNSET);
            return UR_OK;
        }
#endif
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
        color       double!/coord!/vec3!
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
    else if( ur_is(a1, UT_DOUBLE) )
    {
        GLclampf col = ur_double(a1);
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

    // Disable any active shader to quiet glDebugMessage id 0x20092 from the
    // NVidia 430.40 driver.  The message reads:
    //   Program/shader state performance warning: Vertex shader in program 1
    //   is being recompiled based on GL state.
    glUseProgram( 0 );

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
    return: coord! or none!
    Get display rectangle (0, 0, width, height).
*/
CFUNC( uc_display_area )
{
    (void) a1;
    (void) ut;
#if defined(__ANDROID__)
    // On Android display-area can be used to tell if the application is in
    // the stopped state (when there is no display or GL context).
    if( gView && gView->ctx )
#else
    if( gView )
#endif
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

    // NOTE: GLES only supports reading in RGBA format from the GL_BACK buffer.
    // Would like to grab the front buffer or we are likely to grab a blank
    // screen (since key input is done after glClear).

    glGetIntegerv( GL_VIEWPORT, vp );
    bin = ur_makeRaster( ut, UR_RAST_RGBA, vp[2], vp[3], res );
    if( bin->ptr.b )
    {
        //glFinish();
        glReadBuffer( GL_BACK );
        glReadPixels( vp[0], vp[1], vp[2], vp[3], GL_RGBA, GL_UNSIGNED_BYTE,
                      ur_rastElem(bin) );
        //printf( "KR readPixels error %X\n", glGetError() );
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
#ifdef KEY_F1
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
#endif
    }
    else if( len == 3 )
    {
             if( strnequ( cp, "spc", 3 ) ) code = KEY_Space;
        else if( strnequ( cp, "esc", 3 ) ) code = KEY_Escape;
        else if( strnequ( cp, "tab", 3 ) ) code = KEY_Tab;
        else if( strnequ( cp, "end", 3 ) ) code = KEY_End;
        else if( strnequ( cp, "del", 3 ) ) code = KEY_Delete;
#ifdef KEY_F1
        else if( strnequ( cp, "f10", 3 ) ) code = KEY_F10;
        else if( strnequ( cp, "f11", 3 ) ) code = KEY_F11;
        else if( strnequ( cp, "f12", 3 ) ) code = KEY_F12;
#endif
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
#ifdef KEY_Help
        else if( strnequ( cp, "help",     4 ) ) code = KEY_Help;
#endif
#if defined(__ANDROID__)
        else if( strnequ( cp, "app-back", 8 ) ) code = KEY_Back;
        else if( strnequ( cp, "vol-up",   6 ) ) code = KEY_Volume_Up;
        else if( strnequ( cp, "vol-down", 8 ) ) code = KEY_Volume_Down;
        else if( strnequ( cp, "power",    5 ) ) code = KEY_Power;
#endif
#ifdef KEY_KP_Up
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
#endif
    }

    return code;
}


/*-cf-
    key-code
        key  char!/word!
    return: code int!

    Maps key to window system key code.
*/
CFUNC_PUB( cfunc_key_code )
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
CFUNC( cfunc_play )
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
CFUNC( cfunc_stop )
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
    set-volume
        what    int!/word!
        vol     double!
    return: unset!
    group: audio
*/
CFUNC( cfunc_set_volume )
{
    float vol;
    const UCell* a2 = a1 + 1;
    (void) ut;

    if( ur_is(a2, UT_DOUBLE) )
    {
        vol = ur_double(a2);
        if( ur_is(a1, UT_INT) )
            aud_setSoundVolume( vol );
        else if( ur_is(a1, UT_WORD) )
            aud_setMusicVolume( vol );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    show
        widget  draw-prog!/widget!
    return: unset!
*/
CFUNC( cfunc_show )
{
    if( ur_is(a1, UT_DRAWPROG) )
    {
        ur_buffer( ur_drawProgN(a1) )->flags = 0;
    }
    else if( ur_is(a1, UT_WIDGET) )
    {
        GWidget* wp = ur_widgetPtr( a1 );
        gui_show( wp, 1 );
#if 0
        if( CFUNC_OPTIONS & OPT_SHOW_FOCUS )
        {
            gui_setKeyFocus( wp );
            //gui_setMouseFocus( root, wp );
        }
#endif
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    hide
        widget  draw-prog!/widget!
    return: unset!
*/
CFUNC( cfunc_hide )
{
    if( ur_is(a1, UT_DRAWPROG) )
    {
        ur_buffer( ur_drawProgN(a1) )->flags = UR_DRAWPROG_HIDDEN;
    }
    else if( ur_is(a1, UT_WIDGET) )
    {
        gui_show( ur_widgetPtr( a1 ), 0 );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    visible?
        widget  draw-prog!/widget!
    return: logic!
*/
CFUNC( cfunc_visibleQ )
{
    int hidden;

    if( ur_is(a1, UT_DRAWPROG) )
        hidden = ur_buffer( ur_drawProgN(a1) )->flags & UR_DRAWPROG_HIDDEN;
    else if( ur_is(a1, UT_WIDGET) )
        hidden = ur_widgetPtr( a1 )->flags & GW_HIDDEN;
    else
        hidden = 1;

    ur_setId(res, UT_LOGIC);
    ur_logic(res) = hidden ? 0 : 1;
    return UR_OK;
}


/*-cf-
    move
        widget      widget!
        position    coord!/widget!
        /center
    return: unset!
*/
CFUNC( cfunc_move )
{
#define OPT_MOVE_CENTER  0x01
    UCell* a2 = a1 + 1;
    GWidget* wp;
    GRect* area = 0;
    int x, y;

    if( ur_is(a1, UT_WIDGET) )
    {
        if( ur_is(a2, UT_WIDGET) )
        {
            GWidget* aw = ur_widgetPtr( a2 );
            area = &aw->area;
        }
        else if( ur_is(a2, UT_COORD) )
        {
            if( (CFUNC_OPTIONS & OPT_MOVE_CENTER) && (a2->coord.len < 4) )
            {
                return ur_error( ut, UR_ERR_SCRIPT,
                                 "move/center coord! requires four elements" );
            }
            area = (GRect*) a2->coord.n;
        }

        if( area )
        {
            wp = ur_widgetPtr( a1 );
            assert( sizeof(a2->coord.n[0]) == sizeof(wp->area.x) );

            if( CFUNC_OPTIONS & OPT_MOVE_CENTER )
            {
                x = (area->w - wp->area.w) / 2;
                y = (area->h - wp->area.h) / 2;
            }
            else
            {
                x = area->x;
                y = area->y;
            }
            gui_move( wp, x, y );
        }
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


/*-cf-
    resize
        widget      widget!
        area        coord!
    return: unset!
*/
CFUNC( cfunc_resize )
{
    UCell* a2 = a1 + 1;
    int x, y;
    (void) ut;

    if( ur_is(a1, UT_WIDGET) && ur_is(a2, UT_COORD) )
    {
        GWidget* wp = ur_widgetPtr( a1 );
        if( a2->coord.len > 3 )
        {
            x = a2->coord.n[2];
            y = a2->coord.n[3];
        }
        else
        {
            x = a2->coord.n[0];
            y = a2->coord.n[1];
        }
        gui_resize( wp, x, y );
    }
    ur_setId(res, UT_UNSET);
    return UR_OK;
}


static int _convertUnits( UThread* ut, const UCell* a1, UCell* res,
                          double conv )
{
    double n;
    if( ur_is(a1, UT_DOUBLE) )
        n = ur_double(a1);
    else if( ur_is(a1, UT_INT) )
        n = (double) ur_int(a1);
    else
        return ur_error( ut, UR_ERR_TYPE,
                         "Unit conversion expected int!/double!");

    ur_setId(res, UT_DOUBLE);
    ur_double(res) = n * conv;
    return UR_OK;
}


/*-cf-
    to-degrees
        rad    int!/double!
    return: degrees
    group: math
*/
CFUNC( cfunc_to_degrees )
{
    return _convertUnits( ut, a1, res, 180.0/UR_PI );
}


/*-cf-
    to-radians
        deg    int!/double!
    return: radians
    group: math
*/
CFUNC( cfunc_to_radians )
{
    return _convertUnits( ut, a1, res, UR_PI/180.0 );
}


/*-cf-
    limit
        number int!/double!
        min    int!/double!
        max    int!/double!
    return: Number clamped to min and max.
    group: math
*/
CFUNC( cfunc_limit )
{
    (void) ut;

    if( ur_is(a1, UT_DOUBLE) )
    {
        double m;
        double n = ur_double(a1);

        m = ur_double(a1+1);
        if( n < m )
        {
            n = m;
        }
        else
        {
            m = ur_double(a1+2);
            if( n > m )
                n = m;
        }

        ur_setId(res, UT_DOUBLE);
        ur_double(res) = n;
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

    return ur_error(ut, UR_ERR_TYPE, "limit expected int!/double!");
}


static void _matrixLookAt( float* mat, const float* focalPnt )
{
    float* right = mat;
    float* up    = mat + 4;
    float* zv    = mat + 8;

    zv[0] = zv[4] - focalPnt[0];
    zv[1] = zv[5] - focalPnt[1];
    zv[2] = zv[6] - focalPnt[2];
    ur_normalize( zv );

    up[0] = 0.0;
    up[1] = 1.0;
    up[2] = 0.0;

    ur_cross( up, zv, right );
    ur_normalize( right );

    // Recompute up to make perpendicular to right & zv.
    ur_cross( zv, right, up );
    ur_normalize( up );
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

    if( ur_is(a1, UT_VECTOR) && ur_is(a2, UT_VEC3) )
    {
        mat = ur_bufferSerM( a1 );
        if( ! mat )
            return UR_THROW;
        _matrixLookAt( mat->ptr.f, a2->vec3.xyz );
        *res = *a1;
        return UR_OK;
    }

    return ur_error( ut, UR_ERR_TYPE, "look-at expected vector! and vec3!" );
}


const UBuffer* _cameraOrientMatrix( UThread* ut, const UBuffer* ctx )
{
    const UCell* cell = ur_ctxCell( ctx, CAM_CTX_ORIENT );
    if( ur_is(cell, UT_VECTOR) )
    {
        const UBuffer* arr = ur_bufferSer( cell );
        if( (arr->form == UR_VEC_F32) && (arr->used == 16) )
            return arr;
    }
    return 0;
}


/*-cf-
    turntable
        camera      context!
        delta       azim,elev  coord!/vec3!
    return: Transformed camera.
    group: math
*/
CFUNC( cfunc_turntable )
{
    UBuffer* ctx;
    const UCell* a2 = a1 + 1;
    const UBuffer* mat;
    UCell* orbit;
    UCell* focus;
    double dx, dy;
    double elev;
    double ced;

    if( ur_is(a2, UT_VEC3) )
    {
        dx = a2->vec3.xyz[0];
        dy = a2->vec3.xyz[1];
    }
    else
    {
        dx = (double) a2->coord.n[0];
        dy = (double) a2->coord.n[1];
    }

    if( ! ur_is(a1, UT_CONTEXT) )
        goto bad_cam;
    if( ! (ctx = ur_bufferSerM( a1 )) )
        return UR_THROW;
    if( ctx->used < CAM_CTX_ORBIT_COUNT )
        goto bad_cam;

    orbit = ur_ctxCell(ctx, CAM_CTX_ORBIT);
    focus = ur_ctxCell(ctx, CAM_CTX_FOCAL_PNT);
    if( ! ur_is(orbit, UT_VEC3) || ! ur_is(focus, UT_VEC3) )
        goto bad_cam;
    if( ! (mat = _cameraOrientMatrix( ut, ctx )) )
        goto bad_cam;

    orbit->vec3.xyz[0] += degToRad(dx);
    elev = orbit->vec3.xyz[1];
    if( dy )
    {
        elev += degToRad(dy);
        if( elev < -1.53938 )
            elev = -1.53938;
        else if( elev > 1.53938 )
            elev = 1.53938;
        orbit->vec3.xyz[1] = elev;
    }

    ced = cos( elev ) * orbit->vec3.xyz[2];
    mat->ptr.f[12] = focus->vec3.xyz[0] + (ced * cos(orbit->vec3.xyz[0]));
    mat->ptr.f[13] = focus->vec3.xyz[1] + (orbit->vec3.xyz[2] * sin(elev));
    mat->ptr.f[14] = focus->vec3.xyz[2] + (ced * sin(orbit->vec3.xyz[0]));

    _matrixLookAt( mat->ptr.f, focus->vec3.xyz );

    *res = *a1;
    return UR_OK;

bad_cam:
    return ur_error( ut, UR_ERR_TYPE, "turntable expected orbit-cam" );
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
    int type1 = ur_type(v1);

    if( type1 == ur_type(v2) )
    {
        if( frac < 0.0 )
            frac = 0.0;
        else if( frac > 1.0 )
            frac = 1.0;

        if( type1 == UT_DOUBLE )
        {
            ur_setId(res, UT_DOUBLE);
            INTERP( ur_double(res), ur_double(v1), ur_double(v2) );
            return 1;
        }
        else if( type1 == UT_VEC3 )
        {
            ur_setId(res, UT_VEC3);
            INTERP( V3(res,0), V3(v1,0), V3(v2,0) );
            INTERP( V3(res,1), V3(v1,1), V3(v2,1) );
            INTERP( V3(res,2), V3(v1,2), V3(v2,2) );
            return 1;
        }
        else if( type1 == UT_COORD )
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
        else if( type1 == UT_QUAT )
        {
            ur_setId(res, UT_QUAT);
            quat_slerp( v1, v2, frac, res );
            return 1;
        }
    }
    return 0;
}


#define LERP_MSG "lerp expected two similar double!/coord!/vec3!/quat! values"

/*-cf-
    lerp
        value1      double!/coord!/vec3!/quat!
        value2      double!/coord!/vec3!/quat!
        fraction    double!
    return: Interpolated value.
    group: math
*/
CFUNC( cfunc_lerp )
{
    UCell* a2   = a1 + 1;
    UCell* frac = a1 + 2;

    if( ur_is(frac, UT_DOUBLE) )
    {
        if( _lerpCells( a1, a2, ur_double(frac), res ) )
            return UR_OK;
        return ur_error( ut, UR_ERR_TYPE, LERP_MSG );
    }
    return ur_error( ut, UR_ERR_TYPE, "lerp expected double! fraction" );
}


/*
  cv & res may be the same.

  Return UR_OK/UR_THROW.
*/
static int _curveValue( UThread* ut, const UCell* cv, UCell* res, double t,
                        double* period )
{
    UBlockIt bi;
    const UCell* v1;
    int high, mid, low, last;
    double d;
    double interval;

    ur_blockIt( ut, &bi, cv );
    high = bi.end - bi.it;
    if( high & 1 )
    {
        --high;
        --bi.end;
    }
    if( high < 4 )
    {
        if( high )
        {
            *res = bi.it[1];
            *period = ur_double( bi.end - 2 );
        }
        else
        {
            ur_setId( res, UT_NONE );
            *period = 0.0;
        }
        //*atEnd = ANIM_END_LOW | ANIM_END_HIGH;
        return UR_OK;
    }
    *period = ur_double( bi.end - 2 );

    // Binary search for t in the time/value pairs.

    low = 0;
    high = last = (high >> 1) - 1;
    while( low < high )
    {
        mid = (low + high) >> 1;
        v1 = bi.it + (mid << 1);

        d = ur_double(v1);
        if( d < t )
            low = mid + 1;
        else if( d > t )
            high = mid - 1;
        else
            goto set_res;
    }

    v1 = bi.it + (low << 1);
    d = ur_double(v1);
    if( t >= d )
    {
        if( low == last )
            goto set_res;
        interval = ur_double(v1 + 2);
do_lerp:
        interval -= d;
        t -= d;
        if( interval < 0.0001 || t >= interval )
        {
            v1 += 2;
            goto set_res;
        }
        if( _lerpCells( v1 + 1, v1 + 3, t / interval, res ) )
        {
            //*atEnd = 0;
            return UR_OK;
        }
        return ur_error( ut, UR_ERR_TYPE, LERP_MSG );
    }
    else if( low )
    {
        interval = d;
        v1 -= 2;
        d = ur_double(v1);
        goto do_lerp;
    }

set_res:

    *res = v1[1];
    /*
    if( v1 == bi.it )
        *atEnd = ANIM_END_LOW;
    else if( v1 == (bi.end - 2) )
        *atEnd = ANIM_END_HIGH;
    else
        *atEnd = 0;
    */
    return UR_OK;
}


/*-cf-
    curve-at
        curve   block!
        time    int!/double!
    return: Value on curve at time.

    The curve block is a sequence of time and value pairs, ordered by time.
*/
CFUNC( cfunc_curve_at )
{
    UCell* a2 = a1 + 1;
    double t;

    if( ! ur_is(a1, UT_BLOCK) )
        return ur_error( ut, UR_ERR_TYPE, "curve-value expected block! curve" );

    if( ur_is(a2, UT_DOUBLE) )
        t = ur_double(a2);
    else if( ur_is(a2, UT_INT) )
        t = (double) ur_int(a2);
    else
        return ur_error( ut, UR_ERR_TYPE,
                         "curve-value expected int!/double! time" );

    return _curveValue( ut, a1, res, t, &t );
}


enum ContextIndexAnimation
{
    CI_ANIM_VALUE = 0,
    CI_ANIM_CURVE,
    CI_ANIM_SCALE,
    CI_ANIM_TIME,
    CI_ANIM_BEHAVIOR,
    CI_ANIM_CELLS
};


/*
  Return UR_OK/UR_THROW.
*/
static int _animate( UThread* ut, const UCell* acell, double dt, int* playing )
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

    period = ur_double(vc + CI_ANIM_SCALE);
    if( (period != 1.0) && (period > 0.0) )
        dt /= period;

    timec = vc + CI_ANIM_TIME;

    if( ur_is(behav, UT_INT) )
    {
        int repeat = ur_int(behav);

        if( repeat > 0 )
        {
            dt = ur_double(timec) + dt;
            if( ! _curveValue( ut, curve, value, dt, &period ) )
                return UR_THROW;
            if( dt > period )
            {
                if( repeat <= 1 ) 
                {
                    ur_setId(behav, UT_NONE);
                }
                else
                {
                    dt -= period;
                    ur_int(behav) = repeat - 1;
                }
            }
        }
        else if( repeat < 0 )
        {
            dt = ur_double(timec) - dt;
            if( ! _curveValue( ut, curve, value, dt, &period ) )
                return UR_THROW;
            if( dt < 0.0 )
            {
                if( repeat >= -1 ) 
                {
                    ur_setId(behav, UT_NONE);
                }
                else
                {
                    dt += period;
                    ur_int(behav) = repeat + 1;
                }
            }
        }
        else
        {
            ur_setId(behav, UT_NONE);
            return UR_OK;
        }

set_time:
        ur_double(timec) = dt;
        *playing = 1;   // Set as playing even at end so final frame is shown.
    }
    else if( ur_is(behav, UT_WORD) )
    {
        switch( ur_atom(behav) )
        {
            case UR_ATOM_LOOP:
                dt += ur_double(timec);
                if( ! _curveValue( ut, curve, value, dt, &period ) )
                    return UR_THROW;
                if( dt > period )
                    dt -= period;
                goto set_time;

            case UR_ATOM_PING_PONG:
            case UR_ATOM_PONG:
                break;
        }
    }
    return UR_OK;
}


/*-cf-
    animate
        anims   block!/context!   Animation or block of animations
        time    double!          Delta time
    return: True if any animations are playing.
*/
CFUNC( cfunc_animate )
{
    double dt;
    int playing = 0;

    if( ! ur_is(a1 + 1, UT_DOUBLE) )
        return ur_error( ut, UR_ERR_TYPE, "animate expected double! time" );
    dt = ur_double(a1 + 1);

    if( ur_is(a1, UT_CONTEXT) )
    {
        if( ! _animate( ut, a1, dt, &playing ) )
            return UR_THROW;
    }
    else if( ur_is(a1, UT_BLOCK) )
    {
        UBlockIt bi;
        ur_blockIt( ut, &bi, a1 );
        ur_foreach( bi )
        {
            if( ur_is(bi.it, UT_CONTEXT) )
            {
                if( ! _animate( ut, bi.it, dt, &playing ) )
                    return UR_THROW;
            }
        }
    }
    else
    {
        return ur_error( ut, UR_ERR_TYPE, "animate expected block!/context!" );
    }
    ur_setId(res, UT_LOGIC);
    ur_logic(res) = playing;
    return UR_OK;
}


/*-cf-
    blit
        dest    raster!
        src     raster!
        pos     coord!  Destination position and optional size
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


UBuffer* makeDistanceField( UThread* ut, const RasterHead* src,
                            int (*inside)( uint8_t*, void* ), void* user,
                            double scale, UCell* res );

static int _insideMask( uint8_t* pix, void* user )
{
    uint32_t img = pix[0] << 16 | pix[1] << 8 | pix[2];
    uint32_t mask = *((uint32_t*) user);
    return img != mask;
}

/*-cf-
    make-sdf
        src     raster!
        mask    int!
        scale   double!
    return: New signed distance field raster.
    group: data
*/
CFUNC( cfunc_make_sdf )
{
    uint32_t mask = ur_int(a1 + 1);
    const UCell* scale = a1 + 2;
    makeDistanceField( ut, ur_rastHead(a1), _insideMask, &mask,
                       ur_double(scale), res );
    return UR_OK;
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
CFUNC( cfunc_point_in )
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
        ur_logic(res) = inside;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "point-in expected two coord! values" );
}


typedef struct
{
    float proj[ 16 ];
    float orient[ 16 ];
    float zNear;
    float zFar;
    float view[4];
}
Camera;


float number_f( const UCell* cell )
{
    if( ur_is(cell, UT_DOUBLE) )
        return (float) ur_double(cell);
    if( ur_is(cell, UT_INT) )
        return (float) ur_int(cell);
    return 0.0f;
}


/*
  Returns non-zero if ctx is a valid camera with a perspective projection.
*/
static int cameraData( UThread* ut, const UBuffer* ctx, Camera* cam )
{
    const UCell* viewC;
    const UCell* val;
    const UBuffer* mat;
    float fov;
    float w, h;
    int i;

    if( ctx->used < CAM_CTX_COUNT )
        return 0;

    viewC = ur_ctxCell( ctx, CAM_CTX_VIEWPORT );
    if( ur_is(viewC, UT_COORD) && (viewC->coord.len > 3) )
    {
        for( i = 0; i < 4; ++i )
            cam->view[ i ] = (float) viewC->coord.n[ i ];

        // This clipping plane fetch matches dop_camera().
        val = ur_ctxCell( ctx, CAM_CTX_CLIP );
        if( ur_is(val, UT_VEC3) )
        {
            cam->zNear = val->vec3.xyz[0];
            cam->zFar  = val->vec3.xyz[1];
        }
        else
        {
            cam->zNear =  0.1;
            cam->zFar  = 10.0;
        }

        fov = number_f( ur_ctxCell( ctx, CAM_CTX_FOV ) );
        if( fov > 0.0 )
        {

            w = (float) viewC->coord.n[2];
            h = (float) viewC->coord.n[3];
            ur_perspective( cam->proj, fov, w / h, cam->zNear, cam->zFar );

            if( (mat = _cameraOrientMatrix( ut, ctx )) )
            {
                ur_matrixInverse( cam->orient, mat->ptr.f );
                return 1;
            }
        }
    }
    return 0;
}


/*
  http://www.opengl.org/wiki/GluProject_and_gluUnProject_code
*/
static int projectPoint( const float* pnt, const Camera* cam, float* windowPos,
                         int dropNear )
{
    float tmp[7];
    float w;
    const float* mat;

    // Modelview transform (pnt w is always 1)
    mat = cam->orient;
    tmp[0] = mat[0]*pnt[0] + mat[4]*pnt[1] + mat[8] *pnt[2] + mat[12];
    tmp[1] = mat[1]*pnt[0] + mat[5]*pnt[1] + mat[9] *pnt[2] + mat[13];
    tmp[2] = mat[2]*pnt[0] + mat[6]*pnt[1] + mat[10]*pnt[2] + mat[14];
    tmp[3] = mat[3]*pnt[0] + mat[7]*pnt[1] + mat[11]*pnt[2] + mat[15];

    // Projection transform.
    mat = cam->proj;
    tmp[4] = mat[0]*tmp[0] + mat[4]*tmp[1] + mat[8] *tmp[2] + mat[12]*tmp[3];
    tmp[5] = mat[1]*tmp[0] + mat[5]*tmp[1] + mat[9] *tmp[2] + mat[13]*tmp[3];
#ifdef PROJECT_Z
    tmp[6] = mat[2]*tmp[0] + mat[6]*tmp[1] + mat[10]*tmp[2] + mat[14]*tmp[3];
#endif
    w = -tmp[2];    // Last row of projection matrix is always [0 0 -1 0].

    // Do perspective division to normalize between -1 and 1.
    if( w == 0.0 )
        return 0;
    if( w < cam->zNear )
    {
        if( dropNear )
            return 0;
        if( w < 0.0 )
            w = -w;
    }

    w = 1.0 / w;
    tmp[4] *= w;
    tmp[5] *= w;
#ifdef PROJECT_Z
    tmp[6] *= w;
#endif

    // Map to window coordinates.
    windowPos[0] = (tmp[4] * 0.5 + 0.5) * cam->view[2] + cam->view[0];
    windowPos[1] = (tmp[5] * 0.5 + 0.5) * cam->view[3] + cam->view[1];
#ifdef PROJECT_Z
    // This is only correct when glDepthRange(0.0, 1.0)
    windowPos[2] = (1.0 + tmp[6]) * 0.5;  // Between 0 and 1
#endif
    return 1;
}


/*-cf-
    pick-point
        screen-point    coord!    (x,y)
        camera          context!
        points          vector!
        pos-offset      int!/coord!   (stride,offset)
    return: int!/none!
*/
CFUNC( cfunc_pick_point )
{
    Camera cam;
    float sx, sy;
    float pnt[ 3 ];
    float d;
    float dist = 9999999.0;
    UIndex closest = -1;
    int stride = 3;

    if( ! ur_is(a1, UT_COORD) )
    {
        return ur_error( ut, UR_ERR_TYPE,
                         "pick-point expected screen-point coord!" );
    }
    sx = (float) a1->coord.n[0];
    sy = (float) a1->coord.n[1];

    if( ! ur_is(a1 + 1, UT_CONTEXT) )
    {
bad_camera:
        return ur_error( ut, UR_ERR_TYPE,
                         "pick-point expected camera context!" );
    }
    if( ! cameraData( ut, ur_bufferSer( a1 + 1 ), &cam ) )
        goto bad_camera;
    //sy = cam.view[3] - sy;    // Needed if screen-point is not in GL coord. 

    if( ur_is(a1 + 2, UT_VECTOR) )
    {
        USeriesIter si;
        float* vpnt;

        ur_seriesSlice( ut, &si, a1 + 2 );
        if( si.buf->form != UR_VEC_F32 )
            goto bad_vector;

        vpnt = si.buf->ptr.f;
        {
            const UCell* size = a1 + 3;
            if( ur_is(size, UT_INT) )
            {
                stride = ur_int(size);
            }
            else if( ur_is(size, UT_COORD) )
            {
                stride  = size->coord.n[0];
                vpnt   += size->coord.n[1];
            }
        }

        for( si.end -= 2; si.it < si.end; si.it += stride )
        {
            if( ! projectPoint( vpnt + si.it, &cam, pnt, 1 ) )
                continue;
            //printf( "KR point %d %f,%f\n", si.it, pnt[0], pnt[1] );

            pnt[0] -= sx;
            pnt[1] -= sy;
            d = pnt[0] * pnt[0] + pnt[1] * pnt[1];
            if( d < dist )
            {
                dist = d;
                closest = si.it;
            }
        }
    }
    else
    {
bad_vector:
        return ur_error( ut, UR_ERR_TYPE,
                         "pick-point expected points f32 vector!" );
    }

    if( closest > -1 )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = closest / stride;
    }
    else
    {
        ur_setId(res, UT_NONE);
    }
    return UR_OK;
}


/*-cf-
    project-point
        pnt vec3!
        a   vec3!/context!
        b   vec3!
    return: Point projected onto line or screen.
    group: math

    To project the point onto a line pass vec3! end points for a and b.

    If 'a is a camera context! then the point will be projected onto the
    camera's viewport.
*/
CFUNC( cfunc_project_point )
{
    const UCell* a = a1 + 1;
    const UCell* b = a1 + 2;
    float* rp = res->vec3.xyz;

    // TODO: Support projection onto plane (a: vec3! b: double!)

    if( ! ur_is(a1, UT_VEC3) )
    {
        return ur_error( ut, UR_ERR_TYPE,
                         "project-point expected vec3! point" );
    }

    ur_setId(res, UT_VEC3);

    if( ur_is(a, UT_VEC3) && ur_is(b, UT_VEC3) )
    {
        const float* pnt = a1->vec3.xyz;
        ur_lineToPoint( a->vec3.xyz, b->vec3.xyz, pnt, rp );
        rp[0] = pnt[0] - rp[0];
        rp[1] = pnt[1] - rp[1];
        rp[2] = pnt[2] - rp[2];
        return UR_OK;
    }
    else if( ur_is(a, UT_CONTEXT) )
    {
        Camera cam;
        if( cameraData( ut, ur_bufferSer( a ), &cam ) )
        {
            if( ! projectPoint( a1->vec3.xyz, &cam, rp, 0 ) )
                rp[0] = rp[1] = rp[2] = -1.0f;
            return UR_OK;
        }
    }
    return ur_error( ut, UR_ERR_TYPE,
                     "project-point expexted line vec3! or camera context!" );
}


#if defined(GL_ES_VERSION_2_0) && ! defined(GL_OES_mapbuffer)
#define CHANGE_SUBDATA  1
#endif

/*-cf-
    change-vbo
        buffer  vbo!
        data    vector!
        length  int!/coord! (stride,offset,npv)
    return: unset!
    group: data
*/
CFUNC( cfunc_change_vbo )
{
    UCell* vec = a1 + 1;
    UCell* len = a1 + 2;
    int copyLen;

#ifdef CHANGE_SUBDATA
    if( ur_is(len, UT_INT) )
        copyLen = ur_int(len);
#else
    int stride;
    int offset;
    int loops;

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
#endif
    else
        goto bad_dt;

    if( ur_is(vec, UT_VECTOR) && //(ur_vectorDT(vec) == UT_DOUBLE) &&
        ur_is(a1, UT_VBO) )
    {
        USeriesIter si;
        UBuffer* vbo = ur_buffer( ur_vboResN(a1) );
        GLuint* buf = vbo_bufIds(vbo);

        ur_seriesSlice( ut, &si, vec );

        // TODO: Need way to check if buf[0] is an GL_ARRAY_BUFFER.

        if( vbo_count(vbo) && (si.it < si.end) && copyLen )
        {
            float* src = si.buf->ptr.f + si.it;

#ifdef CHANGE_SUBDATA
            glBindBuffer( GL_ARRAY_BUFFER, buf[0] );
            glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(float) * copyLen, src );
#else
            GLfloat* dst;

            glBindBuffer( GL_ARRAY_BUFFER, buf[0] );
            dst = (GLfloat*) glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
            if( ! dst )
                return ur_error( ut, UR_ERR_INTERNAL, "glMapBuffer failed" );

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
#endif
        }
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }

bad_dt:

    return ur_error( ut, UR_ERR_TYPE, "change-vbo expected vbo! vector! "
#ifdef CHANGE_SUBDATA
                     "int!"
#else
                     "int!/coord!"
#endif
                   );
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
    gl-version
    return: string!
    group: gl
*/
CFUNC( uc_gl_version )
{
    UBuffer* buf = ur_makeStringCell( ut, UR_ENC_LATIN1, 0, res );
    (void) a1;
    ur_strAppendCStr( buf, (char*) glGetString( GL_VERSION ) );
    return UR_OK;
}


/*-cf-
    gl-max-textures
    return: GL_MAX_TEXTURE_IMAGE_UNITS int!
    group: gl
*/
CFUNC( uc_gl_max_textures )
{
    GLint count;
    (void) ut;
    (void) a1;

    glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &count );

    ur_setId(res, UT_INT);
    ur_int(res) = count;
    return UR_OK;
}


/*
  Returns zero and throws error if matrix is invalid.
*/
float* ur_matrixM( UThread* ut, const UCell* cell )
{
    if( ur_is(cell, UT_VECTOR) )
    {
        UBuffer* mat = ur_bufferSerM( cell );
        if( ! mat )
            return 0;
        if( (mat->form == UR_VEC_F32) && (mat->used >= 16) )
            return mat->ptr.f;
    }
    ur_error( ut, UR_ERR_TYPE, "Expected matrix vector!" );
    return 0;
}


/*
  Returns zero and throws error if matrix is invalid.
*/
float* ur_matrix( UThread* ut, const UCell* cell )
{
    if( ur_is(cell, UT_VECTOR) )
    {
        const UBuffer* mat = ur_bufferSer( cell );
        if( (mat->form == UR_VEC_F32) && (mat->used >= 16) )
            return mat->ptr.f;
    }
    ur_error( ut, UR_ERR_TYPE, "Expected matrix vector!" );
    return 0;
}


/*-cf-
    set-matrix
        matrix  vector!
        value   quat!/vector!/word!
    return: Modified matrix
    group: gl
*/
CFUNC( cfunc_set_matrix )
{
    const UCell* a2 = a1 + 1;
    float* matf = ur_matrixM( ut, a1 );
    if( ! matf )
        return UR_THROW;
    if( ur_is(a2, UT_QUAT) )
    {
        quat_toMatrix( a2, matf, 0 );
set_res:
        *res = *a1;
        return UR_OK;
    }
    else if( ur_is(a2, UT_WORD) )
    {
        // Transpose 3x3.
        float tmp;

#define MAT_SWAP(A,B) \
        tmp = matf[A]; \
        matf[A] = matf[B]; \
        matf[B] = tmp

        MAT_SWAP( 1, 4 );
        MAT_SWAP( 2, 8 );
        MAT_SWAP( 6, 9 );

        goto set_res;
    }
    else
    {
        const float* src = ur_matrix( ut, a2 );
        if( src )
        {
            memCpy( matf, src, sizeof(float) * 16 );
            goto set_res;
        }
    }
    return ur_error( ut, UR_ERR_TYPE, "set-matrix expected vector!/quat!" );
}


/*-cf-
    mul-matrix
        value   vector!/vec3!
        matrix  vector!
    return: Modified value.
    group: gl
*/
CFUNC( cfunc_mul_matrix )
{
    const float* matB;

    if( ! (matB = ur_matrix( ut, a1 + 1 )) )
        return UR_THROW;

    *res = *a1;
    if( ur_is(a1, UT_VEC3) )
    {
        ur_transform( res->vec3.xyz, matB );
    }
    else
    {
        float* matA;
        if( ! (matA = ur_matrixM( ut, a1 )) )
            return UR_THROW;
        ur_matrixMult( matA, matB, matA );
    }
    return UR_OK;
}


/*-cf-
    translate
        matrix  vector!
        vector  vec3!
    return: Modified matrix.
    group: gl
*/
CFUNC( cfunc_translate )
{
    float* mat;
    const UCell* a2 = a1 + 1;

    if( ! (mat = ur_matrix( ut, a1 )) )
        return UR_THROW;
    *res = *a1;

    mat[12] += a2->vec3.xyz[0];
    mat[13] += a2->vec3.xyz[1];
    mat[14] += a2->vec3.xyz[2];
    return UR_OK;
}


/*-cf-
    rotate
        matrix  vector!
        axis    vec3!
        angle   double!  Angle in degrees.
    return: Modified matrix.
    group: gl
*/
CFUNC( cfunc_rotate )
{
    float rot[16];
    float* mat;

    if( ! (mat = ur_matrix( ut, a1 )) )
        return UR_THROW;
    *res = *a1;

    ur_loadRotation( rot, (a1+1)->vec3.xyz, degToRad( ur_double(a1+2) ) );
    ur_matrixMult( mat, rot, mat );
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
        ur_is(tos, UT_DOUBLE) )
    {
        PReal x, y, w, h, n;
        int in;
        uint8_t* ep;

        octaves = ur_int(octc);
        if( octaves < 1 )
            octaves = 1;
        else if( octaves > 4 )
            octaves = 4;

        persist = ur_double(tos);
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

    return ur_error( UR_ERR_TYPE, "perlin expected raster! int! double!" );
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
#ifndef GL_ES_VERSION_2_0
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                                        GL_COMPARE_R_TO_TEXTURE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC,
                                        GL_LEQUAL );
#endif
        //glTexParameteri( GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE,
        //                 GL_INTENSITY );


        fboName = glid_genFramebuffer();
        glBindFramebuffer( GL_FRAMEBUFFER, fboName );
        glFramebufferTexture2D( GL_FRAMEBUFFER,
                 GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texName, 0 );
#ifndef GL_ES_VERSION_2_0
        glDrawBuffer( GL_NONE );
        glReadBuffer( GL_NONE );
#endif

        if( (err = _framebufferStatus()) )
            return ur_error( ut, UR_ERR_INTERNAL, err );

        glBindFramebuffer( GL_FRAMEBUFFER, 0 );

        ur_setId(res, UT_FBO);
        ur_fboId(res)    = fboName;
        ur_fboRenId(res) = 0;
        ur_fboTexId(res) = texName;
        return UR_OK;
    }
    return ur_error( ut, UR_ERR_TYPE, "shadowmap expected coord!" );
}


/*-cf-
    pick-drawn
        prog    draw-prog!
        point   coord!      Position on display.
    return: int! containing RGB color at point.
    group: gl

    NOTE: This renders into the display framebuffer, so it should be called
    before the normal frame rendering.
*/
CFUNC( cfunc_pick_drawn )
{
    uint8_t pixel[4];
    int px, py;
    UStatus ok;

    px = a1[1].coord.n[0];
    py = a1[1].coord.n[1];

    glEnable( GL_SCISSOR_TEST );
    glScissor( px, py, 1, 1 );
    //glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glEnable( GL_DEPTH_TEST );
    glEnable( GL_CULL_FACE );

    ok = ur_runDrawProg( ut, a1->series.buf );

    glDisable( GL_SCISSOR_TEST );
    glReadPixels( px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel );

    if( ok == UR_OK )
    {
        ur_setId(res, UT_INT);
        ur_int(res) = ((int)pixel[0] << 16) | ((int)pixel[1] << 8) | pixel[2];
        //printf( "KR pick-drawn %d,%d -> 0x%lX\n", px, py, ur_int(res) );
    }
    return ok;
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
        return ur_runDrawProg( ut, a1->series.buf );
    }
    else if( ur_is(a1, UT_WIDGET) )
    {
        GWidget* wp = ur_widgetPtr(a1);
        wp->wclass->render( wp );
    }
    return UR_OK;
}


CFUNC_PUB( cfunc_free );

// Override Boron free.
CFUNC( cfunc_freeGL )
{
    if( ur_is(a1, UT_WIDGET) )
    {
        gui_freeWidgetDefer( ur_widgetPtr( a1 ) );
        ur_setId(res, UT_UNSET);
        return UR_OK;
    }
    return cfunc_free( ut, a1, res );
}


static char _bootScript[] =
#include "boot.c"
    ;


extern CFUNC_PUB( cfunc_buffer_audio );
extern CFUNC_PUB( cfunc_load_png );
extern CFUNC_PUB( cfunc_save_png );
//extern CFUNC_PUB( cfunc_particle_sim );


#ifdef DEBUG
#include <ctype.h>
#endif

// Intern commonly used atoms.
static void _createFixedAtoms( UThread* ut )
{
#define FA_COUNT    76
    UAtom atoms[ FA_COUNT ];

    ur_internAtoms( ut,
        "add size loop repeat text binary wait close context\n"
        "width height area rect raster texture\n"
        "gui-style shader-cache value elem focus resize key-down key-up\n"
        "mouse-move mouse-up mouse-down mouse-wheel\n"
        "on-window-created on-window-destroyed on-save-instance\n"
        "root parent child\n"
        "ambient diffuse specular pos shader vertex normal fragment\n"
        "default dynamic static stream\n"
        "left right center bottom top horiz vert\n"
        "rgb rgba depth clamp nearest linear\n"
        "min mag mipmap gray\n"
        "burn color trans sprite\n"
        "once ping-pong pong\n"
        "collide fall integrate attach anchor action face",
        atoms );

#ifdef DEBUG
    if( atoms[0] != UR_ATOM_ADD ||
        atoms[4] != UR_ATOM_TEXT ||
        atoms[9] != UR_ATOM_WIDTH ||
        atoms[FA_COUNT - 1] != UR_ATOM_FACE )
    {
        char str[ 64 ]; // MAX_WORD_LEN
        char* sp;
        const char* cp;
        int i, c;
        UAtom ca, prev = 0xffff;

        fprintf( stderr, "#ifndef GL_ATOMS_H\n#define GL_ATOMS_H\n\n"
                 "enum GLFixedAtoms\n{\n" );
        for( i = 0; i < FA_COUNT; ++i )
        {
            ca = atoms[i];
            cp = ur_atomCStr(ut, ca);
            sp = str;
            while( (c = *cp++) )
                *sp++ = (c == '-') ? '_' : toupper( c );
            *sp = '\0';
            fprintf( stderr, "%s\tUR_ATOM_%s", i ? ",\n" : "", str );
            if( ca != prev + 1 )
                fprintf( stderr, "\t= %d", ca );
            prev = ca;
        }
        fprintf( stderr, "\n};\n\n#endif\n" );
    }
#endif

    assert( atoms[0] == UR_ATOM_ADD );
    assert( atoms[1] == UR_ATOM_SIZE );
    assert( atoms[2] == UR_ATOM_LOOP );
    assert( atoms[3] == UR_ATOM_REPEAT );
    assert( atoms[4] == UR_ATOM_TEXT );
    assert( atoms[5] == UR_ATOM_BINARY );
    assert( atoms[6] == UR_ATOM_WAIT );
    assert( atoms[7] == UR_ATOM_CLOSE );
    assert( atoms[8] == UR_ATOM_CONTEXT );
    assert( atoms[9] == UR_ATOM_WIDTH );
    assert( atoms[FA_COUNT - 1] == UR_ATOM_FACE );
}


static void _createDrawOpTable( UThread* ut )
{
#define DA_COUNT    57
    UAtom atoms[ DA_COUNT ];
    UAtomEntry* ent;
    UBuffer* buf;
    UIndex bufN;
    int i;

    // Matches order in draw_ops.h.
    ur_internAtoms( ut,
        "nop end clear enable disable\n"
        "call solid model decal image\n"
        "particle color colors verts normals\n"
        "uvs attrib points lines line-strip\n"
        "tris tri-strip tri-fan quads quad_strip\n"
        "tris-inst\n"
        "sphere box quad camera light\n"
        "lighting push pop translate rotate\n"
        "scale font text shader uniform\n"
        "framebuffer framebuffer-tex\n"
        "shadow-begin shadow-end samples-query samples-begin\n"
        "buffer buffer-inst depth-test blend cull color-mask\n"
        "depth-mask point-size point-sprite read-pixels",
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

#define DEF_CF(f,spec)  f,
static BoronCFunc _glfuncs[] =
{
#include "glfunc_table.c"
};

#undef DEF_CF
#define DEF_CF(f,spec)  spec
static const char _glfuncSpecs[] =
{
#include "glfunc_table.c"
};

extern void gui_addStdClasses();


UThread* boron_makeEnvGL( UEnvParameters* param )
{
#if defined(__linux__) && ! defined(__ANDROID__)
    static char joyStr[] = "joystick";
#endif
    UThread* ut;
    GLint version[2];


#if 0
    printf( "sizeof(UCellRasterFont) %ld\n", sizeof(UCellRasterFont) );
    printf( "sizeof(GLuint) %ld\n", sizeof(GLuint) );
    printf( "sizeof(GWidget) %ld\n", sizeof(GWidget) );
#endif
    assert( sizeof(GLuint) == 4 );


    // Initialize glEnv before boron_makeEnv() since the datatype recycle
    // methods will get called.

    glEnv.view = 0;
    glEnv.guiUT = 0;
    glEnv.guiArgBlkN = UR_INVALID_BUF;
    glEnv.prevMouseX = MOUSE_UNSET;
    glEnv.prevMouseY = MOUSE_UNSET;
    glEnv.guiThrow = 0;

    {
    const UDatatype* table[ UT_GL_COUNT - UT_BORON_COUNT ];
    unsigned int i;

    for( i = 0; i < (sizeof(gl_types) / sizeof(UDatatype)); ++i )
        table[i] = gl_types + i;
    param->dtCount = i;
    param->dtTable = table;

    ut = boron_makeEnv( param );
    }
    if( ! ut )
        return 0;

    _createFixedAtoms( ut );
    _createDrawOpTable( ut );

    if( ! boron_defineCFunc( ut, UR_MAIN_CONTEXT, _glfuncs,
                             _glfuncSpecs, sizeof(_glfuncSpecs)-1 ) )
        return NULL;

    boron_overrideCFunc( ut, "free", cfunc_freeGL );

    if( ! boron_evalUtf8( ut, _bootScript, sizeof(_bootScript) - 1 ) )
        return NULL;

#if defined(__linux__) && ! defined(__ANDROID__)
    boron_addPortDevice( ut, &port_joystick, ur_intern(ut, joyStr, 8) );
#endif

#ifndef NO_AUDIO
    {
    char* var = getenv( "BORON_GL_AUDIO" );
    if( ! var || (var[0] != '0') )
        aud_startup();
    }
#endif

    gView = glv_create( GLV_ATTRIB_DOUBLEBUFFER | GLV_ATTRIB_MULTISAMPLE |
#ifdef DEBUG_GL
                        GLV_ATTRIB_DEBUG |
#endif
                        GLV_ATTRIB_ES );
    if( ! gView )
    {
        fprintf( stderr, "glv_create() failed\n" );
cleanup:
        boron_freeEnv( ut );
        return NULL;
    }

    //printf( "GL_VERSION: %s\n", (const char*) glGetString( GL_VERSION ) );
    glGetIntegerv(GL_MAJOR_VERSION, version);
    glGetIntegerv(GL_MINOR_VERSION, version+1);
    if( version[0] < 3 || (version[0] == 3 && version[1] < 1) )
    {
        glv_destroy( gView );
        gView = 0;
        fprintf( stderr, "OpenGL ES 3.1 required\n" );
        goto cleanup;
    }

#ifdef DEBUG_GL
    enableGLDebug();
#endif

#if 0
    {
    GLint nf;
    GLint fmt[4];
    glGetIntegerv( GL_NUM_PROGRAM_BINARY_FORMATS, &nf );
    glGetIntegerv( GL_PROGRAM_BINARY_FORMATS, fmt );
    fprintf( stderr, "binary-formats: %d (%X)\n", nf, fmt[0] );
    }
#endif

    // A non-zero guiUT is our indicator that the GL context has been created.
    glEnv.guiUT = ut;
    gView->user = &glEnv;

    glv_setTitle( gView, "Boron-GL" );
    glv_setEventHandler( gView, eventHandler );
    //glv_showCursor( gView, 0 );

    glid_startup();
    //gui_init( &glEnv.gui, ut );

    ur_binInit( &glEnv.tmpBin, 0 );
    ur_strInit( &glEnv.tmpStr, UR_ENC_LATIN1, 0 );
    ur_ctxInit( &glEnv.widgetClasses, 0 );
    ur_arrInit( &glEnv.rootWidgets, sizeof(GWidget*), 0 );

    gui_addStdClasses();

    return ut;
}


void boron_freeEnvGL( UThread* ut )
{
    if( ut )
    {
        ur_binFree( &glEnv.tmpBin );
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

        // Free Boron before view since datatypes can make glDelete* calls.
        boron_freeEnv( ut );
        glv_destroy( gView );
    }
}


/*
  Destroy all datatypes in the UThread dataStore that reference OpenGL
  resources.  Note that this does not garbage collect the buffers.
*/
void boron_releaseGL( UThread* ut )
{
    const UDatatype** dt = ut->types;
    UBuffer* it  = ut->dataStore.ptr.buf;
    UBuffer* end = it + ut->dataStore.used;

    while( it != end )
    {
        switch( it->type )
        {
            case UT_DRAWPROG:
            case UT_TEXTURE:
            case UT_SHADER:
            case UT_FBO:
            case UT_VBO:
                dt[ it->type ]->destroy( it );
                break;
        }
        ++it;
    }

    glid_release();
}


/*
   Local version of gluErrorString() so we don't need to link with libGLU.
*/
const char* gl_errorString( GLenum error )
{
    switch( error )
    {
        case GL_NO_ERROR:           return "no error";
        case GL_INVALID_ENUM:       return "invalid enumerant";
        case GL_INVALID_VALUE:      return "invalid value";
        case GL_INVALID_OPERATION:  return "invalid operation";
#ifdef GL_STACK_OVERFLOW
        case GL_STACK_OVERFLOW:     return "stack overflow";
        case GL_STACK_UNDERFLOW:    return "stack underflow";
#endif
        case GL_OUT_OF_MEMORY:      return "out of memory";
#ifdef GL_TABLE_TOO_LARGE
        case GL_TABLE_TOO_LARGE:    return "table too large";
#endif
        case GL_INVALID_FRAMEBUFFER_OPERATION:
                                    return "invalid framebuffer operation";
    }
    return "unnkown GL error";
}


// EOF
