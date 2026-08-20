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

#include "log.h"

#include "dstrings.h"
#include "safe_windows.h"

#if !defined(_GAMING_XBOX)
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#endif
#include <stdio.h>

namespace dmLog
{

#if !defined(_GAMING_XBOX)
static HANDLE g_DebugOutputReadPipe = INVALID_HANDLE_VALUE;

static DWORD WINAPI DebugOutputThread(void* context)
{
    HANDLE read_pipe = (HANDLE) context;
    char buffer[1024 + 1];
    DWORD bytes_read = 0;

    while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, 0) && bytes_read != 0)
    {
        buffer[bytes_read] = 0;
        OutputDebugStringA(buffer);
    }

    CloseHandle(read_pipe);
    return 0;
}

static void RedirectStdIOToDebugOutput()
{
    if (g_DebugOutputReadPipe != INVALID_HANDLE_VALUE)
        return;

    HANDLE read_pipe = INVALID_HANDLE_VALUE;
    HANDLE write_pipe = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&read_pipe, &write_pipe, 0, 0))
        return;

    HANDLE thread = CreateThread(0, 0, DebugOutputThread, read_pipe, 0, 0);
    if (thread == NULL)
    {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return;
    }
    CloseHandle(thread);

    int write_fd = _open_osfhandle((intptr_t) write_pipe, _O_TEXT);
    if (write_fd == -1)
    {
        CloseHandle(write_pipe);
        return;
    }

    fflush(stdout);
    fflush(stderr);
    _dup2(write_fd, _fileno(stdout));
    _dup2(write_fd, _fileno(stderr));
    _close(write_fd);

    setvbuf(stdout, 0, _IONBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);

    g_DebugOutputReadPipe = read_pipe;
}
#endif

void DoLogPlatform(LogSeverity severity, const char* output, int output_len)
{
#if defined(DM_LOG_TO_DEBUGGER)
    (void) severity;
    (void) output_len;
    OutputDebugStringA(output);
#else
    FILE* stream = (severity == LOG_SEVERITY_ERROR || severity == LOG_SEVERITY_FATAL) ? stderr : stdout;
    fwrite(output, 1, output_len, stream);
#endif
}

void CloseConsoleWindow()
{
#if !defined(_GAMING_XBOX)
    if (!IsDebuggerPresent())
        return;

    RedirectStdIOToDebugOutput();

    HWND console_window = GetConsoleWindow();
    if (console_window == NULL)
        return;

    ShowWindow(console_window, SW_HIDE);
    FreeConsole();
#endif
}

bool HResultToString(HRESULT hr, char* buffer, size_t buffer_size)
{
    buffer[0] = 0;
    return 0 != FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, hr,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                    (LPSTR) buffer, (DWORD) buffer_size,
                    NULL);
}

void LogHResult(LogSeverity severity, HRESULT result, const char* str_buf)
{
    char msg[256];
    char buffer[1024];
    dmLog::HResultToString(result, msg, sizeof(msg));
    dmSnPrintf(buffer, sizeof(buffer), "%s (hr: 0x%08x code: %d : '%s')\n", str_buf, result, HRESULT_CODE(result), msg);
    dmLogError(buffer);
    OutputDebugStringA(buffer);
}

} // namespace dmLog
