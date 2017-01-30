#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource_archive.h"
#include <dlib/lz4.h>
#include <dlib/log.h>
#include <dlib/crypt.h>
#include <dlib/path.h>

// Maximum hash length convention. This size should large enough.
// If this length changes the VERSION needs to be bumped.
#define DMRESOURCE_MAX_HASH (64) // Equivalent to 512 bits

#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
#include <netinet/in.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

namespace dmResourceArchive
{
    const static uint64_t FILE_LOADED_INDICATOR = 1337;
    const char* KEY = "aQj8CScgNP4VsfXK";

    enum EntryFlag
    {
        ENTRY_FLAG_ENCRYPTED = 1 << 0,
    };

    struct ArchiveIndex
    {
        ArchiveIndex()
        {
            memset(this, 0, sizeof(ArchiveIndex));
        }

        uint32_t m_Version;
        uint32_t m_Pad;
        uint64_t m_Userdata;
        uint32_t m_EntryDataCount;
        uint32_t m_EntryDataOffset;
        uint32_t m_HashOffset;
        uint32_t m_HashLength;
    };

    struct ArchiveIndexContainer
    {
        ArchiveIndexContainer()
        {
            memset(this, 0, sizeof(ArchiveIndexContainer));
        }

        ArchiveIndex* m_ArchiveIndex; // this could be mem-mapped or loaded into memory from file
        bool m_IsMemMapped;

        /// Used if the archive is loaded from file
        uint8_t* m_Hashes;
        EntryData* m_Entries;
        FILE* m_FileResourceData;
        uint8_t* m_ResourceData;
    };

    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, const void* resource_data, HArchiveIndexContainer* archive)
    {
        *archive = new ArchiveIndexContainer;
        (*archive)->m_IsMemMapped = true;
        ArchiveIndex* a = (ArchiveIndex*) index_buffer;
        uint32_t version = htonl(a->m_Version);
        if (version != VERSION)
        {
            return RESULT_VERSION_MISMATCH;
        }
        (*archive)->m_ResourceData = (uint8_t*)resource_data;
        (*archive)->m_ArchiveIndex = a;
        return RESULT_OK;
    }

    void CleanupResources(FILE* index_file, FILE* data_file, ArchiveIndexContainer* archive)
    {
        if (index_file)
        {
            fclose(index_file);
        }

        if (data_file)
        {
            fclose(data_file);
        }

        if (archive)
        {
            delete archive;
        }
    }

    Result LoadArchive(const char* path_index, HArchiveIndexContainer* archive)
    {
        uint32_t filename_count = 0;
        while (true)
        {
            if (path_index[filename_count] == '\0')
                break;
            if (filename_count >= DMPATH_MAX_PATH)
                return RESULT_IO_ERROR;

            ++filename_count;
        }

        char path_data[DMPATH_MAX_PATH];
        uint32_t entry_count = 0, entry_offset = 0, hash_len = 0, hash_offset = 0, hash_total_size = 0, entries_total_size = 0;
        FILE* f_index = fopen(path_index, "rb");
        FILE* f_data = 0;

        ArchiveIndexContainer* aic = 0;
        ArchiveIndex* ai = 0;

        Result r = RESULT_OK;
        *archive = 0;

        if (!f_index)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        aic = new ArchiveIndexContainer;
        aic->m_IsMemMapped = false;

        ai = new ArchiveIndex;
        if (fread(ai, 1, sizeof(ArchiveIndex), f_index) != sizeof(ArchiveIndex))
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        if(htonl(ai->m_Version) != VERSION)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_VERSION_MISMATCH;
        }

        entry_count = htonl(ai->m_EntryDataCount);
        entry_offset = htonl(ai->m_EntryDataOffset);
        hash_len = htonl(ai->m_HashLength);
        hash_offset = htonl(ai->m_HashOffset);

        fseek(f_index, hash_offset, SEEK_SET);
        aic->m_Hashes = new uint8_t[entry_count * DMRESOURCE_MAX_HASH];
        hash_total_size = entry_count * DMRESOURCE_MAX_HASH;
        if (fread(aic->m_Hashes, 1, hash_total_size, f_index) != hash_total_size)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        fseek(f_index, entry_offset, SEEK_SET);
        aic->m_Entries = new EntryData[entry_count];
        entries_total_size = entry_count * sizeof(EntryData);
        if (fread(aic->m_Entries, 1, entries_total_size, f_index) != entries_total_size)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        // Mark that this archive was loaded from file, and not memory-mapped
        ai->m_Userdata = FILE_LOADED_INDICATOR;

        // Data file has same path and filename as index file, but extension .arcd instead of .arci.
        memcpy(&path_data, path_index, filename_count+1); // copy NULL terminator as well
        path_data[filename_count-1] = 'd';
        f_data = fopen(path_data, "rb");

        if (!f_data)
        {
            CleanupResources(f_index, f_data, aic);
            return RESULT_IO_ERROR;
        }

        aic->m_FileResourceData = f_data;
        aic->m_ArchiveIndex = ai;
        *archive = aic;

        return r;
    }

    void Delete(HArchiveIndexContainer archive)
    {
        // Only cleanup if not mem-mapped
        if(!archive->m_IsMemMapped)
        {
            delete[] archive->m_Entries;
            delete[] archive->m_Hashes;

            if (archive->m_FileResourceData)
            {
                fclose(archive->m_FileResourceData);
            }
            delete archive->m_ArchiveIndex;
        }

        delete archive;
    }

    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, EntryData* entry)
    {
        uint32_t entry_count = htonl(archive->m_ArchiveIndex->m_EntryDataCount);
        uint32_t entry_offset = htonl(archive->m_ArchiveIndex->m_EntryDataOffset);
        uint32_t hash_offset = htonl(archive->m_ArchiveIndex->m_HashOffset);
        uint32_t hash_len = htonl(archive->m_ArchiveIndex->m_HashLength);
        uint8_t* hashes = 0;
        EntryData* entries = 0;

        // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
        if (!archive->m_IsMemMapped)
        {
            hashes = archive->m_Hashes;
            entries = archive->m_Entries;
        }
        else
        {
            hashes = (uint8_t*)(uintptr_t(archive->m_ArchiveIndex) + hash_offset);
            entries = (EntryData*)(uintptr_t(archive->m_ArchiveIndex) + entry_offset);
        }

        // Search for hash with binary search (entries are sorted on hash)
        int first = 0;
        int last = (int)entry_count-1;
        while (first <= last)
        {
            int mid = first + (last - first) / 2;
            uint8_t* h = (hashes + DMRESOURCE_MAX_HASH * mid);

            int cmp = memcmp(hash, h, hash_len);
            if (cmp == 0)
            {
                if (entry != NULL)
                {
                    EntryData* e = &entries[mid];
                    entry->m_ResourceDataOffset = htonl(e->m_ResourceDataOffset);
                    entry->m_ResourceSize = htonl(e->m_ResourceSize);
                    entry->m_ResourceCompressedSize = htonl(e->m_ResourceCompressedSize);
                    entry->m_Flags = htonl(e->m_Flags);
                }

                return RESULT_OK;
            }
            else if (cmp > 0)
            {
                first = mid+1;
            }
            else if (cmp < 0)
            {
                last = mid-1;
            }
        }

        return RESULT_NOT_FOUND;
    }

    Result Read(HArchiveIndexContainer archive, EntryData* entry_data, void* buffer)
    {
        uint32_t size = entry_data->m_ResourceSize;
        uint32_t compressed_size = entry_data->m_ResourceCompressedSize;

        if (!archive->m_IsMemMapped)
        {
            fseek(archive->m_FileResourceData, entry_data->m_ResourceDataOffset, SEEK_SET);
            if (compressed_size != 0xFFFFFFFF) // resource is compressed
            {
                char *compressed_buf = (char*)malloc(compressed_size);
                if (!compressed_buf)
                {
                    return RESULT_MEM_ERROR;
                }

                if (fread(compressed_buf, 1, compressed_size, archive->m_FileResourceData) != compressed_size)
                {
                    free(compressed_buf);
                    return RESULT_IO_ERROR;
                }

                if(entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED)
                {
                    dmCrypt::Result cr = dmCrypt::Decrypt(dmCrypt::ALGORITHM_XTEA, (uint8_t*) compressed_buf, compressed_size, (const uint8_t*) KEY, strlen(KEY));
                    if (cr != dmCrypt::RESULT_OK)
                    {
                        free(compressed_buf);
                        return RESULT_UNKNOWN;
                    }
                }

                dmLZ4::Result r = dmLZ4::DecompressBufferFast(compressed_buf, compressed_size, buffer, size);
                free(compressed_buf);

                if (r == dmLZ4::RESULT_OK)
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
                if (fread(buffer, 1, size, archive->m_FileResourceData) == size)
                {
                    dmCrypt::Result cr = dmCrypt::RESULT_OK;
                    if (entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED)
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
            Result ret;

            void* r = (void*) ((uintptr_t(archive->m_ResourceData) + entry_data->m_ResourceDataOffset));
            void* decrypted = r;

            if (entry_data->m_Flags & ENTRY_FLAG_ENCRYPTED)
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

            if (compressed_size != 0xFFFFFFFF)
            {
                // Entry is compressed
                dmLZ4::Result result = dmLZ4::DecompressBufferFast(decrypted, compressed_size, buffer, size);
                if (result == dmLZ4::RESULT_OK)
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

    uint32_t GetEntryCount(HArchiveIndexContainer archive)
    {
        return htonl(archive->m_ArchiveIndex->m_EntryDataCount);
    }
}  // namespace dmResourceArchive
