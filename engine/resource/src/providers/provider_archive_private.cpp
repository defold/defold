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

#include "provider_private.h"
#include "provider_archive_private.h"
#include "../resource.h"

#include <dlib/dstrings.h>
#include <dlib/endian.h>
#include <dlib/log.h>
#include <dlib/memory.h>
#include <dlib/sys.h>

namespace dmResourceProviderArchivePrivate
{
    const char* GetArchiveIndexPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size)
    {
        dmSnPrintf(buffer, buffer_size, "%s%s.arci", uri->m_Location, uri->m_Path);
        return buffer;
    }

    const char* GetArchiveDataPath(const dmURI::Parts* uri, char* buffer, uint32_t buffer_size)
    {
        dmSnPrintf(buffer, buffer_size, "%s%s.arcd", uri->m_Location, uri->m_Path);
        return buffer;
    }

    void DebugPrintBuffer(const uint8_t* hash, uint32_t len)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            printf("%02X", hash[i]);
        }
    }

    static void PrintHash(const uint8_t* hash, uint32_t len)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            printf("%02X", hash[i]);
        }
    }

    void DebugPrintArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive)
    {
        uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
        uint8_t* hashes = 0;
        dmResourceArchive::EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_ArchiveFileIndex->m_Hashes;
            entries = archive->m_ArchiveFileIndex->m_Entries;
        }
        else
        {
            hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
            entries = (dmResourceArchive::EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
        }

        for (uint32_t i = 0; i < entry_count; ++i)
        {
            uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * i);
            dmResourceArchive::EntryData* e = &entries[i];

            uint32_t flags = dmEndian::ToNetwork(e->m_Flags);
            printf("entry e/c/l: %d%d%d sz: %u csz: %u off: %u hash: ",
                (flags & dmResourceArchive::ENTRY_FLAG_ENCRYPTED) != 0,
                (flags & dmResourceArchive::ENTRY_FLAG_COMPRESSED) != 0,
                (flags & dmResourceArchive::ENTRY_FLAG_LIVEUPDATE_DATA) != 0,
                dmEndian::ToNetwork(e->m_ResourceSize), dmEndian::ToNetwork(e->m_ResourceCompressedSize), dmEndian::ToNetwork(e->m_ResourceDataOffset));

            PrintHash(h, 20);
            printf("\n");
        }
    }
}
