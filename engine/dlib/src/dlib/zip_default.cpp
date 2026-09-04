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

#include "zip_private.h"
#include "sys.h"

namespace dmZip
{

static size_t ReadFileAt(FILE* file, uint64_t offset, void* buffer, size_t size)
{
    if (dmSys::FileSeek64(file, offset) != 0)
        return 0;
    return fread(buffer, 1, size, file);
}

Result OpenFileRange(FILE* file, uint64_t offset, uint64_t size, HZip* zip)
{
    return OpenFileRangeInternal(file, offset, size, ReadFileAt, 0, 0, zip);
}

Result OpenResourcePlatform(const char* path, HZip* zip)
{
    char mount_path[1024];
    if (dmSys::ResolveMountFileName(mount_path, sizeof(mount_path), path) != dmSys::RESULT_OK)
    {
        *zip = 0;
        return RESULT_NO_SUCH_ENTRY;
    }
    return Open(mount_path, zip);
}

} // namespace dmZip
