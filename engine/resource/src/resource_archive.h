#ifndef RESOURCE_ARCHIVE_H
#define RESOURCE_ARCHIVE_H

/*
 Resource archive file format
 - All meta-data in network endian
 - All entries are lexically sorted by name

 uint32_t m_Version;     // No minor or bug-fix version numbering. Version must match identically.
 uint32_t m_EntryCount;
 uint32_t m_EntryOffset; // Offset to first entry relative file start

 struct Entry
 {
     uint32_t m_NameOffset;     // Offset to name relative file start
     uint32_t m_ResourceOffset; // Offset to resource relative file start
     uint32_t m_ResourceSize;
 };

*/

namespace dmResourceArchive
{
    typedef struct Archive* HArchive;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_NOT_FOUND = 1,
        RESULT_VERSION_MISMATCH = -1,
    };

    struct EntryInfo
    {
        const char* m_Name;
        const void* m_Resource;
        uint32_t    m_Size;
    };

    Result WrapArchiveBuffer(const void* buffer, uint32_t buffer_size, HArchive* archive);
    Result FindEntry(HArchive archive, const char* name, EntryInfo* entry);
    uint32_t GetEntryCount(HArchive archive);

}  // namespace dmResourceArchive

#endif
