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

#include "zip.h"
#include "zip/zip.h"

#if defined(_WIN32)
  #if defined(_WIN64)
    typedef int64_t ssize_t;
  #else
    typedef int32_t ssize_t;
  #endif
#endif

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

Result OpenEntry(HZip zip, const char* name)
{
    int r = zip_entry_open(zip, name);
    return r == 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result CloseEntry(HZip zip)
{
    zip_entry_close(zip);
    return RESULT_OK;
}

Result GetEntrySize(HZip zip, uint32_t* size)
{
    uint64_t sz = zip_entry_size(zip);
    *size = (uint32_t)(sz & 0xFFFFFFFF);
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
