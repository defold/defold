#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <graphics/graphics.h>
#include <crash/crash.h>

#include "engine.h"
#include "engine_version.h"

int engine_main(int argc, char *argv[])
{
#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmHashEnableReverseHash(dLib::IsDebugMode());

    dmCrash::Init(dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
    dmDDF::RegisterAllTypes();
    dmSocket::Initialize();
    dmMemProfile::Initialize();
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmLogParams params;
    dmLogInitialize(&params);

    if (!dmGraphics::Initialize())
    {
        dmLogError("Could not initialize graphics.");
        return 0x0;
    }

    int exit_code = dmEngine::Launch(argc, argv, 0, 0, 0);

    dmGraphics::Finalize();
    dmLogFinalize();
    dmProfile::Finalize();
    dmMemProfile::Finalize();
    dmSocket::Finalize();
    return exit_code;
}
