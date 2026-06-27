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

#ifndef DM_SYS_POSIX_H
#define DM_SYS_POSIX_H

#include "sys.h"

namespace dmSysPosix
{
    char*         GetEnv(const char* name);
    dmSys::Result Rename(const char* dst_filename, const char* src_filename);
    dmSys::Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len);
    dmSys::Result GetLogPath(char* path, uint32_t path_len);
    void          FillTimeZone(dmSys::SystemInfo* info);
    bool          ResourceExists(const char* path);
    dmSys::Result ResourceSize(const char* path, uint32_t* resource_size);
    dmSys::Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size);
    dmSys::Result LoadResourcePartial(const char* path, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread);
    dmSys::Result Rmdir(const char* path);
    dmSys::Result Mkdir(const char* path, uint32_t mode);
    dmSys::Result IsDir(const char* path);
    bool          Exists(const char* path);
    dmSys::Result IterateTree(const char* dirpath, bool recursive, bool call_before, void* ctx, void (*callback)(void* ctx, const char* path, bool isdir));
    dmSys::Result Unlink(const char* path);
    dmSys::Result Stat(const char* path, dmSys::StatInfo* stat_info);
    int           StatIsDir(const dmSys::StatInfo* stat_info);
    int           StatIsFile(const dmSys::StatInfo* stat_info);
} // namespace dmSysPosix

#endif // DM_SYS_POSIX_H
