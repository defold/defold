
#include "internal.h"


int _glfwPlatformGetJoystickParam( int joy, int param )
{
	if (joy >= 0 && joy < HID_MAX_GAMEPAD_COUNT) {
	    switch(param) {
	    	case GLFW_PRESENT:
	    		return tvosJoystick[joy].present;
	    	case GLFW_AXES:
	    		return tvosJoystick[joy].numAxes;
	    	case GLFW_BUTTONS:
	    		return tvosJoystick[joy].numButtons;
	    }
	}
    return 0;
}

int _glfwPlatformGetJoystickPos( int joy, float *pos, int numaxes )
{
	if (joy >= 0 && joy < HID_MAX_GAMEPAD_COUNT) {
		for(int i=0; i<MIN(tvosJoystick[joy].numAxes, numaxes); ++i) {
			pos[i] = tvosJoystick[joy].axes[i];
		}	
	}
    return 0;
}

int _glfwPlatformGetJoystickButtons( int joy, unsigned char *buttons, int numbuttons )
{
	if (joy >= 0 && joy < HID_MAX_GAMEPAD_COUNT) {
		for(int i=0; i<MIN(tvosJoystick[joy].numButtons, numbuttons); ++i) {
			buttons[i] = tvosJoystick[joy].buttons[i];
		}	
	    return 0;
	}
    return 0;
}

int _glfwPlatformGetJoystickDeviceId( int joy, char** device_id )
{
	if (joy >= 0 && joy < HID_MAX_GAMEPAD_COUNT) {
		if (tvosJoystick[joy].present == 1) {
		    *device_id = "AppleTV Remote";
		    return GL_TRUE;
		}
		return GL_FALSE;
	}
}
