#ifndef ES_COMPAT_H
#define ES_COMPAT_H


#ifdef GL_ES_VERSION_2_0
#define GL_MODELVIEW    0x1700
#define GL_PROJECTION   0x1701

#define GL_MODELVIEW_MATRIX     0x0BA6
#define GL_PROJECTION_MATRIX    0x0BA7
#endif


#define glMatrixMode    esMatrixMode
#define glLoadIdentity  esLoadIdentity
#define glLoadMatrixf   esLoadMatrixf
#define glMultMatrixf   esMultMatrixf
#define glPopMatrix     esPopMatrix
#define glPushMatrix    esPushMatrix
#define glRotatef       esRotatef
#define glScalef        esScalef
#define glTranslatef    esTranslatef
#define glOrtho         esOrtho


void esMatrixMode( GLenum mode );
void esLoadIdentity();
void esLoadMatrixf( const GLfloat* mat );
void esMultMatrixf( const GLfloat* mat );
void esPopMatrix();
void esPushMatrix();
void esRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
void esScalef( GLfloat x, GLfloat y, GLfloat z );
void esTranslatef( GLfloat x, GLfloat y, GLfloat z );
void esOrtho( GLfloat left, GLfloat right, GLfloat bottom, GLfloat top,
              GLfloat near_val, GLfloat far_val );
void es_updateUniformMatrix();
void es_updateUniformMatrixView();


extern GLfloat* matrixTop;
extern char  es_matrixUsed;
extern char  es_matrixMod;


#endif  //ES_COMPAT_H
