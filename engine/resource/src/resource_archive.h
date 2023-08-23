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

#ifndef DM_RESOURCE_ARCHIVE_H
#define DM_RESOURCE_ARCHIVE_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <dlib/uri.h>
#include <dlib/align.h>
#include <dlib/array.h>
#include <dlib/path.h> // DMPATH_MAX_PATH


namespace dmResourceArchive
{
    /**
     * This specifies the version of the resource archive and allows the engine
     * to check a manifest to ensure that it's compatible with the engine's
     * version of the archive format.
     */
    const static uint32_t VERSION = 5;

    // Maximum hash length convention. This size should large enough.
    // If this length changes the VERSION needs to be bumped.
    // Equivalent to 512 bits
    const static uint32_t MAX_HASH = 64;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_NOT_FOUND = 1,
        RESULT_VERSION_MISMATCH = -1,
        RESULT_IO_ERROR = -2,
        RESULT_MEM_ERROR = -3,
        RESULT_OUTBUFFER_TOO_SMALL = -4,
        RESULT_ALREADY_STORED = -5,
        RESULT_INVALID_DATA = -6,
        RESULT_UNKNOWN = -1000,
    };

    enum EntryFlag
    {
        ENTRY_FLAG_ENCRYPTED        = 1 << 0,
        ENTRY_FLAG_COMPRESSED       = 1 << 1,
        ENTRY_FLAG_LIVEUPDATE_DATA  = 1 << 2,
    };

    // part of the .arci file format
    struct DM_ALIGNED(16) EntryData
    {
        EntryData() :
            m_ResourceDataOffset(0),
            m_ResourceSize(0),
            m_ResourceCompressedSize(0),
            m_Flags(0) {}

        uint32_t m_ResourceDataOffset;
        uint32_t m_ResourceSize;
        uint32_t m_ResourceCompressedSize;  // 0xFFFFFFFF if uncompressed
        uint32_t m_Flags;                   // A combination of dmResourceArchive::EntryFlag
    };

    // For memory mapped files (or files read directly into memory)
    struct DM_ALIGNED(16) ArchiveIndex
    {
        ArchiveIndex();

        uint32_t m_Version;
        uint32_t :32;
        uint64_t m_Userdata;
        uint32_t m_EntryDataCount;
        uint32_t m_EntryDataOffset;
        uint32_t m_HashOffset;
        uint32_t m_HashLength;
        uint8_t  m_ArchiveIndexMD5[16]; // 16 bytes is the size of md5
    };

    // Used if the archive is loaded from file (i.e a bundled archive of live update archive)
    struct ArchiveFileIndex
    {
        ArchiveFileIndex()
        {
            memset(this, 0, sizeof(ArchiveFileIndex));
        }
        char        m_Path[DMPATH_MAX_PATH];
        uint8_t*    m_Hashes;           // Sorted list of filenames (i.e. hashes)
        EntryData*  m_Entries;          // Indices of this list matches indices of m_Hashes
        FILE*       m_FileResourceData; // game.arcd file handle
        uint8_t*    m_ResourceData;     // mem-mapped game.arcd
        uint32_t    m_ResourceSize;     // the size of the memory mapped region
        bool        m_IsMemMapped;      // Is the data memory mapped?
    };

    struct ArchiveIndexContainer
    {
        ArchiveIndexContainer()
        {
            memset(this, 0, sizeof(ArchiveIndexContainer));
        }

        ArchiveIndexContainer* m_Next;

        ArchiveIndex*       m_ArchiveIndex;     // this could be mem-mapped or loaded into memory from file
        ArchiveFileIndex*   m_ArchiveFileIndex; // Used if the archive is loaded from file (bundled archive)

        //ArchiveLoader       m_Loader;
        void*               m_UserData;         // private to the loader

        uint32_t m_ArchiveIndexSize;            // kept for unmapping
        uint8_t  m_IsMemMapped:1; // if the m_ArchiveIndex is memory mapped
        uint8_t  :7;
    };

    typedef struct ArchiveIndexContainer* HArchiveIndexContainer;

    typedef struct ArchiveIndex* HArchiveIndex;

    // This header is a little bit hidden.
    // It is written by the ArchiveBuilder.java in writeResourcePack()
    struct DM_ALIGNED(16) LiveUpdateResourceHeader {
        uint32_t    m_Size;
        uint8_t     m_Flags;        // See dmResourceArchive::EntryData / EntryFlag
        uint8_t     m_Padding[11];
    };

    struct LiveUpdateResource {
        LiveUpdateResource() {
            memset(this, 0x0, sizeof(LiveUpdateResource));
        }

        LiveUpdateResource(const uint8_t* buf, size_t buflen) {
            Set(buf, buflen);
        }

        void Set(const uint8_t* buf, size_t buflen) {
            m_Count = buflen - sizeof(LiveUpdateResourceHeader);
            m_Data = (uint8_t*)((uintptr_t)buf + sizeof(LiveUpdateResourceHeader));
            m_Header = (LiveUpdateResourceHeader*)((uintptr_t)buf);
        }

        void Set(const LiveUpdateResource& other) {
            *this = other;
        }

        uint32_t GetSizeWithHeader() const {
            return m_Count + (uint32_t)sizeof(LiveUpdateResourceHeader);
        }

        bool HasValidHeader() const {
            return m_Header != 0;
        }

        void* GetDataWithHeader() const
        {
            return (void*)m_Header;
        }

        const uint8_t*              m_Data;
        uint32_t                    m_Count;
        LiveUpdateResourceHeader*   m_Header;
    };

    // Loads a .arci and a .arcd into an HArchiveContainer
    Result LoadArchiveFromFile(const char* index_path, const char* data_path, HArchiveIndexContainer* out);

    /**
     * Wrap an archive index and data file already loaded in memory. Calling Delete() on wrapped
     * archive is not needed.
     * @param index_buffer archive index memory to wrap
     * @param index_buffer_size archive index size
     * @param mem_mapped_index is the data memory mapped
     * @param resource_buffer archive index memory to wrap
     * @param resource_buffer_size archive index size
     * @param mem_mapped_data is the data memory mapped
     * @param archive archive index container handle
     * @return RESULT_OK on success
     */
    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, bool mem_mapped_index,
                             const void* resource_data, uint32_t resource_data_size, bool mem_mapped_data,
                             HArchiveIndexContainer* archive);

    /**
     * Find resource entry within the loaded archives
     * @param archive archive index handle
     * @param hash resource hash digest to find
     * @param entry entry data
     * @return RESULT_OK on success
     */
    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, EntryData** entry);

    /**
     * Read resource from the given archive
     * @param archive archive index handle
     * @param entry_data entry data
     * @param buffer buffer to load to
     * @return RESULT_OK on success
     */
    Result ReadEntry(HArchiveIndexContainer archive, const EntryData* entry, void* buffer);

    /**
     * Delete archive index. Only required for archives created with LoadArchive function
     * @param archive archive index handle
     */
    void Delete(HArchiveIndexContainer &archive);


    /**
     * Make a deep-copy of the existing archive index within archive container and return copy on successful insertion of LiveUpdate resource in the archive
     * @param archive archive container
     * @param path file to save to
     * @param hash_digest hash_digest data
     * @param hash_digest_len size in bytes of hash_digest data
     * @param resource LiveUpdate resource to insert
     * @param proj_id project id SHA
     * @param out_new_index reference to HArchiveIndex that will cointain the new archive index (on success)
     * @return RESULT_OK on success
     */
    Result NewArchiveIndexWithResource(HArchiveIndexContainer archive, const char* path, const uint8_t* hash_digest, uint32_t hash_digest_len,
                                            const dmResourceArchive::LiveUpdateResource* resource, HArchiveIndex& out_new_index);

    /**
     * Set new archive index in archive container. Replace existing archive index if set
     * @param archive archive container
     * @param new_index HArchiveIndex to set
     * @param mem_mapped memory mapped if true
     */
    void SetNewArchiveIndex(HArchiveIndexContainer archive_container, HArchiveIndex new_index, bool mem_mapped);

    // For debugging purposes only
    void DebugArchiveIndex(HArchiveIndexContainer archive);

}  // namespace dmResourceArchive

#endif
