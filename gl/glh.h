#ifdef __APPLE__

#include <OpenGL/glu.h>

#elif defined(__ANDROID__)

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>   // glMapBufferOES
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glBindFramebufferEXT        glBindFramebuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glFramebufferTexture2DEXT   glFramebufferTexture2D
#define glMapBuffer                 glMapBufferOES
#define glUnmapBuffer               glUnmapBufferOES
#define GL_DEPTH_COMPONENT24        GL_DEPTH_COMPONENT24_OES
#define GL_DEPTH_ATTACHMENT_EXT     GL_DEPTH_ATTACHMENT
#define GL_FRAMEBUFFER_EXT          GL_FRAMEBUFFER
#define GL_RENDERBUFFER_EXT         GL_RENDERBUFFER
#define GL_WRITE_ONLY               GL_WRITE_ONLY_OES   
#define GL_COLOR_ATTACHMENT0_EXT    GL_COLOR_ATTACHMENT0

#else

#define GL_GLEXT_PROTOTYPES
#include <GL/glu.h>

#endif
