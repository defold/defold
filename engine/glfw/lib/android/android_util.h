#ifndef _ANDROID_UTIL_H_
#define _ANDROID_UTIL_H_

#include "internal.h"

void init_gl(EGLDisplay* out_display, EGLContext* out_context, EGLConfig* out_config);

void final_gl(_GLFWwin* win);

void create_gl_surface(_GLFWwin* win);

void destroy_gl_surface(_GLFWwin* win);

#endif // _ANDROID_UTIL_H_
