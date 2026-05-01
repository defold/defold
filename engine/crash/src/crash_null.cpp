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

#include <stdio.h>
#include "crash.h"
#include "crash_private.h"

namespace dmCrash
{
    bool IsInitialized()
    {
        return false;
    }

    void Init(const char* version, const char* hash)
    {
        (void)version;
        (void)hash;
    }

    bool IsEnabled()
    {
        return false;
    }

    void SetEnabled(bool enable)
    {
        (void)enable;
    }

    void SetExtraInfoCallback(FCallstackExtraInfoCallback cbk, void* ctx)
    {
        (void)cbk;
        (void)ctx;
    }

    void SetFilePath(const char* filepath)
    {
        (void)filepath;
    }

    const char* GetFilePath()
    {
        return 0;
    }

    AppState* GetAppState()
    {
        return 0;
    }

    Result SetUserField(uint32_t index, const char* value)
    {
        (void)index;
        (void)value;
        return RESULT_OK;
    }

    HDump LoadPreviousPath(const char* where)
    {
        (void)where;
        return 0;
    }

    HDump LoadPrevious()
    {
        return 0;
    }

    void Release(HDump dump)
    {
        (void)dump;
    }

    bool IsValidHandle(HDump dump)
    {
        (void)dump;
        return false;
    }

    const char* GetExtraData(HDump dump)
    {
        (void)dump;
        return 0;
    }

    int GetSignum(HDump dump)
    {
        (void)dump;
        return 0;
    }

    const char* GetSysField(HDump dump, SysField fld)
    {
        (void)dump;
        (void)fld;
        return 0;
    }

    const char* GetUserField(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }

    void Purge()
    {
    }

    uint32_t GetBacktraceAddrCount(HDump dump)
    {
        (void)dump;
        return 0;
    }

    void* GetBacktraceAddr(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }

    const char* GetModuleName(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }

    void* GetModuleAddr(HDump dump, uint32_t index)
    {
        (void)dump;
        (void)index;
        return 0;
    }
}

