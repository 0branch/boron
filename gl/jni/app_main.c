

#include <unistd.h>
#include <android/log.h>
#include <android/asset_manager.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#if !defined(__LP64__)
#include <time64.h>
#endif
#include <glv.h>
#include <glv_activity.h>
#include <boron/boron-gl.h>
#include <boron/urlan_atoms.h>


#define  LOG_TAG    "libborongl"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define REDIRECT_IO 1

//#define LOCAL_RENDER
#ifdef LOCAL_RENDER
UThread* gUT;
UIndex gUpdate = 0;
#endif


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


static void logBoronError( UThread* ut )
{
    const UCell* ex = ur_exception( ut );
    if( ur_is(ex, UT_ERROR) )
    {
        reportError( ut, ex );
        boron_reset( ut );
    }
}


#ifdef LOCAL_RENDER
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

void renderFrame()
{
    UCell* res;
    UCell* cell;
    int ok;
    static float grey = 0.0f;

    grey += 0.01f;
    if( grey > 1.0f )
        grey = 0.0f;

    memcpy( verts, gTriangleVertices, sizeof(verts) );
    if( gUpdate )
    {
        cell = ur_push( gUT, UT_BLOCK );
        res = cell - 1;
        ur_setSeries( cell, gUpdate, 0 );
        ok = boron_doBlock( gUT, cell, res );
        ur_pop( gUT );
        if( UR_OK == ok )
        {
            verts[0] = res->vec3.xyz[0];
            verts[1] = res->vec3.xyz[1];
        }
        else
        {
            logBoronError( gUT );
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
            break;

        case GLV_EVENT_KEY_DOWN:
            LOGI( "KR key-down %d\n", ev->code );
            break;

        case GLV_EVENT_MOTION:
            LOGI( "KR motion %d,%d\n", ev->x, ev->y );
            break;

        case GLV_EVENT_APP:
            if( ev->code == APP_CMD_STOP )
                *((int*) view->user) = 0;
            break;
    }
}
#endif


#if 0
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
            memcpy( buf, AAsset_getBuffer( as ), len );
            *rlen = len;
        }
    }

    AAsset_close( as );
    return buf;
}


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


void android_main( struct android_app* app )
{
    UCell res;
    UThread* ut;
    GLView* view;
    int state = 1;


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
    view->user = &state;
    glv_setEventHandler( view, eventHandler );
    setupGraphics( view->width, view->height );

    {
    UEnvParameters param;
    gUT = ut = boron_makeEnv( boron_envParam(&param) );
    }
#else
    {
    UEnvParameters param;
    ut = boron_makeEnvGL( boron_envParam(&param) );
    }
    //glEnv.view->user = &state;
    //glv_setEventHandler( glEnv.view, eventHandler );
#endif
    if( ! ut )
    {
        LOGE( "boron_makeEnv failed\n" );
        return;
    }
    ur_freezeEnv( ut );

    {
        int len;
        char* buf = (char*) loadAsset( app, "main.b", &len );
        if( buf )
        {
            //LOGI( "main.b {%s}\n", buf );
            if( ! boron_evalUtf8( ut, buf, len ) )
                logBoronError( ut );
            free( buf );
        }
    }

#ifdef LOCAL_RENDER
    gUpdate = ur_tokenize( ut, _update, _update + sizeof(_update) - 1, &res );
    ur_hold( gUpdate );
    boron_bindDefault( ut, gUpdate );

    while( state && ! app->destroyRequested )
    {
        glv_handleEvents( view );
        renderFrame();
        glv_swapBuffers( view );
    }

    boron_freeEnv( gUT );
    glv_destroy( view );
#else
    {
    UAtom atoms[1];
    UCell* cell;
    UStatus ok;
    ur_internAtoms( ut, "main-update", atoms );

    while( state && ! app->destroyRequested )
    {
        cell = value( ut, atoms[0], UT_BLOCK );
        if( ! cell )
            break;
        ok = boron_doBlock( ut, cell, ur_push(ut, UT_UNSET) );
        ur_pop(ut);
        if( ok == UR_THROW )
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
                    state = 0;
                else
                    LOGE( "unhandled exception %s\n", ur_atomCStr(ut, atom) );
            }
            boron_reset( ut );
        }
    }
    }

    boron_freeEnvGL( ut );
#endif

    LOGI( "KR android_main exiting\n" );
}


//EOF
