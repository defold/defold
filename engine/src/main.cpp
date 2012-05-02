#include <dlib/socket.h>
#include "engine.h"

#if defined(__MACH__) and !defined(__arm__)
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

#include <dlib/memprofile.h>

int main(int argc, char *argv[])
{
    dmDDF::RegisterAllTypes();
    dmMemProfile::Initialize();

    dmSocket::Initialize();

    dmEngine::HEngine engine = dmEngine::New();
    int32_t exit_code = 1;
    if (dmEngine::Init(engine, argc, argv))
    {
#if defined(__MACH__) and !defined(__arm__)
        ProcessSerialNumber psn;
        OSErr err;

        // Move window to front. Required if running without application bundle.
        err = GetCurrentProcess( &psn );
        if (err == noErr)
            (void) SetFrontProcess( &psn );
#endif

        exit_code = dmEngine::Run(engine);
    }

    dmEngine::Delete(engine);
    dmSocket::Finalize();
    dmMemProfile::Finalize();

    return exit_code;
}
