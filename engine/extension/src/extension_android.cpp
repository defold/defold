#include "extension.h"
#include <graphics/glfw/glfw.h>

namespace dmExtension
{
    void RegisterAndroidOnActivityResultListener(OnActivityResult listener)
    {
        glfwRegisterOnActivityResultListener(listener);
    }

    void UnregisterAndroidOnActivityResultListener(OnActivityResult listener)
    {
        glfwUnregisterOnActivityResultListener(listener);
    }
}
