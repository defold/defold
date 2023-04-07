#include "provider.h"
// #include "provider_archive.h"
// #include "../resource_archive.h"

// #include <dlib/dstrings.h>
// #include <dlib/endian.h>
// #include <dlib/log.h>
// #include <dlib/lz4.h>
// #include <dlib/memory.h>
// #include <dlib/sys.h>

namespace dmResourceProviderHttp
{
    // dmResourceArchive::Result LoadManifestFromBuffer(const uint8_t* buffer, uint32_t buffer_len, dmResource::Manifest** out)
    // {
    //     dmResource::Manifest* manifest = new dmResource::Manifest();
    //     dmResource::Result result = ManifestLoadMessage(buffer, buffer_len, manifest);

    //     if (dmResource::RESULT_OK == result)
    //         *out = manifest;
    //     else
    //     {
    //         dmResource::DeleteManifest(manifest);
    //     }

    //     return dmResource::RESULT_OK == result ? dmResourceArchive::RESULT_OK : dmResourceArchive::RESULT_IO_ERROR;
    // }

    // dmResourceArchive::Result LoadManifest(const char* path, dmResource::Manifest** out)
    // {
    //     uint32_t manifest_length = 0;
    //     uint8_t* manifest_buffer = 0x0;

    //     uint32_t dummy_file_size = 0;
    //     dmSys::ResourceSize(path, &manifest_length);
    //     dmMemory::AlignedMalloc((void**)&manifest_buffer, 16, manifest_length);
    //     assert(manifest_buffer);
    //     dmSys::Result sys_result = dmSys::LoadResource(path, manifest_buffer, manifest_length, &dummy_file_size);

    //     if (sys_result != dmSys::RESULT_OK)
    //     {
    //         dmLogError("Failed to read manifest %s (%i)", path, sys_result);
    //         dmMemory::AlignedFree(manifest_buffer);
    //         return dmResourceArchive::RESULT_IO_ERROR;
    //     }

    //     dmResourceArchive::Result result = LoadManifestFromBuffer(manifest_buffer, manifest_length, out);
    //     dmMemory::AlignedFree(manifest_buffer);
    //     return result;
    // }

    // static dmResourceArchive::Result ResourceArchiveDefaultLoadManifest(const char* archive_name, const char* app_path, const char* app_support_path,
    //                                                                     const dmResource::Manifest* previous, dmResource::Manifest** out)
    // {
    //     // This function should be called first, so there should be no prior manifest
    //     assert(previous == 0);

    //     char manifest_path[DMPATH_MAX_PATH];
    //     dmPath::Concat(app_path, archive_name, manifest_path, sizeof(manifest_path));
    //     dmStrlCat(manifest_path, ".dmanifest", sizeof(manifest_path));

    //     return LoadManifest(manifest_path, out);
    // }

    // static dmResourceArchive::Result ResourceArchiveDefaultLoad(const dmResource::Manifest* manifest, const char* archive_name, const char* app_path, const char* app_support_path,
    //                                                             dmResourceArchive::HArchiveIndexContainer previous, dmResourceArchive::HArchiveIndexContainer* out)
    // {
    //     char archive_index_path[DMPATH_MAX_PATH];
    //     char archive_data_path[DMPATH_MAX_PATH];
    //     dmPath::Concat(app_path, archive_name, archive_index_path, sizeof(archive_index_path));
    //     dmPath::Concat(app_path, archive_name, archive_data_path, sizeof(archive_index_path));
    //     dmStrlCat(archive_index_path, ".arci", sizeof(archive_index_path));
    //     dmStrlCat(archive_data_path, ".arcd", sizeof(archive_data_path));

    //     void* mount_info = 0;
    //     dmResource::Result result = dmResource::MountArchiveInternal(archive_index_path, archive_data_path, out, &mount_info);
    //     if (dmResource::RESULT_OK == result && mount_info != 0 && *out != 0)
    //     {
    //         (*out)->m_UserData = mount_info;
    //     }
    //     return dmResourceArchive::RESULT_OK;
    // }

    // static dmResourceArchive::Result ResourceArchiveDefaultUnload(dmResourceArchive::HArchiveIndexContainer archive)
    // {
    //     dmResource::UnmountArchiveInternal(archive, archive->m_UserData);
    //     return dmResourceArchive::RESULT_OK;
    // }

    // dmResourceArchive::Result FindEntryInArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len, dmResourceArchive::EntryData* entry)
    // {
    //     uint32_t entry_count = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataCount);
    //     uint32_t entry_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_EntryDataOffset);
    //     uint32_t hash_offset = dmEndian::ToNetwork(archive->m_ArchiveIndex->m_HashOffset);
    //     uint8_t* hashes = 0;
    //     dmResourceArchive::EntryData* entries = 0;

    //     // If archive is loaded from file use the member arrays for hashes and entries, otherwise read with mem offsets.
    //     if (!archive->m_IsMemMapped)
    //     {
    //         hashes = archive->m_ArchiveFileIndex->m_Hashes;
    //         entries = archive->m_ArchiveFileIndex->m_Entries;
    //     }
    //     else
    //     {
    //         hashes = (uint8_t*)((uintptr_t)archive->m_ArchiveIndex + hash_offset);
    //         entries = (dmResourceArchive::EntryData*)((uintptr_t)archive->m_ArchiveIndex + entry_offset);
    //     }

    //     // Search for hash with binary search (entries are sorted on hash)
    //     int first = 0;
    //     int last = (int)entry_count-1;
    //     while (first <= last)
    //     {
    //         int mid = first + (last - first) / 2;
    //         uint8_t* h = (hashes + dmResourceArchive::MAX_HASH * mid);

    //         int cmp = memcmp(hash, h, hash_len);
    //         if (cmp == 0)
    //         {
    //             if (entry != 0)
    //             {
    //                 dmResourceArchive::EntryData* e = &entries[mid];
    //                 entry->m_ResourceDataOffset = dmEndian::ToNetwork(e->m_ResourceDataOffset);
    //                 entry->m_ResourceSize = dmEndian::ToNetwork(e->m_ResourceSize);
    //                 entry->m_ResourceCompressedSize = dmEndian::ToNetwork(e->m_ResourceCompressedSize);
    //                 entry->m_Flags = dmEndian::ToNetwork(e->m_Flags);
    //             }
    //             return dmResourceArchive::RESULT_OK;
    //         }
    //         else if (cmp > 0)
    //         {
    //             first = mid+1;
    //         }
    //         else if (cmp < 0)
    //         {
    //             last = mid-1;
    //         }
    //     }

    //     return dmResourceArchive::RESULT_NOT_FOUND;
    // }

    // dmResourceArchive::Result ReadEntryFromArchive(dmResourceArchive::HArchiveIndexContainer archive, const uint8_t* hash, uint32_t hash_len,
    //                                                 const dmResourceArchive::EntryData* entry, void* buffer)
    // {
    //     (void)hash;
    //     (void)hash_len;
    //     uint32_t size = entry->m_ResourceSize;
    //     uint32_t compressed_size = entry->m_ResourceCompressedSize;

    //     bool encrypted = (entry->m_Flags & dmResourceArchive::ENTRY_FLAG_ENCRYPTED);
    //     bool compressed = compressed_size != 0xFFFFFFFF;

    //     const dmResourceArchive::ArchiveFileIndex* afi = archive->m_ArchiveFileIndex;
    //     bool resource_memmapped = afi->m_IsMemMapped;

    //     // We have multiple combinations for regular archives:
    //     // memory mapped (yes/no) * compressed (yes/no) * encrypted (yes/no)
    //     // Decryption can be done into a buffer of the same size, although not into a read only array
    //     // We do this is to avoid unnecessary memory allocations

    //     //  compressed +  encrypted +  memmapped = need malloc (encrypt cannot modify memmapped file, decompress cannot use same src/dst)
    //     //  compressed +  encrypted + !memmapped = need malloc (decompress cannot use same src/dst)
    //     //  compressed + !encrypted +  memmapped = doesn't need malloc (can decompress because src != dst)
    //     //  compressed + !encrypted + !memmapped = need malloc (decompress cannot use same src/dst)
    //     // !compressed +  encrypted +  memmapped = doesn't need malloc
    //     // !compressed +  encrypted + !memmapped = doesn't need malloc
    //     // !compressed + !encrypted +  memmapped = doesn't need malloc
    //     // !compressed + !encrypted + !memmapped = doesn't need malloc

    //     void* compressed_buf = 0;
    //     bool temp_buffer = false;

    //     if (compressed && !( !encrypted && resource_memmapped))
    //     {
    //         compressed_buf = malloc(compressed_size);
    //         temp_buffer = true;
    //     } else {
    //         // For uncompressed data, use the destination buffer
    //         compressed_buf = buffer;
    //         memset(buffer, 0, size);
    //         compressed_size = compressed ? compressed_size : size;
    //     }

    //     assert(compressed_buf);

    //     if (!resource_memmapped)
    //     {
    //         FILE* resource_file = afi->m_FileResourceData;

    //         assert(temp_buffer || compressed_buf == buffer);

    //         fseek(resource_file, entry->m_ResourceDataOffset, SEEK_SET);
    //         if (fread(compressed_buf, 1, compressed_size, resource_file) != compressed_size)
    //         {
    //             if (temp_buffer)
    //                 free(compressed_buf);
    //             return dmResourceArchive::RESULT_IO_ERROR;
    //         }
    //     } else {
    //         void* r = (void*) (((uintptr_t)afi->m_ResourceData + entry->m_ResourceDataOffset));

    //         if (compressed && !encrypted)
    //         {
    //             compressed_buf = r;
    //         } else {
    //             memcpy(compressed_buf, r, compressed_size);
    //         }
    //     }

    //     // Design wish: If we switch the order of operations (and thus the file format)
    //     // we can simplify the code and save a temporary allocation
    //     //
    //     // Currently, we need to decrypt into a new buffer (since the input is read only):
    //     // input:[Encrypted + LZ4] ->   temp:[  LZ4      ] -> output:[   data   ]
    //     //
    //     // We could instead have:
    //     // input:[Encrypted + LZ4] -> output:[ Encrypted ] -> output:[   data   ]

    //     if(encrypted)
    //     {
    //         assert(temp_buffer || compressed_buf == buffer);

    //         dmResourceArchive::Result r = dmResourceArchive::DecryptBuffer(compressed_buf, compressed_size);
    //         if (r != dmResourceArchive::RESULT_OK)
    //         {
    //             if (temp_buffer)
    //                 free(compressed_buf);
    //             return r;
    //         }
    //     }

    //     if (compressed)
    //     {
    //         assert(compressed_buf != buffer);
    //         int decompressed_size;
    //         dmLZ4::Result r = dmLZ4::DecompressBuffer(compressed_buf, compressed_size, buffer, size, &decompressed_size);
    //         if (dmLZ4::RESULT_OK != r)
    //         {
    //             if (temp_buffer)
    //                 free(compressed_buf);
    //             return dmResourceArchive::RESULT_OUTBUFFER_TOO_SMALL;
    //         }
    //     } else {
    //         if (buffer != compressed_buf)
    //             memcpy(buffer, compressed_buf, size);
    //     }

    //     if (temp_buffer)
    //         free(compressed_buf);

    //     return dmResourceArchive::RESULT_OK;
    // }

    // static void SetupArchiveLoader(dmResourceProvider::ArchiveLoader* loader)
    // {
    //     dmResourceArchive::ArchiveLoader loader;
    //     loader.m_NameHash = dmHashString64("http");
    //     loader.m_LoadManifest = ResourceArchiveDefaultLoadManifest;
    //     loader.m_Load = ResourceArchiveDefaultLoad;
    //     loader.m_Unload = ResourceArchiveDefaultUnload;
    //     loader.m_FindEntry = FindEntryInArchive;
    //     loader.m_Read = ReadEntryFromArchive;
    //     dmResourceArchive::RegisterArchiveLoader(loader);
    // }

    //DM_DECLARE_ARCHIVE_LOADER(ResourceProviderHttp, dmHashString64("http"), SetupArchiveLoader);
}
