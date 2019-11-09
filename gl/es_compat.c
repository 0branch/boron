/*
    OpenGL matrix compatability routines for GLES.
*/


#include <assert.h>
#include <string.h>
#include "glh.h"
#include "math3d.h"


#define DEG2RAD         (UR_PI / 180.0f)
#define STACK_DEPTH     6
#define viewStackEnd  (viewStack + 16 * STACK_DEPTH)

static GLfloat  projection[ 16 ];
static GLfloat  viewStack[ 16 * STACK_DEPTH ];
static GLfloat* viewTop = viewStack;
static GLenum _mode = GL_MODELVIEW;
GLfloat* matrixTop = viewStack;
char  es_matrixUsed = 0;
char  es_matrixMod = 0;


#if 0
#include <stdio.h>
#define DUMP(lab,mat)   dumpMatrix(lab, mat)
void dumpMatrix( const char* label, const float* mat )
{
    fprintf( stderr, "%s\n    %f,%f,%f\n    %f,%f,%f\n"
                         "    %f,%f,%f\n    %f,%f,%f\n",
             label,
             mat[0], mat[1], mat[2],
             mat[4], mat[5], mat[6],
             mat[8], mat[9], mat[10],
             mat[12], mat[13], mat[14] );
}
#else
#define DUMP(lab,mat)
#endif


void es_updateUniformMatrix()
{
    GLfloat mat[16];

    es_matrixMod = 0;

    // mat = projection * view * model
    ur_matrixMult( projection, matrixTop, mat );

    glUniformMatrix4fv( 0 /*ULOC_TRANSFORM*/, 1, GL_FALSE, mat );
    DUMP( "transform", mat );

    if( es_matrixUsed > 1 )
    {
        DUMP( "modelView", matrixTop );
        glUniformMatrix4fv( 2 /*ULOC_MODELVIEW*/, 1, GL_FALSE, matrixTop );
    }
}


/*
  Set projection*view & view matrices for cases where model matrix is
  provided by other means (e.g. instanced draws with matrices in vertex
  attributes).
*/
void es_updateUniformMatrixView()
{
    GLfloat mat[16];

    es_matrixMod = 0;

    // mat = projection * view
    ur_matrixMult( projection, viewStack, mat );

    glUniformMatrix4fv( 0 /*ULOC_TRANSFORM*/, 1, GL_FALSE, mat );
    DUMP( "projectionView", mat );

    if( es_matrixUsed > 1 )
    {
        glUniformMatrix4fv( 2 /*ULOC_MODELVIEW*/, 1, GL_FALSE, viewStack );
        DUMP( "view", mat );
    }
}


void esMatrixMode( GLenum mode )
{
    if( mode == GL_MODELVIEW )
    {
        matrixTop = viewTop;
        _mode = mode;
    }
    else if( mode == GL_PROJECTION )
    {
        if( _mode == GL_MODELVIEW )
            viewTop = matrixTop;
        matrixTop = projection;
        _mode = mode;
    }
}


void esLoadIdentity()
{
    ur_loadIdentity( matrixTop );
    es_matrixMod = 1;
}


void esLoadMatrixf( const GLfloat* mat )
{
    memcpy( matrixTop, mat, 16 * sizeof(GLfloat) );
    es_matrixMod = 1;
}


void esMultMatrixf( const GLfloat* mat )
{
    ur_matrixMult( matrixTop, mat, matrixTop );
    es_matrixMod = 1;
}


// Combines glPushMatrix followed by glMultMatrixf.
void es_pushMultMatrix( const float* mat )
{
    assert( _mode == GL_MODELVIEW );
    if( matrixTop != viewStackEnd )
    {
        GLfloat* prev = matrixTop;
        matrixTop += 16;
        ur_matrixMult( prev, mat, matrixTop );
        es_matrixMod = 1;
    }
}


void esPopMatrix()
{
    if( _mode == GL_MODELVIEW && matrixTop != viewStack )
    {
        matrixTop -= 16;
        es_matrixMod = 1;
    }
}


void esPushMatrix()
{
    if( _mode == GL_MODELVIEW && matrixTop != viewStackEnd )
    {
        GLfloat* prev = matrixTop;
        matrixTop += 16;
        memcpy( matrixTop, prev, 16 * sizeof(GLfloat) );
    }
}


void esRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z )
{
    float mat[16];
    float axis[3]; 

    if( angle == 0.0f )
        return;

    axis[0] = x;
    axis[1] = y;
    axis[2] = z;

    ur_loadRotation( mat, axis, angle * DEG2RAD );
    ur_matrixMult( matrixTop, mat, matrixTop );
    es_matrixMod = 1;
    // dumpMatrix( "rot:", matrixTop );
}


void esScalef( GLfloat x, GLfloat y, GLfloat z )
{
    GLfloat* m = matrixTop;

    m[0] *= x;   
    m[1] *= x;   
    m[2] *= x;   
    m[3] *= x;   

    m[4] *= y;   
    m[5] *= y;   
    m[6] *= y;   
    m[7] *= y;   

    m[8]  *= z;
    m[9]  *= z;
    m[10] *= z;
    m[11] *= z;

    es_matrixMod = 1;
}


void esTranslatef( GLfloat x, GLfloat y, GLfloat z )
{
    GLfloat* m = matrixTop;

    m[12] = m[0] * x + m[4] * y + m[8]  * z + m[12];
    m[13] = m[1] * x + m[5] * y + m[9]  * z + m[13];
    m[14] = m[2] * x + m[6] * y + m[10] * z + m[14];
    m[15] = m[3] * x + m[7] * y + m[11] * z + m[15];

    es_matrixMod = 1;
}


void esOrtho( GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
              GLfloat near_val, GLfloat far_val )
{
    GLfloat mat[16];

    if( left == right || bottom == top || near_val == far_val )
        return;

#define M(row,col)  mat[col * 4 + row]

    M(0,0) = 2.0f / (right - left);
    M(0,1) = 0.0f;
    M(0,2) = 0.0f;
    M(0,3) = -(right + left) / (right - left);

    M(1,0) = 0.0f;
    M(1,1) = 2.0f / (top - bottom);
    M(1,2) = 0.0f;
    M(1,3) = -(top + bottom) / (top - bottom);

    M(2,0) = 0.0f;
    M(2,1) = 0.0f;
    M(2,2) = -2.0f / (far_val - near_val);
    M(2,3) = -(far_val + near_val) / (far_val - near_val);

    M(3,0) = 0.0f;
    M(3,1) = 0.0f;
    M(3,2) = 0.0f;
    M(3,3) = 1.0f;

    ur_matrixMult( matrixTop, mat, matrixTop );
}


//EOF
