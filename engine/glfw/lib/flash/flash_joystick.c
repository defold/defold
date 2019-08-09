#include "internal.h"

int _glfwPlatformGetJoystickParam( int joy, int param )
{
    return 0;
}
int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
    return 0;
}
int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
    return 0;
}
int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
    (void) joy;
    (void) device_id;
    return GL_FALSE;
}
int _glfwPlatformGetJoystickHats( int joy, unsigned char *hats, int numhats )
{
    return 0;
}
