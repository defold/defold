#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <graphics/glfw/glfw.h>
#include <crash/crash.h>

#include "engine.h"
#include "engine_version.h"

int main(int argc, char *argv[])
{
#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmCrash::Init(dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
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
