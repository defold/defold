#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{

    void WriteDump()
    {
        WriteCrash(g_FilePath, &g_AppState);
    }
    
    void SetCrashFilename(const char*)
    {
    }

    void PlatformPurge()
    {
    }
    
    void InstallHandler()
    {
    }

}
