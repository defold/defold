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
    // WriteDump is void for js-web, JSWriteDump is used instead.
}

void dmCrash::InstallHandler()
{
    // Crash handler is installed directly via JavaScript.
    // window.onerror = crash_handler;
}

static char json_buffer[dmCrash::AppState::EXTRA_MAX];

extern "C" void JSWriteExtra(int fd) {
    write(fd, json_buffer, strlen(json_buffer));
}

extern "C" void JSWriteDump(char* message, char* json_stacktrace) {
    dmCrash::g_AppState.m_PtrCount = 0;
    dmCrash::g_AppState.m_Signum = 0xDEAD;

    dmJson::Document doc = { 0 };
    memset(json_buffer, 0x0, dmCrash::AppState::EXTRA_MAX);
    if (dmJson::Parse(json_stacktrace, &doc) == dmJson::RESULT_OK)
    {
        uint32_t len = dmMath::Min(dmCrash::AppState::EXTRA_MAX - 1, strlen(json_stacktrace));
        strncpy(json_buffer, json_stacktrace, len);
        dmCrash::WriteCrash(dmCrash::g_FilePath, &dmCrash::g_AppState, JSWriteExtra);

        memset(json_buffer, 0x0, dmCrash::AppState::EXTRA_MAX);
        dmJson::Free(&doc);
    }
}