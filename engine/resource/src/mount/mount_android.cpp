#include <dlib/log.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <android_native_app_glue.h>
#include <android/asset_manager.h>
extern struct android_app* __attribute__((weak)) g_AndroidApp ;

#include "resource.h"
#include "resource_archive.h"
#include "resource_private.h"

namespace dmResource
{
    struct MountInfo
    {
        AAsset *asset;
    };

    Result MountArchiveInternal(const char* path, dmResourceArchive::HArchive* archive, void** mount_info)
    {
        AAssetManager* am = g_AndroidApp->activity->assetManager;
        AAsset* asset = AAssetManager_open(am, path, AASSET_MODE_RANDOM);
        if (!asset)
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }

        const void *map = AAsset_getBuffer(asset);
        if (!map)
        {
            AAsset_close(asset);
            return RESULT_IO_ERROR;
        }

        uint32_t length = AAsset_getLength(asset);
        dmResourceArchive::Result r = WrapArchiveBuffer(map, length, archive);
        if (r != dmResourceArchive::RESULT_OK)
        {
            AAsset_close(asset);
            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->asset = asset;
        *mount_info = (void*)info;
        return RESULT_OK;
    }

    void UnmountArchiveInternal(dmResourceArchive::HArchive archive, void* mount_info)
    {
        MountInfo* info = (MountInfo*) mount_info;
        if (info)
        {
            AAsset_close(info->asset);
            delete info;
        }
    }
}
