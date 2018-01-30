
#include <dlib/log.h>

#include "resource.h"
#include "resource_archive.h"
#include "resource_private.h"

namespace dmResource
{
    Result MapFile(const char* filename, void*& map, uint32_t& size)
    {
        // Not used
        return RESULT_OK;
    }

    Result UnmapFile(void*& map, uint32_t size)
    {
        // Not used
        return RESULT_OK;
    }

    // TODO implement
    Result MountManifest(const char* manifest_filename, void*& out_map, uint32_t& out_size)
    {
        return RESULT_OK;
    }

    Result UnmountManifest(void *& map, uint32_t size)
    {
        return RESULT_OK;
    }

    Result MountArchiveInternal(const char* index_path, const char* data_path, const char* lu_data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        *mount_info = 0;
        dmLogInfo("=========================");
        dmLogInfo("MOUNTING GENERIC");
        dmLogInfo("index: %s", index_path);
        dmLogInfo("data: %s", data_path);
        dmLogInfo("lu data: %s", lu_data_path);
        dmLogInfo("=========================");
        dmResourceArchive::Result r = LoadArchive(index_path, data_path, lu_data_path, archive);
        if (r != dmResourceArchive::RESULT_OK)
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
        return RESULT_OK;
    }

    void UnmountArchiveInternal(dmResourceArchive::HArchiveIndexContainer archive, void* mount_info)
    {
        dmResourceArchive::Delete(archive);
    }
}