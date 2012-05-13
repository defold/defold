#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "resource_archive.h"

#if defined(__linux__) || defined(__MACH__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

namespace dmResourceArchive
{

    struct Entry
    {
        uint32_t m_NameOffset;
        uint32_t m_ResourceOffset;
        uint32_t m_ResourceSize;
    };

    struct Archive
    {
        uint32_t m_Version;
        uint32_t m_EntryCount;
        uint32_t m_FirstEntryOffset;
    };


    Result WrapArchiveBuffer(const void* buffer, uint32_t buffer_size, HArchive* archive)
    {
        Archive* a = (Archive*) buffer;
        uint32_t version = htonl(a->m_Version);
        if (version != 1)
        {
            return RESULT_VERSION_MISMATCH;
        }

        *archive = a;
        return RESULT_OK;
    }

    Result FindEntry(HArchive archive, const char* name, EntryInfo* entry)
    {
        uint32_t count = htonl(archive->m_EntryCount);
        uint32_t first_offset = htonl(archive->m_FirstEntryOffset);
        Entry* first = (Entry*) (first_offset + uintptr_t(archive));
        for (uint32_t i = 0; i < count; ++i)
        {
            Entry* file_entry = &first[i];
            const char* entry_name = (const char*) (htonl(file_entry->m_NameOffset) + uintptr_t(archive));
            if (strcmp(name, entry_name) == 0)
            {
                entry->m_Name = name;
                entry->m_Resource = (const void*) (htonl(file_entry->m_ResourceOffset) + uintptr_t(archive));
                entry->m_Size = htonl(file_entry->m_ResourceSize);
                return RESULT_OK;
            }
        }

        return RESULT_NOT_FOUND;
    }

    uint32_t GetEntryCount(HArchive archive)
    {
        return htonl(archive->m_EntryCount);
    }

}  // namespace dmResourceArchive
