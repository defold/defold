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

#include <Windows.h>
#include <dlib/dlib.h>
#include <dlib/log.h>

namespace dmCrash
{
    void WriteCrash(const char* file_name, AppState* data)
    {
        bool is_debug_mode = dLib::IsDebugMode();
        dLib::SetDebugMode(true);
        HANDLE fhandle = CreateFileA(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fhandle != NULL)
        {
            DWORD written;
            AppStateHeader header;
            header.m_Version = AppState::VERSION;
            header.m_StructSize = sizeof(AppState);

            if (WriteFile(fhandle, &header, sizeof(AppStateHeader), &written, 0))
            {
                if (WriteFile(fhandle, data, sizeof(AppState), &written, 0))
                {
                    dmLogInfo("Successfully wrote Crashdump to file: %s", file_name);
                    CloseHandle(fhandle);
                }
                else
                {
                    dmLogError("Failed to write Crashdump content.");
                    CloseHandle(fhandle);
                }
            }
            else
            {
                dmLogError("Failed to write Crashdump header.");
                CloseHandle(fhandle);
            }
        }
        else
        {
            dmLogError("Failed to write Crashdump file.");
        }
        dLib::SetDebugMode(is_debug_mode);
    }
}
