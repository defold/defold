#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    void WriteExtra(int fd)
    {

    }

    void WriteDump()
    {
        WriteCrash(g_FilePath, &g_AppState, WriteExtra);
    }

    void InstallHandler()
    {

    }
}
