

#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if !defined(__LP64__)
#include <time64.h>
#endif
#include <glv_activity.h>
#include <boron/boron-gl.h>
#include <boron/urlan_atoms.h>


#define  LOG_TAG    "libborongl"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define REDIRECT_IO 1

//#define LOCAL_RENDER


#if !defined(__LP64__)
// https://chromium.googlesource.com/chromium/src/+/master/base/os_compat_android.cc
// 32-bit Android has only timegm64() and no timegm().
// We replicate the behaviour of timegm() when the result overflows time_t.
time_t timegm( struct tm* const t )
{
    // time_t is signed on Android.
    static const time_t kTimeMax = ~(1 << (sizeof(time_t) * CHAR_BIT - 1));
    static const time_t kTimeMin =  (1 << (sizeof(time_t) * CHAR_BIT - 1));
    time64_t result = timegm64(t);
    if( result < kTimeMin || result > kTimeMax )
        return -1;
    return result;
}
#endif


#ifdef REDIRECT_IO
static void* stdioThread( void* arg )
{
    char buf[ 256 ];
    FILE* in = fdopen( ((int*) arg)[0], "r" );
    while( 1 )
    {
        fgets( buf, sizeof(buf), in );
        __android_log_write( ANDROID_LOG_VERBOSE, "stderr", buf );
    }
    return NULL;
}


int gStdOutPipe[2];

void redirectIO()
{
    pthread_t thr;
    pthread_attr_t attr;

    pipe( gStdOutPipe );
    //dup2( gStdOutPipe[1], STDOUT_FILENO );    // stdio doesn't seem to work.
    dup2( gStdOutPipe[1], STDERR_FILENO );

    pthread_attr_init( &attr );
    pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

    pthread_create( &thr, &attr, stdioThread, gStdOutPipe );
}
#endif


static void reportError( UThread* ut, const UCell* errC )
{
    UBuffer str;

    ur_strInit( &str, UR_ENC_UTF8, 0 );
    ur_toText( ut, errC, &str );
    ur_strTermNull( &str );
    __android_log_write( ANDROID_LOG_ERROR, LOG_TAG, str.ptr.c );
    ur_strFree( &str );
}


#ifdef LOCAL_RENDER
static void logBoronError( UThread* ut )
{
    const UCell* ex = ur_exception( ut );
    if( ur_is(ex, UT_ERROR) )
    {
        reportError( ut, ex );
        boron_reset( ut );
    }
}


static void printGLString(const char *name, GLenum s)
{
    const char* v = (const char*) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}


static void checkGlError(const char* op)
{
    GLint error;
    for( error = glGetError(); error; error = glGetError() )
    {
        LOGI("after %s() glError (0x%x)\n", op, error);
    }
}


GLuint loadShader(GLenum shaderType, const char* pSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen)
            {
                char* buf = (char*) malloc(infoLen);
                if (buf)
                {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d:\n%s\n", shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}


GLuint createProgram(const char* pVertexSource, const char* pFragmentSource)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if( ! vertexShader )
        return 0;

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if( ! pixelShader)
        return 0;

    GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}


GLuint gProgram;
GLuint gvPositionHandle;

static const char gVertexShader[] =
    "attribute vec4 vPosition;\n"
    "void main() {\n"
    "  gl_Position = vPosition;\n"
    "}\n";

static const char gFragmentShader[] =
    "precision mediump float;\n"
    "void main() {\n"
    "  gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
    "}\n";

int setupGraphics( int w, int h )
{
    printGLString("Version", GL_VERSION);
    printGLString("Vendor", GL_VENDOR);
    printGLString("Renderer", GL_RENDERER);
    printGLString("Extensions", GL_EXTENSIONS);

    LOGI("setupGraphics(%d, %d)", w, h);
    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram) {
        LOGE("Could not create program.");
        return 0;
    }
    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    checkGlError("glGetAttribLocation");
    LOGI("glGetAttribLocation(\"vPosition\") = %d\n", gvPositionHandle);

    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return 1;
}


const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };
GLfloat verts[ 6 ];

static void renderFrame( UThread* ut, UIndex updateBlkN )
{
    UCell* res;
    UCell* cell;
    int ok;
    static float grey = 0.0f;

    grey += 0.01f;
    if( grey > 1.0f )
        grey = 0.0f;

    memcpy( verts, gTriangleVertices, sizeof(verts) );
    if( updateBlkN )
    {
        cell = ur_push( ut, UT_BLOCK );
        res = cell - 1;
        ur_setSeries( cell, updateBlkN, 0 );
        ok = boron_doBlock( ut, cell, res );
        ur_pop( ut );
        if( UR_OK == ok )
        {
            verts[0] = res->vec3.xyz[0];
            verts[1] = res->vec3.xyz[1];
        }
        else
        {
            logBoronError( ut );
        }
    }

    glClearColor( grey, grey, grey, 1.0f );
    glClear( GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT );

    glUseProgram( gProgram );
    checkGlError( "glUseProgram" );

    glVertexAttribPointer( gvPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, verts );
    checkGlError("glVertexAttribPointer");
    glEnableVertexAttribArray( gvPositionHandle );
    checkGlError("glEnableVertexAttribArray");
    glDrawArrays( GL_TRIANGLES, 0, 3 );
    checkGlError("glDrawArrays");
}


static const char _update[] =
    "sleep 0.02\n"
    "sub random 1.0,1.0,1.0 0.5,0.5\n";


#include "glv_keys.h"

static void eventHandler( GLView* view, GLViewEvent* ev )
{
    switch( ev->type )
    {
        /*
        case GLV_EVENT_CLOSE:
            *((int*) view->user) = 0;
            break;
        */

        case GLV_EVENT_KEY_UP:
            LOGI( "KR key-up %d\n", ev->code );
            if( ev->code == KEY_Back )
                ANativeActivity_finish( ((struct android_app*)view)->activity );
            break;

        case GLV_EVENT_KEY_DOWN:
            LOGI( "KR key-down %d\n", ev->code );
            break;

        case GLV_EVENT_MOTION:
            LOGI( "KR motion %d,%d\n", ev->x, ev->y );
            break;

        case GLV_EVENT_APP:
            LOGI( "KR app %d\n", ev->code );
            if( ev->code == APP_CMD_INIT_WINDOW )
                setupGraphics( view->width, view->height );
            break;
        /*
          Event sequence initiated by ANativeActivity_finish().
            glv_activity: Pause                 -> APP_CMD_PAUSE
            glv_activity: NativeWindowDestroyed -> APP_CMD_TERM_WINDOW
            glv_activity: Stop                  -> APP_CMD_STOP
            glv_activity: InputQueueDestroyed   -> APP_CMD_INPUT_CHANGED
            glv_activity: Destroy               -> APP_CMD_DESTROY

          Event sequence when Home key pressed:
            glv_activity: WindowFocusChanged
            glv_activity: Pause
            glv_activity: SaveInstanceState     -> APP_CMD_SAVE_STATE
            glv_activity: Stop
            glv_activity: NativeWindowDestroyed

          Event sequence when Overview key pressed:
            glv_activity: Pause
            glv_activity: WindowFocusChanged *  (Sometimes seen before Pause)
            glv_activity: SaveInstanceState     -> APP_CMD_SAVE_STATE
            glv_activity: Stop
            glv_activity: NativeWindowDestroyed
        */
    }
}
#endif


#if 0
static UCell* value( UThread* ut, UAtom name, int type )
{
    UBuffer* ctx = ur_threadContext( ut );
    int i = ur_ctxLookup( ctx, name );
    if( i > -1 )
    {
        UCell* cell = ur_ctxCell( ctx, i );
        if( ur_is(cell, type) )
            return cell;
    }
    return NULL;
}


void listAssets( struct android_app* app )
{
    const char* fn;
    AAssetDir* dir = AAssetManager_openDir( app->activity->assetManager, "" );
    if( dir )
    {
        while( (fn = AAssetDir_getNextFileName( dir ) ) )
        {
            LOGI( "asset: %s\n", fn );
        }
        AAssetDir_close( dir );
    }
}
#endif


#ifndef LOCAL_RENDER
// Return pointer to a buffer which the caller must free() or NULL if failed.
void* loadAsset( struct android_app* app, const char* file, int* rlen )
{
    off_t len;
    AAsset* as;
    void* buf = 0;

    as = AAssetManager_open( app->activity->assetManager, file,
                             AASSET_MODE_BUFFER );
    if( ! as )
    {
        LOGE( "Could not open asset %s\n", file );
        return NULL;
    }

    len = AAsset_getLength( as );
    if( len > 0 )
    {
        buf = malloc( len );
        if( buf )
        {
            AAsset_read( as, buf, len );
            *rlen = len;
        }
    }

    AAsset_close( as );
    return buf;
}


UIndex loadScript( struct android_app* app, UThread* ut, const char* filename,
                   UCell* res )
{
    UIndex bufN = 0;
    int len;
    char* script = (char*) loadAsset( app, filename, &len );
    if( script )
    {
        // Comment out any Unix shell interpreter line.
        if( script[0] == '#' && script[1] == '!' )
            script[0] = ';';

        bufN = ur_tokenize( ut, script, script + len, res );
        free( script );
        if( bufN )
            boron_bindDefault( ut, bufN );
    }
    return bufN;
}
#endif


extern CFUNC_PUB(cfunc_readPort);
extern struct android_app* gGlvApp;

// Partial implementation of 'read which uses the AAssetManager rather than
// direct filesystem access.
CFUNC( cf_assetRead )
{
    const char* filename;
    AAsset* as;
    UBuffer* buf;
    int ok;
    int n;

    if( ur_is(a1, UT_PORT) )
        return cfunc_readPort( ut, a1, res );

    if( CFUNC_OPTIONS )
        return ur_error( ut, UR_ERR_SCRIPT,
                         "FIXME: Asset read does not handle options." );

    n = ur_type(a1);
    if( ur_isStringType( n ) )
    {
        filename = boron_cpath( ut, a1, 0 );
        as = AAssetManager_open( gGlvApp->activity->assetManager, filename,
                                 AASSET_MODE_BUFFER );
        if( ! as )
            return ur_error( ut, UR_ERR_ACCESS,
                             "Could not open asset %s", filename );

        ok = UR_OK;
        buf = ur_makeBinaryCell( ut, AAsset_getLength( as ), res );
        n = ur_testAvail(buf);
        if( n > 0 )
        {
            if( AAsset_read( as, buf->ptr.b, n ) == n )
                buf->used = n;
            else
                ok = ur_error( ut, UR_ERR_ACCESS, "Failed reading asset %s",
                               filename );
        }
        AAsset_close( as );
        return ok;
    }

    //return cfunc_read( ut, a1, res );
    return boron_badArg( ut, ur_type(a1), 1 );
}


void android_main( struct android_app* app )
{
    UThread* ut;
#ifdef LOCAL_RENDER
    UCell res;
    GLView* view;
    UIndex updateN;
#else
    UCell bcell;
    UCell* cell;
    UIndex bufN;
#endif


#ifdef REDIRECT_IO
    redirectIO();
#endif

#ifdef LOCAL_RENDER
    view = glv_create( 0 );
    if( ! view )
    {
        LOGE( "glv_create failed\n" );
        return;
    }

    glv_setEventHandler( view, eventHandler );
    setupGraphics( view->width, view->height );
#endif

    {
    UEnvParameters param;
#ifdef LOCAL_RENDER
    ut = boron_makeEnv( boron_envParam(&param) );
#else
    ut = boron_makeEnvGL( boron_envParam(&param) );
    boron_overrideCFunc( ut, "read", cf_assetRead );
#endif
    }
    if( ! ut )
    {
        LOGE( "boron_makeEnv failed\n" );
        return;
    }
    ur_freezeEnv( ut );

#ifdef LOCAL_RENDER
    updateN = ur_tokenize( ut, _update, _update + sizeof(_update) - 1, &res );
    ur_hold( updateN );
    boron_bindDefault( ut, updateN );

    while( ! app->destroyRequested )
    {
        glv_handleEvents( view );
        if( view->ctx )
        {
            renderFrame( ut, updateN );
            glv_swapBuffers( view );
        }
        else
        {
            // The activity has been stopped and has no display.
            glv_waitEvent( view );
        }
    }

    boron_freeEnv( ut );
    glv_destroy( view );
#else
    // Cannot simply eval "do %main.b" or call boron_load() as cfunc_load()
    // calls the internal cfunc_read(), so we must load it ourselves.

    bufN = loadScript( app, ut, "main.b", &bcell );
    if( bufN )
    {
        ur_hold( bufN );    // Hold forever.
        cell = ut->stack.ptr.cell + ut->stack.used - 1;
        if( ! boron_doBlock( ut, &bcell, cell ) )
        {
            cell = ur_exception( ut );
            if( ur_is(cell, UT_ERROR) )
            {
                reportError( ut, cell );
            }
            else if( ur_is(cell, UT_WORD) )
            {
                UAtom atom = ur_atom(cell);
                if( atom == UR_ATOM_QUIT || atom == UR_ATOM_HALT )
                {
                    // Call finish to close the application window.  We must
                    // immediately call boron_freeEnvGL to free any Boron-GL
                    // datatypes before the window (and the GL context)
                    // actually goes away.
                    ANativeActivity_finish( app->activity );
                }
                else
                    LOGE( "unhandled exception %s\n", ur_atomCStr(ut, atom) );
            }
        }
    }

    boron_freeEnvGL( ut );
#endif
}


//EOF
