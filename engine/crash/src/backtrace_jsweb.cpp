// Copyright 2020 The Defold Foundation
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

#include "crash.h"
#include "crash_private.h"
#include <dlib/log.h>
#include <dmsdk/dlib/json.h>
#include <dlib/math.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <unistd.h>

static bool g_CrashDumpEnabled = true;

void dmCrash::WriteDump()
{
    // WriteDump is void for js-web, see JSWriteDump.
}

void dmCrash::SetCrashFilename(const char*)
{
}

void dmCrash::PlatformPurge()
{
}

void dmCrash::InstallHandler()
{
    // window.onerror is set in dmloader.js.
}

void dmCrash::EnableHandler(bool enable)
{
    g_CrashDumpEnabled = enable;
}

extern "C" void JSWriteDump(char* json_stacktrace) {
    if (!g_CrashDumpEnabled)
        return;
    dmCrash::g_AppState.m_PtrCount = 0;
    dmCrash::g_AppState.m_Signum = 0xDEAD;
    dmJson::Document doc = { 0 };
    if (dmJson::Parse(json_stacktrace, &doc) == dmJson::RESULT_OK)
    {
        uint32_t len = dmMath::Min((size_t)(dmCrash::AppState::EXTRA_MAX - 1), strlen(json_stacktrace));
        strncpy(dmCrash::g_AppState.m_Extra, json_stacktrace, len);
        dmCrash::WriteCrash(dmCrash::g_FilePath, &dmCrash::g_AppState);
        dmJson::Free(&doc);
    }
}
