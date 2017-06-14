#include "crash.h"
#include "crash_private.h"
#include <dlib/log.h>
#include <dlib/json.h>
#include <dlib/math.h>

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <unistd.h>

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

extern "C" void JSWriteDump(char* json_stacktrace) {
    dmCrash::g_AppState.m_PtrCount = 0;
    dmCrash::g_AppState.m_Signum = 0xDEAD;
    dmJson::Document doc = { 0 };
    if (dmJson::Parse(json_stacktrace, &doc) == dmJson::RESULT_OK)
    {
        uint32_t len = dmMath::Min(dmCrash::AppState::EXTRA_MAX - 1, strlen(json_stacktrace));
        strncpy(dmCrash::g_AppState.m_Extra, json_stacktrace, len);
        dmCrash::WriteCrash(dmCrash::g_FilePath, &dmCrash::g_AppState);
        dmJson::Free(&doc);
    }
}