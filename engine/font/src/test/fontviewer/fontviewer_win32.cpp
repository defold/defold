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

#include "fontviewer_macos.h"
#include <platform/platform_window_win32.h>
#include <limits.h>
#include <dlib/array.h>

int FontViewerMain(int argc, char** argv);

int wmain(int argc, wchar_t** argv)
{
    dmArray<char*> arguments;
    arguments.SetCapacity(argc + 1);
    dmArray<char> utf8;
    uint32_t bytes = 0;
    for (int i = 0; i < argc; ++i)
    {
        bytes += WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, 0, 0, 0, 0);
    }

    utf8.SetCapacity(bytes);
    char* next = utf8.Begin();
    for (int i = 0; i < argc; ++i)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, next, bytes, 0, 0);
        if (!size)
        {
            return 1;
        }

        arguments.Push(next);
        next += size;
        bytes -= size;
    }

    arguments.Push(0);
    return FontViewerMain(argc, arguments.Begin());
}

void FontViewerMacOSSetClipboard(HWindow window, const char* text, uint32_t text_length)
{
    if (text_length > INT_MAX)
    {
        return;
    }

    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, (int)text_length, 0, 0);
    if (text_length && !count)
    {
        return;
    }

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, ((SIZE_T)count + 1) * sizeof(WCHAR));
    WCHAR* wide = memory ? (WCHAR*)GlobalLock(memory) : 0;
    if (wide)
    {
        if (count)
        {
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, (int)text_length, wide, count);
        }

        wide[count] = 0;
        GlobalUnlock(memory);
        if (OpenClipboard(dmPlatform::GetWindowsHWND(window)))
        {
            if (EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory))
            {
                memory = 0; // The clipboard owns the allocation.
            }

            CloseClipboard();
        }
    }

    if (memory)
    {
        GlobalFree(memory);
    }
}

void FontViewerMacOSSetLinkCursor(HWindow window, bool link_cursor)
{
    HWND hwnd = dmPlatform::GetWindowsHWND(window);
    HCURSOR cursor = LoadCursorW(0, MAKEINTRESOURCEW(link_cursor ? 32649 : 32512)); // IDC_HAND or IDC_ARROW
    SetClassLongPtrW(hwnd, GCLP_HCURSOR, (LONG_PTR)cursor);
    POINT pos;
    if (GetCursorPos(&pos) && WindowFromPoint(pos) == hwnd && SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(pos.x, pos.y)) == HTCLIENT)
    {
        SetCursor(cursor);
    }
}

void FontViewerMacOSDestroyLinkCursor(HWindow window)
{
    FontViewerMacOSSetLinkCursor(window, false);
}
