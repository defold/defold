// Copyright 2020-2023 The Defold Foundation
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

#include "zip.h"
#include "zip/zip.h"

namespace dmZip
{

Result Open(const char* path, HZip* zip)
{
    *zip = zip_open(path, 9, 'r');
    return *zip != 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

void Close(HZip zip)
{
    if (zip)
        zip_close(zip);
}

uint32_t GetNumEntries(HZip zip)
{
    return (uint32_t)zip_total_entries(zip);
}

Result OpenEntry(HZip zip, const char* name)
{
    int r = zip_entry_open(zip, name);
    return r == 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result OpenEntry(HZip zip, uint32_t index)
{
    int r = zip_entry_openbyindex(zip, (int)index);
    return r == 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result CloseEntry(HZip zip)
{
    zip_entry_close(zip);
    return RESULT_OK;
}

bool IsEntryDir(HZip zip)
{
    return zip_entry_isdir(zip) != 0;
}

const char* GetEntryName(HZip zip)
{
    return zip_entry_name(zip);
}

Result GetEntrySize(HZip zip, uint32_t* size)
{
    uint64_t sz = zip_entry_size(zip);
    *size = (uint32_t)(sz & 0xFFFFFFFF);
    return RESULT_OK;
}

Result GetEntryIndex(HZip zip, uint32_t* out_index)
{
    int index = zip_entry_index(zip);
    if (index < 0)
        return RESULT_NO_SUCH_ENTRY;
    *out_index = (uint32_t)index;
    return RESULT_OK;
}

Result GetEntryData(HZip zip, void* buffer, uint32_t buffer_size)
{
    ssize_t nwritten = zip_entry_noallocread(zip, buffer, (size_t)buffer_size);
    if (nwritten < 0)
        return RESULT_BUFFER_NOT_LARGE_ENOUGH;
    return RESULT_OK;
}


} // namespace
