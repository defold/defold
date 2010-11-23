#include <dlib/socket.h>
#include "engine.h"

#ifdef __MACH__
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

#include <dlib/memprofile.h>

int main(int argc, char *argv[])
{
    // TODO: This is required to export the symbol dmMemProfileInternalData from dlib
    // due to the missing flag -export-dynamic on darwin
    // Better solution? Some flag perhaps?
    dmMemProfile::Stats stats;
    dmMemProfile::GetStats(&stats);

    dmSocket::Initialize();

    dmEngine::HEngine engine = dmEngine::New();
    int32_t exit_code = 1;
    if (dmEngine::Init(engine, argc, argv))
    {
#ifdef __MACH__
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

    return exit_code;
}
