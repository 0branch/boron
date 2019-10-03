#ifdef __APPLE__

#include <OpenGL/gl.h>

#elif defined(__ANDROID__)

#include <GLES3/gl31.h>
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glBindFramebufferEXT        glBindFramebuffer
#define glCheckFramebufferStatusEXT glCheckFramebufferStatus
#define glFramebufferTexture2DEXT   glFramebufferTexture2D
#define glBindRenderbufferEXT       glBindRenderbuffer
#define glRenderbufferStorageEXT    glRenderbufferStorage
#define glFramebufferRenderbufferEXT glFramebufferRenderbuffer
#define GL_DEPTH_ATTACHMENT_EXT     GL_DEPTH_ATTACHMENT
#define GL_FRAMEBUFFER_EXT          GL_FRAMEBUFFER
#define GL_RENDERBUFFER_EXT         GL_RENDERBUFFER
#define GL_COLOR_ATTACHMENT0_EXT    GL_COLOR_ATTACHMENT0
#include "es_compat.h"

#elif defined(_WIN32)

//#include <GL/gl.h>
#include <GL/glew.h>

#else

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#endif
