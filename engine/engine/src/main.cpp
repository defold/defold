#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <graphics/glfw/glfw.h>
#include "engine.h"

int main(int argc, char *argv[])
{
#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmDDF::RegisterAllTypes();
    dmSocket::Initialize();
    dmMemProfile::Initialize();
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmLogParams params;
    dmLogInitialize(&params);

    // NOTE: We do glfwInit as glfw doesn't cleanup menus properly on OSX.
    if (glfwInit() == GL_FALSE)
    {
        dmLogError("Could not initialize glfw.");
        return 0x0;
    }

    int exit_code = dmEngine::Launch(argc, argv, 0, 0, 0);

    glfwTerminate();

    dmLogFinalize();
    dmProfile::Finalize();
    dmMemProfile::Finalize();
    dmSocket::Finalize();
    return exit_code;
}
