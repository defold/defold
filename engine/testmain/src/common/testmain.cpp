#if !defined(DM_PLATFORM_VENDOR)

#include "testmain.h"


extern "C" bool TestMainPlatformInit()
{
    return true;
}

extern "C" int TestMainIsDebuggerAttached()
{
    return 0;
}

#endif
