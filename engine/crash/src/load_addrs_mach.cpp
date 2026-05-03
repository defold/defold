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

#include "crash_private.h"

#include <mach-o/dyld.h>
#include <dlib/dstrings.h>

namespace dmCrash
{
    void SetLoadAddrs(AppState* state)
    {
        // Structure is all 0 to start with, so just fill everything in.
        state->m_ModuleCount = 0;
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
            state->m_ModuleSize[i] = 0;
            state->m_ModuleCount++;
        }
    }
}
