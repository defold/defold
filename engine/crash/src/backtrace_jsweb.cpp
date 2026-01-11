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

#include "crash.h"
#include "crash_private.h"
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/dlib.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <unistd.h>

static bool g_CrashDumpEnabled = true;
static dmCrash::FCallstackExtraInfoCallback g_CrashExtraInfoCallback = 0;
static void*                                g_CrashExtraInfoCallbackCtx = 0;

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

void dmCrash::HandlerSetExtraInfoCallback(dmCrash::FCallstackExtraInfoCallback cbk, void* ctx)
{
    g_CrashExtraInfoCallback = cbk;
    g_CrashExtraInfoCallbackCtx = ctx;
}

extern "C" void JSWriteDump(char* json_stacktrace) {
    if (!g_CrashDumpEnabled)
        return;

    dmCrash::AppState* state = dmCrash::GetAppState();

    state->m_PtrCount = 0;
    state->m_Signum = 0xDEAD;
    int length = strlen(json_stacktrace);
    uint32_t len = dmMath::Min((dmCrash::AppState::EXTRA_MAX - 1), (uint32_t)length);
    strncpy(state->m_Extra, json_stacktrace, len);
    if (g_CrashExtraInfoCallback)
    {
        int extra_len = strlen(state->m_Extra);
        g_CrashExtraInfoCallback(g_CrashExtraInfoCallbackCtx, state->m_Extra + extra_len, dmCrash::AppState::EXTRA_MAX - extra_len - 1);
    }
    dmCrash::WriteCrash(dmCrash::GetFilePath(), state);

    // It's more convenient to get message as one error for web, so we don't use dmCrash::LogCallstack()
    bool is_debug_mode = dLib::IsDebugMode();
    dLib::SetDebugMode(true);
    dmLogError("CALL STACK:\n%s\nCALL STACK END", state->m_Extra);
    dLib::SetDebugMode(is_debug_mode);
}
