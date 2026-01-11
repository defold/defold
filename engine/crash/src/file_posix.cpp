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

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlib/dlib.h>
#include <dlib/log.h>

#include <stdio.h>

namespace dmCrash
{
    void WriteCrash(const char* file_name, AppState* data)
    {
        bool is_debug_mode = dLib::IsDebugMode();
        dLib::SetDebugMode(true);
        int fhandle = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fhandle != -1)
        {
            AppStateHeader header;
            header.m_Version = AppState::VERSION;
            header.m_StructSize = sizeof(AppState);

            if (write(fhandle, &header, sizeof(AppStateHeader)) == sizeof(AppStateHeader))
            {
                if (write(fhandle, data, sizeof(AppState)) == sizeof(AppState))
                {
                    dmLogInfo("Successfully wrote Crashdump to file: %s", file_name);
                    close(fhandle);
                }
                else
                {
                    dmLogError("Failed to write Crashdump content.");
                    close(fhandle);
                    unlink(file_name);
                }
            }
            else
            {
                dmLogError("Failed to write Crashdump header.");
                close(fhandle);
                unlink(file_name);
            }
        }
        else
        {
            dmLogError("Failed to write Crashdump file.");
        }
        dLib::SetDebugMode(is_debug_mode);
    }
}
