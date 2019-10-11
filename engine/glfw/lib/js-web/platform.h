#ifndef _platform_h_
#define _platform_h_

#include "../../include/GL/glfw.h"

int glfwInitJS( void );

struct {

    GLFWTouch Touch[GLFW_MAX_TOUCH];

} _glfwInput;


#endif // _platform_h_
