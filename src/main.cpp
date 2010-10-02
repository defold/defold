#include "engine.h"

#ifdef __MACH__
// Potential name clash with ddf. If included before ddf/ddf.h (TYPE_BOOL)
#include <Carbon/Carbon.h>
#endif

int main(int argc, char *argv[])
{
#ifdef __MACH__
    ProcessSerialNumber psn;
    OSErr err;

    // Move window to front. Required if running without application bundle.
    err = GetCurrentProcess( &psn );
    if (err == noErr)
        (void) SetFrontProcess( &psn );
#endif

    dmEngine::HEngine engine = dmEngine::New();
    int32_t exit_code = 1;
    if (dmEngine::Init(engine, argc, argv))
    {
        exit_code = dmEngine::Run(engine);
    }

    dmEngine::Delete(engine);

    return exit_code;
}
