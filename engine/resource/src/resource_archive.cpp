#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource_archive.h"
#include <dlib/lz4.h>
#include <dlib/crypt.h>

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

namespace dmResourceArchive
{
    const static uint32_t VERSION = 4;
    const char* KEY = "aQj8CScgNP4VsfXK";

    enum EntryFlag
    {
        ENTRY_FLAG_ENCRYPTED = 1 << 0,
    };

    struct Entry
    {
        uint32_t m_NameOffset;
        uint32_t m_ResourceOffset;
        uint32_t m_ResourceSize;
        // 0xFFFFFFFF if uncompressed
        uint32_t m_ResourceCompressedSize;
        uint32_t m_Flags;
    };

    struct Meta
    {
        char*   m_StringPool;
        Entry*  m_Entries;
        FILE*   m_File;

        Meta()
        {
            m_StringPool = 0;
            m_Entries = 0;
            m_File = 0;
        }

        ~Meta()
        {
            if (m_StringPool)
            {
                delete[] m_StringPool;
            }

            if (m_Entries)
            {
                delete[] m_Entries;
            }

            if (m_File)
            {
                fclose(m_File);
            }
        }

    };

    struct Archive
    {
        uint32_t m_Version;
        uint32_t m_Pad;
        union
        {
            uint64_t m_Userdata;
            Meta*    m_Meta;
        };
        uint32_t m_StringPoolOffset;
        uint32_t m_StringPoolSize;
        uint32_t m_EntryCount;
        uint32_t m_FirstEntryOffset;
    };

    Result WrapArchiveBuffer(const void* buffer, uint32_t buffer_size, HArchive* archive)
    {
        Archive* a = (Archive*) buffer;
        uint32_t version = htonl(a->m_Version);
        if (version != VERSION)
        {
            return RESULT_VERSION_MISMATCH;
        }

        *archive = a;
        return RESULT_OK;
    }

#define READ_AND_BAIL(p, n) \
        if (fread(p, 1, n, f) != n)\
        {\
            r = RESULT_IO_ERROR;\
            goto bail;\
        }\

    Result LoadArchive(const char* file_name, HArchive* archive)
    {
        *archive = 0;

        FILE* f = fopen(file_name, "rb");
        Archive* a = 0;
        Meta* meta = 0;
        Result r = RESULT_OK;

        if (!f)
        {
            r = RESULT_IO_ERROR;
            goto bail;
        }

        a = new Archive;

        READ_AND_BAIL(a, sizeof(Archive));
        if (htonl(a->m_Version) != VERSION)
        {
            r = RESULT_VERSION_MISMATCH;
            goto bail;
        }

        meta = new Meta();
        fseek(f, htonl(a->m_StringPoolOffset), SEEK_SET);
        meta->m_StringPool = new char[htonl(a->m_StringPoolSize)];
        READ_AND_BAIL(meta->m_StringPool, htonl(a->m_StringPoolSize));

        fseek(f, htonl(a->m_FirstEntryOffset), SEEK_SET);
        meta->m_Entries = new Entry[htonl(a->m_EntryCount)];
        READ_AND_BAIL(meta->m_Entries, sizeof(Entry) * htonl(a->m_EntryCount));

        *archive = a;
        meta->m_File = f;
        a->m_Meta = meta;

        return r;
bail:
       if (f)
       {
           fclose(f);
       }

       if (a)
       {
           delete a;
       }

       if (meta)
       {
           delete meta;
       }

       return r;
    }

    void Delete(HArchive archive)
    {
        if (archive->m_Meta)
        {
            delete archive->m_Meta;
            delete archive;
        }
    }

    Result FindEntry(HArchive archive, const char* name, EntryInfo* entry)
    {
        uint32_t count = htonl(archive->m_EntryCount);
        uint32_t first_offset = htonl(archive->m_FirstEntryOffset);
        uint32_t string_pool_offset = htonl(archive->m_StringPoolOffset);

        Entry* first;
        const char* string_pool;
        if (archive->m_Meta)
        {
            Meta* meta = archive->m_Meta;
            first = meta->m_Entries;
            string_pool = meta->m_StringPool;
        }
        else
        {
            first = (Entry*) (first_offset + uintptr_t(archive));
            string_pool = (const char*) (uintptr_t(archive) + uintptr_t(string_pool_offset));
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            Entry* file_entry = &first[i];
            const char* entry_name = (const char*) (htonl(file_entry->m_NameOffset) + string_pool);
            if (strcmp(name, entry_name) == 0)
            {
                entry->m_Name = name;
                //entry->m_Resource = (const void*) (htonl(file_entry->m_ResourceOffset) + uintptr_t(archive));
                entry->m_Offset = htonl(file_entry->m_ResourceOffset);
                entry->m_Size = htonl(file_entry->m_ResourceSize);
                entry->m_CompressedSize = htonl(file_entry->m_ResourceCompressedSize);
                entry->m_Flags = htonl(file_entry->m_Flags);
                entry->m_Entry = file_entry;
                return RESULT_OK;
            }
        }

        return RESULT_NOT_FOUND;
    }

    Result Read(HArchive archive, EntryInfo* entry_info, void* buffer)
    {
        uint32_t size = entry_info->m_Size;
        uint32_t compressed_size = entry_info->m_CompressedSize;
        int decompressed_size = 0;

        if (archive->m_Meta)
        {
            Meta* meta = archive->m_Meta;
            fseek(meta->m_File, entry_info->m_Offset, SEEK_SET);

            if (compressed_size != 0xFFFFFFFF)
            {
                // Entry is compressed
                char *compressed_buf = (char *)malloc(compressed_size);
                if(!compressed_buf)
                {
                    return RESULT_MEM_ERROR;
                }

                if (fread(compressed_buf, 1, compressed_size, meta->m_File) != compressed_size)
                {
                    free(compressed_buf);
                    return RESULT_IO_ERROR;
                }

                if (entry_info->m_Flags & ENTRY_FLAG_ENCRYPTED)
                {
                    dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) compressed_buf, compressed_size, (const uint8_t*) KEY, strlen(KEY));
                    if (cr != dmCrypt::RESULT_OK)
                    {
                        free(compressed_buf);
                        return RESULT_UNKNOWN;
                    }
                }

                dmLZ4::Result r = dmLZ4::DecompressBuffer(compressed_buf, compressed_size, buffer, size, &decompressed_size);
                free(compressed_buf);

                if (r == dmLZ4::RESULT_OK && decompressed_size == size)
                {
                    return RESULT_OK;
                }
                else
                {
                    return RESULT_OUTBUFFER_TOO_SMALL;
                }
            }
            else
            {
                // Entry is uncompressed
                if (fread(buffer, 1, size, meta->m_File) == size)
                {
                    dmCrypt::Result cr = dmCrypt::RESULT_OK;
                    if (entry_info->m_Flags & ENTRY_FLAG_ENCRYPTED)
                    {
                        cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) buffer, size, (const uint8_t*) KEY, strlen(KEY));
                    }
                    return (cr == dmCrypt::RESULT_OK) ? RESULT_OK : RESULT_UNKNOWN;
                }
                else
                {
                    return RESULT_OUTBUFFER_TOO_SMALL;
                }
            }
        }
        else
        {
            void* r = (void*) (entry_info->m_Offset + uintptr_t(archive));
            void* decrypted = r;

            if (entry_info->m_Flags & ENTRY_FLAG_ENCRYPTED)
            {
                uint32_t bufsize = (compressed_size != 0xFFFFFFFF) ? compressed_size : size;
                decrypted = (uint8_t*) malloc(bufsize);
                memcpy(decrypted, r, bufsize);
                dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) decrypted, bufsize, (const uint8_t*) KEY, strlen(KEY));
                if (cr != dmCrypt::RESULT_OK)
                {
                    free(decrypted);
                    return RESULT_UNKNOWN;
                }
            }

            Result ret;
            if (compressed_size != 0xFFFFFFFF)
            {
                // Entry is compressed
                dmLZ4::Result result = dmLZ4::DecompressBuffer(decrypted, compressed_size, buffer, size, &decompressed_size);
                if (result == dmLZ4::RESULT_OK && decompressed_size == size)
                {
                    ret = RESULT_OK;
                }
                else
                {
                    ret = RESULT_OUTBUFFER_TOO_SMALL;
                }
            }
            else
            {
                // Entry is uncompressed
                memcpy(buffer, decrypted, size);
                ret = RESULT_OK;
            }

            // if needed aux buffer
            if (decrypted != r)
            {
                free(decrypted);
            }

            return ret;
        }
    }

    uint32_t GetEntryCount(HArchive archive)
    {
        return htonl(archive->m_EntryCount);
    }

}  // namespace dmResourceArchive
