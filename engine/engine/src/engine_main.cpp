#include <dlib/dlib.h>
#include <dlib/socket.h>
#include <dlib/dns.h>
#include <dlib/memprofile.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/thread.h>
#include <graphics/graphics.h>
#include <crash/crash.h>

#include "engine.h"
#include "engine_version.h"

int engine_main(int argc, char *argv[])
{
    dmThread::SetThreadName(dmThread::GetCurrentThread(), "engine_main");

#if DM_RELEASE
    dLib::SetDebugMode(false);
#endif
    dmHashEnableReverseHash(dLib::IsDebugMode());

    dmCrash::Init(dmEngineVersion::VERSION, dmEngineVersion::VERSION_SHA1);
    dmDDF::RegisterAllTypes();
    dmSocket::Initialize();
    dmDNS::Initialize();
    dmMemProfile::Initialize();
    dmProfile::Initialize(256, 1024 * 16, 128);
    dmLogParams params;
    dmLogInitialize(&params);

    // Delay initialize the window and graphics context creation until the app has started
#if defined(__MACH__) && ( defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR) )
    int exit_code = 0;
    dmGraphics::AppBootstrap(argc, argv,
                (dmGraphics::EngineCreate) dmEngineCreate,
                (dmGraphics::EngineDestroy) dmEngineDestroy,
                (dmGraphics::EngineUpdate) dmEngineUpdate,
                (dmGraphics::EngineGetResult) dmEngineGetResult);
#else
    if (!dmGraphics::Initialize())
    {
        dmLogError("Could not initialize graphics.");
        return 0x0;
    }

    int exit_code = dmEngine::Launch(argc, argv, 0, 0, 0);
#endif

    dmGraphics::Finalize();
    dmLogFinalize();
    dmProfile::Finalize();
    dmMemProfile::Finalize();
    dmDNS::Finalize();
    dmSocket::Finalize();
    return exit_code;
}
