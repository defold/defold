// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <dlfcn.h>
#include <link.h>
#include <stdlib.h>
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
                    break;
                }

                line[addr_break] = 0;
                state->m_ModuleAddr[count] = (void*)strtoull(line, 0, 16);
                state->m_ModuleSize[count] = 0;
                dmStrlCpy(state->m_ModuleName[count], &line[name_start], AppState::MODULE_NAME_SIZE);
                ++count;
            }
        }

        fclose(fp);
        state->m_ModuleCount = count;
    }
}
