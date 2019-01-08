#include "extension.h"
#include <graphics/glfw/glfw.h>

namespace dmExtension
{

    void RegisteriOSUIApplicationDelegate(void* delegate)
    {
        glfwRegisterUIApplicationDelegate(delegate);
    }

    void UnregisteriOSUIApplicationDelegate(void* delegate)
    {
        glfwUnregisterUIApplicationDelegate(delegate);
    }

}

