#include "crash_private.h"

#include <mach-o/dyld.h>
#include <dlib/dstrings.h>

namespace dmCrash
{
    void SetLoadAddrs(AppState* state)
    {
        // Structure is all 0 to start with, so just fill everything in.
        for (uint32_t i=0;i!=AppState::MODULES_MAX;i++)
        {
            const struct mach_header *hdr = _dyld_get_image_header(i);
            if (!hdr)
            {
                break;
            }

            const char *name = _dyld_get_image_name(i);
            const char *p = strrchr(name, '/');
            dmStrlCpy(state->m_ModuleName[i], p ? (p+1) : name, sizeof(state->m_ModuleName));
            state->m_ModuleAddr[i] = (void*)hdr;
        }
    }
}
