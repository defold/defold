#include "extension.h"
#include <graphics/glfw/glfw.h>

namespace dmExtension
{

    void RegisterUIApplicationDelegate(void* delegate)
    {
        glfwRegisterUIApplicationDelegate(delegate);
    }

    void UnregisterUIApplicationDelegate(void* delegate)
    {
        glfwUnregisterUIApplicationDelegate(delegate);
    }

}

