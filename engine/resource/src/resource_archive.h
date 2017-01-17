#ifndef RESOURCE_ARCHIVE_H
#define RESOURCE_ARCHIVE_H

#include <dlib/align.h>

namespace dmResourceArchive
{
    /**
     * This specifies the version of the resource archive and allows the engine
     * to check a manifest to ensure that it's compatible with the engine's
     * version of the archive format.
     */
    const static uint32_t VERSION = 4;

    typedef struct ArchiveIndexContainer* HArchiveIndexContainer;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_NOT_FOUND = 1,
        RESULT_VERSION_MISMATCH = -1,
        RESULT_IO_ERROR = -2,
        RESULT_MEM_ERROR = -3,
        RESULT_OUTBUFFER_TOO_SMALL = -4,
        RESULT_UNKNOWN = -1000,
    };

    struct DM_ALIGNED(16) EntryData
    {
        EntryData() :
            m_ResourceDataOffset(0),
            m_ResourceSize(0),
            m_ResourceCompressedSize(0),
            m_Flags(0) {}

        uint32_t m_ResourceDataOffset;
        uint32_t m_ResourceSize;
        uint32_t m_ResourceCompressedSize; // 0xFFFFFFFF if uncompressed
        uint32_t m_Flags;
    };

    /**
     * Wrap an archive index and data file already loaded in memory. Calling Delete() on wrapped
     * archive is not needed.
     * @param index_buffer archive index memory to wrap
     * @param index_buffer_size archive index size
     * @param resource_data resource data
     * @param archive archive index container handle
     * @return RESULT_OK on success
     */
    Result WrapArchiveBuffer(const void* index_buffer, uint32_t index_buffer_size, const void* resource_data, HArchiveIndexContainer* archive);

    /**
     * Load archive from filename. Only the index data is loaded into memory.
     * Resources are loaded on-demand using Read2() function.
     * @param file_name archive index to load
     * @param archive archive index container handle
     * @return RESULT_OK on success
     */
    Result LoadArchive(const char* file_name, HArchiveIndexContainer* archive);

    /**
     * Find resource entry within archive
     * @param archive archive index handle
     * @param hash resource hash digest to find
     * @param entry entry data
     * @return RESULT_OK on success
     */
    Result FindEntry(HArchiveIndexContainer archive, const uint8_t* hash, EntryData* entry);

    /**
     * Read resource
     * @param archive archive index handle
     * @param entry_data entry data
     * @param buffer buffer to load to
     * @return RESULT_OK on success
     */
    Result Read(HArchiveIndexContainer archive, EntryData* entry_data, void* buffer);

    /**
     * Delete archive index. Only required for archives created with LoadArchive function
     * @param archive archive index handle
     */
    void Delete(HArchiveIndexContainer archive);

    /**
     * Get total entries, i.e. files/resources in archive
     * @param archive archive index handle
     * @return entry count
     */
    uint32_t GetEntryCount(HArchiveIndexContainer archive);

}  // namespace dmResourceArchive

#endif
