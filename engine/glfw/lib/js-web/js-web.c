#include "../../include/GL/glfw.h"

// NOTE: Direct implementation of missing functionality in emscripten glfw
// We are not linking with input.c, etc

int glfwGetJoystickParam( int joy, int param )
{
    return 0;
}
int glfwGetJoystickPos( int joy, float *pos, int numaxes )
{
    return 0;
}
int glfwGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    return 0;
}
int glfwGetJoystickDeviceId( int joy, char** device_id )
{
    (void) joy;
    (void) device_id;
    return GL_FALSE;
}

GLFWAPI void GLFWAPIENTRY glfwSetTouchCallback( GLFWtouchfun cbfun )
{
}

GLFWAPI int GLFWAPIENTRY glfwGetAcceleration(float* x, float* y, float* z)
{
    return 0;
}

GLFWAPI int GLFWAPIENTRY glfwGetTouch(GLFWTouch* touch, int count, int* out_count)
{
    return 0;
}

