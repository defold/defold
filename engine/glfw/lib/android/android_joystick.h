#ifndef _ANDROID_JOYSTICK_H_
#define _ANDROID_JOYSTICK_H_

#define GLFW_ANDROID_GAMEPAD_NUMBUTTONS     19
#define GLFW_ANDROID_GAMEPAD_NUMAXIS        4

#define GLFW_ANDROID_GAMEPAD_DISCONNECTED	0
#define GLFW_ANDROID_GAMEPAD_CONNECTED		1
#define GLFW_ANDROID_GAMEPAD_REFRESHING		2

void glfwAndroidUpdateJoystick(const AInputEvent* event);
void glfwAndroidDiscoverJoysticks();

#endif // _ANDROID_JOYSTICK_H_
