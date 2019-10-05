#ifdef __APPLE__

#include <OpenGL/gl.h>

#elif defined(__ANDROID__)

#include <GLES3/gl31.h>

#elif defined(_WIN32)

//#include <GL/gl.h>
#include <GL/glew.h>

#else

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#endif

#include "es_compat.h"
