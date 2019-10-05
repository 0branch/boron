/*
    OpenGL matrix compatability routines for GLES.
*/


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
GLint es_matrixLoc = 0;
char  es_matrixUsed = 0;
char  es_matrixMod = 0;


void es_updateUniformMatrix()
{
    GLfloat mat[16];

    es_matrixMod = 0;

    // mat = projection * view * model
    ur_matrixMult( projection, matrixTop, mat );

    glUniformMatrix4fv( es_matrixLoc, 1, GL_FALSE, mat );
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


void esPopMatrix()
{
    if( _mode == GL_MODELVIEW && matrixTop != viewStack )
        matrixTop -= 16;
}


void esPushMatrix()
{
    if( _mode == GL_MODELVIEW && matrixTop != viewStackEnd )
    {
        GLfloat* prev = matrixTop;
        matrixTop += 16;
        memcpy( prev, matrixTop, 16 * sizeof(GLfloat) );
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
    /*
    fprintf( stderr, "rot %f,%f,%f\n    %f,%f,%f\n    %f,%f%,f\n",
            matrixTop[0], matrixTop[1], matrixTop[2],
            matrixTop[4], matrixTop[5], matrixTop[6],
            matrixTop[8], matrixTop[9], matrixTop[10] );
    */
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
