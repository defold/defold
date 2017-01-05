#include <dlib/log.h>
#include <dlib/path.h>
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

    struct MountInfo2
    {
        AAsset *index_asset;
        AAsset *data_asset;
    };

    Result MapAsset(AAssetManager* am, const char* path,  void*& out_asset, uint32_t& out_size, void*& out_map)
    {
        out_asset = (void*)AAssetManager_open(am, path, AASSET_MODE_RANDOM);
        if (!out_asset)
        {
            return RESULT_RESOURCE_NOT_FOUND;
        }
        out_map = (void*)AAsset_getBuffer((AAsset*)out_asset);
        if (!out_map)
        {
            AAsset_close((AAsset*)out_asset);
            return RESULT_IO_ERROR;
        }
        out_size = AAsset_getLength((AAsset*)out_asset);

        return RESULT_OK;
    }

    Result MountArchiveInternal(const char* index_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        // Derive path of arcd file from path to arci
        char data_path[DMPATH_MAX_PATH];
        memcpy(&data_path, index_path, strlen(index_path)+1); // copy NULL terminator as well
        data_path[strlen(index_path)-1] = 'd';

        AAssetManager* am = g_AndroidApp->activity->assetManager;
        AAsset* index_asset = 0x0;
        uint32_t index_length = 0;
        void* index_map = 0x0;
        Result r = MapAsset(am, index_path, (void*&)index_asset, index_length, index_map);
        if (r != RESULT_OK)
        {
            dmLogInfo("Error when mapping index file, result: %i", r);
            return RESULT_IO_ERROR;
        }

        AAsset* data_asset = 0x0;
        uint32_t data_length = 0;
        void* data_map = 0x0;

        r = MapAsset(am, data_path, (void*&)data_asset, data_length, data_map);
        if (r != RESULT_OK)
        {
            AAsset_close(index_asset);
            dmLogInfo("Error when mapping data file, result: %i", r);
        }

        dmResourceArchive::Result res = WrapArchiveBuffer2(index_map, index_length, data_map, archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            AAsset_close(index_asset);
            AAsset_close(data_asset);
            return RESULT_IO_ERROR;
        }

        MountInfo2* info = new MountInfo2();
        info->index_asset = index_asset;
        info->data_asset = data_asset;
        *mount_info = (void*)info;
        return RESULT_OK;
    }

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

    void UnmountArchiveInternal(dmResourceArchive::HArchiveIndexContainer archive, void* mount_info)
    {
        MountInfo2* info = (MountInfo2*) mount_info;

        if (!info)
        {
            return;
        }

        if (info->index_asset)
        {
            AAsset_close(info->index_asset);
        }

        if (info->data_asset)
        {
            AAsset_close(info->data_asset);
        }

        delete info;
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
