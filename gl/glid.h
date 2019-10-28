#ifndef GLID_H
#define GLID_H


extern void   glid_startup();
extern void   glid_shutdown();
extern void   glid_release();
extern GLuint glid_genTexture();
extern GLuint glid_genRenderbuffer();
extern GLuint glid_genFramebuffer();
extern void   glid_gcBegin();
extern void   glid_gcSweep();
extern void   glid_gcMarkTexture( GLuint name );
extern void   glid_gcMarkRenderbuffer( GLuint name );
extern void   glid_gcMarkFramebuffer( GLuint name );


#endif
