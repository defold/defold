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

#include "zip.h"
#include "sys.h"
#include "zip/zip.h"

#if defined(__ANDROID__)
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace dmZip
{

static size_t ReadFileAt(FILE* file, uint64_t offset, void* buffer, size_t size)
{
#if defined(__ANDROID__)
    size_t total_read = 0;
    int fd = fileno(file);
    if (fd < 0)
        return 0;

    while (total_read < size)
    {
        uint64_t read_offset = offset + total_read;
        if (read_offset < offset || read_offset > INT64_MAX)
            break;

        size_t read_size = size - total_read;
        if (read_size > SSIZE_MAX)
            read_size = SSIZE_MAX;

        ssize_t nread;
        do
        {
            nread = pread64(fd, (uint8_t*)buffer + total_read, read_size, (off64_t)read_offset);
        } while (nread < 0 && errno == EINTR);

        if (nread <= 0)
            break;
        total_read += (size_t)nread;
    }

    return total_read;
#else
    if (dmSys::FileSeek64(file, offset) != 0)
        return 0;
    return fread(buffer, 1, size, file);
#endif
}

Result Open(const char* path, HZip* zip)
{
    *zip = zip_open(path, 9, 'r');
    return *zip != 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result OpenStream(const char *stream, uint32_t size, HZip* zip)
{
    *zip = zip_stream_open(stream, size, 9, 'r');
    return *zip != 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result OpenFileRange(FILE* file, uint64_t offset, uint64_t size, HZip* zip)
{
    *zip = zip_cstream_openwithoffset(file, offset, size, 9, ReadFileAt);
    return *zip != 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

void Close(HZip zip)
{
    if (zip)
        zip_close(zip);
}

uint32_t GetNumEntries(HZip zip)
{
    return (uint32_t)zip_entries_total(zip);
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

Result GetEntryDataOffset(HZip zip, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
{
    ssize_t nwritten = zip_entry_noallocreadwithoffset(zip, (size_t)offset, (size_t)size, buffer);
    if (nwritten < 0)
        return RESULT_BUFFER_NOT_LARGE_ENOUGH;
    *nread = nwritten;
    return RESULT_OK;
}


} // namespace
