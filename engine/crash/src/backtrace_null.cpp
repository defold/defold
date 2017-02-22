#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{

    void WriteDump()
    {
        WriteCrash(g_FilePath, &g_AppState);
    }

    void InstallHandler(const char* mini_dump_path)
    {
    	(void)mini_dump_path;

    }

}
