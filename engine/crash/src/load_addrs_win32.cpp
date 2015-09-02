#include "crash_private.h"

#include <dlib/log.h>
#include <windows.h>
#include <psapi.h>

#if PSAPI_VERSION=1
#pragma comment(lib, "psapi.lib")
#endif

namespace dmCrash
{
    void SetLoadAddrs(AppState* state)
    {
        HANDLE process = GetCurrentProcess();
        HMODULE mods[AppState::MODULES_MAX];
        DWORD needed;

        if (EnumProcessModules(process, mods, sizeof(mods), &needed))
        {
            uint32_t count = needed / sizeof(HANDLE);
            for (uint32_t i=0;i!=count;i++)
            {
                if (!GetModuleFileNameExA(process, mods[i], state->m_ModuleName[i], AppState::MODULE_NAME_SIZE))
                {
                    dmLogWarning("Failed to read name of module");
                }
                MODULEINFO info;
                if (GetModuleInformation(process, mods[i], &info, sizeof(info)))
                {
                    state->m_ModuleAddr[i] = info.lpBaseOfDll;
                }
            }
        }
        else
        {
            dmLogWarning("Failed to enumerate process modules");
        }
    }
}
