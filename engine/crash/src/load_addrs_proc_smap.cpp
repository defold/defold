#include <dlfcn.h>
#include <link.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "crash_private.h"

namespace dmCrash
{
    void SetLoadAddrs(AppState* state)
    {
        FILE *fp = fopen("/proc/self/smaps", "rt");
        if (!fp)
        {
            dmLogWarning("Could not read /proc/self/smaps");
            return;
        }

        int count = 0;
        char line[1024];
        while (fgets(line, sizeof(line), fp))
        {
            // example line, extract address and name of all module lines
            //
            // b6f54000-b6f55000 rw-p 00001000 b3:17 3478       /system/lib/libsigchain.so
            int len = strlen(line);
            int addr_break = len;
            int perm_start = len;
            int name_start = len;
            for (int i=0;i<len;i++)
            {
                if (addr_break == len)
                {
                    if (line[i] == '-')
                        addr_break = i;
                }
                else if (perm_start == len)
                {
                    if (line[i] == ' ')
                        perm_start = i+1;
                }
                else
                {
                    // pick the last /
                    if (line[i] == '/')
                        name_start = i + 1;
                }

                if (line[i] == '\n')
                    line[i] = 0;
            }

            if (addr_break < len && perm_start < len && name_start < len)
            {
                bool is_exec = false;
                for (int p=perm_start;p<(perm_start+4) && p<len;p++)
                {
                    if (line[p] == 'x')
                    {
                        is_exec = true;
                    }
                }

                if (!is_exec)
                {
                    continue;
                }

                if (count == AppState::MODULES_MAX)
                {
                    dmLogWarning("Number of modules exceeds capacity");
                    break;
                }

                line[addr_break] = 0;
                state->m_ModuleAddr[count] = (void*)strtoull(line, 0, 16);
                dmStrlCpy(state->m_ModuleName[count], &line[name_start], AppState::MODULE_NAME_SIZE);
                ++count;
            }
        }

        fclose(fp);
    }
}
