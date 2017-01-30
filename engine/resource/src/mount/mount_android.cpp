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
        AAsset *index_asset;
        AAsset *data_asset;
        AAsset *lu_data_asset;
    };

    Result MapAsset(AAssetManager* am, const char* path,  void*& out_asset, uint32_t& out_size, void*& out_map)
    {
        out_asset = (void*)AAssetManager_open(am, path, AASSET_MODE_RANDOM);
        if (!out_asset)
        {
            dmLogInfo("ANDROID res not fpound at path: %s", path);
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

    Result MapFile(const char* path, void*& out_map, uint32_t& out_size)
    {
        dmLogInfo("ANDROID mapping file (mmap)... path: %s", path)
        int fd = open(path, O_RDONLY);
        if (fd < 0)
        {
            dmLogInfo("ANDROID resource not found at path: %s", path);
            return RESULT_RESOURCE_NOT_FOUND;
        }

        struct stat fs;
        if (fstat(fd, &fs))
        {
            close(fd);
            return RESULT_IO_ERROR;
        }

        out_map = mmap(0, fs.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);

        if (!out_map || out_map == (void*)-1)
        {
            return RESULT_IO_ERROR;
        }

        out_size = fs.st_size;

        return RESULT_OK;
    }

    Result UnmapFile(void*& map, uint32_t size)
    {
        if (map != 0x0)
        {
            dmLogInfo("ANDROID Unmapping file (munmap)")
            munmap(map, size);
        }
        return RESULT_OK;
    }

    Result MountArchiveInternal(const char* index_path, const char* data_path, const char* lu_data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
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

        AAsset* lu_data_asset = 0x0;
        void* lu_data_map = 0x0;
        uint32_t lu_data_size = 0;
        FILE* lu_data_file = 0x0;
        if (lu_data_path != 0x0)
        {
            r = MapAsset(am, lu_data_path, (void*&)lu_data_asset, lu_data_size, lu_data_map);
            if (r != RESULT_OK)
            {
                AAsset_close(index_asset);
                AAsset_close(data_asset);
                dmLogError("Error mapping liveupdate data file");
                return RESULT_IO_ERROR;
            }

            lu_data_file = fopen(lu_data_path, "rb+");
            if (!lu_data_file)
            {
                AAsset_close(index_asset);
                AAsset_close(data_asset);
                AAsset_close(lu_data_asset);
                dmLogError("Error opening liveupdate data file");
                return RESULT_IO_ERROR;
            }
        }

        dmResourceArchive::Result res = WrapArchiveBuffer(index_map, index_length, data_map, lu_data_path, lu_data_map, lu_data_file, archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            AAsset_close(index_asset);
            AAsset_close(data_asset);
            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->index_asset = index_asset;
        info->data_asset = data_asset;
        info->lu_data_asset = lu_data_asset;
        *mount_info = (void*)info;
        return RESULT_OK;
    }

    void UnmountArchiveInternal(dmResourceArchive::HArchiveIndexContainer archive, void* mount_info)
    {
        MountInfo* info = (MountInfo*) mount_info;

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

        if (info->lu_data_asset)
        {
            AAsset_close(info->lu_data_asset);
        }

        delete info;

        dmResourceArchive::Delete(archive);
    }
}
