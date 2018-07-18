#include "extension.h"
#include <graphics/glfw/glfw.h>

namespace dmExtension
{

    void RegisterOnActivityResultListener(OnActivityResult listener)
    {
        glfwRegisterOnActivityResultListener(listener);
    }

    void UnregisterOnActivityResultListener(OnActivityResult listener)
    {
        glfwUnregisterOnActivityResultListener(listener);
    }

}
