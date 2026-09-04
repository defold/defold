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
#include "path.h"
#include "zip_private.h"
#include "zip/zip.h"

namespace dmZip
{

Result OpenArchive(zip_t* archive, void* close_context, FCloseCallback close_callback, HZip* zip)
{
    *zip = 0;
    if (!archive)
    {
        if (close_callback)
            close_callback(close_context);
        return RESULT_NO_SUCH_ENTRY;
    }

    ZipArchive* zip_archive = new ZipArchive;
    zip_archive->m_Archive = archive;
    zip_archive->m_CloseContext = close_context;
    zip_archive->m_CloseCallback = close_callback;
    *zip = zip_archive;
    return RESULT_OK;
}

Result Open(const char* path, HZip* zip)
{
    return OpenArchive(zip_open(path, 9, 'r'), 0, 0, zip);
}

Result OpenResource(const char* path, HZip* zip)
{
    char normalized_path[1024];
    dmPath::Normalize(path, normalized_path, sizeof(normalized_path));
    return OpenResourcePlatform(normalized_path, zip);
}

Result OpenStream(const char *stream, uint32_t size, HZip* zip)
{
    return OpenArchive(zip_stream_open(stream, size, 9, 'r'), 0, 0, zip);
}

Result OpenFileRangeInternal(FILE* file, uint64_t offset, uint64_t size,
                             zip_cstream_read_callback read_callback,
                             void* close_context, FCloseCallback close_callback,
                             HZip* zip)
{
    zip_t* archive = zip_cstream_openwithoffset(file, offset, size, 9, read_callback);
    return OpenArchive(archive, close_context, close_callback, zip);
}

void Close(HZip zip)
{
    if (zip)
    {
        zip_close(zip->m_Archive);
        if (zip->m_CloseCallback)
            zip->m_CloseCallback(zip->m_CloseContext);
        delete zip;
    }
}

uint32_t GetNumEntries(HZip zip)
{
    return (uint32_t)zip_entries_total(zip->m_Archive);
}

Result OpenEntry(HZip zip, const char* name)
{
    int r = zip_entry_open(zip->m_Archive, name);
    return r == 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result OpenEntry(HZip zip, uint32_t index)
{
    int r = zip_entry_openbyindex(zip->m_Archive, (int)index);
    return r == 0 ? RESULT_OK : RESULT_NO_SUCH_ENTRY;
}

Result CloseEntry(HZip zip)
{
    zip_entry_close(zip->m_Archive);
    return RESULT_OK;
}

bool IsEntryDir(HZip zip)
{
    return zip_entry_isdir(zip->m_Archive) != 0;
}

const char* GetEntryName(HZip zip)
{
    return zip_entry_name(zip->m_Archive);
}

Result GetEntrySize(HZip zip, uint32_t* size)
{
    uint64_t sz = zip_entry_size(zip->m_Archive);
    *size = (uint32_t)(sz & 0xFFFFFFFF);
    return RESULT_OK;
}

Result GetEntryIndex(HZip zip, uint32_t* out_index)
{
    int index = zip_entry_index(zip->m_Archive);
    if (index < 0)
        return RESULT_NO_SUCH_ENTRY;
    *out_index = (uint32_t)index;
    return RESULT_OK;
}

Result GetEntryData(HZip zip, void* buffer, uint32_t buffer_size)
{
    ssize_t nwritten = zip_entry_noallocread(zip->m_Archive, buffer, (size_t)buffer_size);
    if (nwritten < 0)
        return RESULT_BUFFER_NOT_LARGE_ENOUGH;
    return RESULT_OK;
}

Result GetEntryDataOffset(HZip zip, uint32_t offset, uint32_t size, void* buffer, uint32_t* nread)
{
    ssize_t nwritten = zip_entry_noallocreadwithoffset(zip->m_Archive, (size_t)offset, (size_t)size, buffer);
    if (nwritten < 0)
        return RESULT_BUFFER_NOT_LARGE_ENOUGH;
    *nread = nwritten;
    return RESULT_OK;
}


} // namespace
