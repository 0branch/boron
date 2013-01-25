#ifndef ES_COMPAT_H
#define ES_COMPAT_H


#define GL_MODELVIEW    0x1700
#define GL_PROJECTION   0x1701

#define GL_MODELVIEW_MATRIX     0x0BA6
#define GL_PROJECTION_MATRIX    0x0BA7


void glMatrixMode( GLenum mode );
void glLoadIdentity();
void glLoadMatrixf( const GLfloat* mat );
void glMultMatrixf( const GLfloat* mat );
void glPopMatrix();
void glPushMatrix();
void glRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
void glScalef( GLfloat x, GLfloat y, GLfloat z );
void glTranslatef( GLfloat x, GLfloat y, GLfloat z );
void glOrtho( GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
              GLfloat near_val, GLfloat far_val );
const GLubyte* gluErrorString( GLenum error );
void es_updateUniformMatrix();


extern GLfloat* matrixTop;
extern GLint es_matrixLoc;
extern char  es_matrixUsed;
extern char  es_matrixMod;


#endif  //ES_COMPAT_H
