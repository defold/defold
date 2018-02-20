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
        void *lu_index_map;
        uint32_t lu_index_length;
        void *lu_data_map;
        uint32_t lu_data_map_lenght;
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

    Result MapFile(const char* path, void*& out_map, uint32_t& out_size)
    {
        int fd = open(path, O_RDONLY);
        if (fd < 0)
        {
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

    static Result UnmapAsset(AAsset*& asset)
    {
        if (asset != 0x0)
        {
            AAsset_close(asset);
        }
        return RESULT_OK;
    }

    Result UnmapFile(void*& map, uint32_t size)
    {
        if (map != 0x0)
        {
            munmap(map, size);
        }
        return RESULT_OK;
    }

    Result MountManifest(const char* manifest_filename, void*& out_map, uint32_t& out_size)
    {
        out_size = 0;
        return MapFile(manifest_filename, out_map, out_size);
    }

    Result UnmountManifest(void *& map, uint32_t size)
    {
        return UnmapFile(map, size);
    }

    Result MountArchiveInternal(const char* index_path, const char* data_path, const char* lu_data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        AAssetManager* am = g_AndroidApp->activity->assetManager;
        AAsset* index_asset = 0x0;
        uint32_t index_length = 0;
        void* index_map = 0x0;
        AAsset* data_asset = 0x0;
        uint32_t data_length = 0;
        void* data_map = 0x0;

        Result r = MapAsset(am, data_path, (void*&)data_asset, data_length, data_map);
        if (r != RESULT_OK)
        {
            dmLogError("Error when mapping data file, result = %i", r);
            return RESULT_IO_ERROR;
        }

        // Map index asset (if bundled) or file (if in local storage)
        if (strcmp(index_path, "game.arci") == 0)
        {
            r = MapAsset(am, index_path, (void*&)index_asset, index_length, index_map);
            if (r != RESULT_OK)
            {
                UnmapAsset(data_asset);
                dmLogError("Error when mapping index file, result: %i", r);
                return RESULT_IO_ERROR;
            }
        }
        else
        {
            r = MapFile(index_path, index_map, index_length);
            if (r != RESULT_OK)
            {
                UnmapAsset(data_asset);
                dmLogError("Error mapping liveupdate index file, result = %i", r);
                return RESULT_IO_ERROR;
            }
        }

        // Map liveupdate data resource file (if any)
        void* lu_data_map = 0x0;
        uint32_t lu_data_length = 0;
        FILE* lu_data_file = 0x0;
        if (lu_data_path != 0x0)
        {
            r = MapFile(lu_data_path, lu_data_map, lu_data_length);
            if (r != RESULT_OK)
            {
                UnmapAsset(index_asset);
                UnmapAsset(data_asset);
                UnmapFile(index_map, index_length);
                dmLogError("Error mapping liveupdate data file, result = %i", r);
                return RESULT_IO_ERROR;
            }

            lu_data_file = fopen(lu_data_path, "rb+");
            if (!lu_data_file)
            {
                UnmapAsset(index_asset);
                UnmapAsset(data_asset);
                UnmapFile(index_map, index_length);
                UnmapFile(lu_data_map, lu_data_length);
                dmLogError("Error opening liveupdate data file, result = %i", r);
                return RESULT_IO_ERROR;
            }
        }

        dmResourceArchive::Result res = WrapArchiveBuffer(index_map, index_length, data_map, lu_data_path, lu_data_map, lu_data_file, archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            UnmapAsset(index_asset);
            UnmapAsset(data_asset);
            if (lu_data_path)
            {
                UnmapFile(index_map, index_length);
                UnmapFile(lu_data_map, lu_data_length);
                fclose(lu_data_file);
            }
            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->index_asset = index_asset;
        info->data_asset = data_asset;
        info->lu_index_map = index_map;
        info->lu_index_length = index_length;
        info->lu_data_map = lu_data_map;
        info->lu_data_map_lenght = lu_data_length;
        *mount_info = (void*)info;
        return RESULT_OK;
    }

    void UnmountArchiveInternal(dmResourceArchive::HArchiveIndexContainer &archive, void* mount_info)
    {
        MountInfo* info = (MountInfo*) mount_info;

        if (!info)
        {
            return;
        }

        if (info->index_asset)
        {
            UnmapAsset(info->index_asset);
        }
        else if (info->lu_index_map)
        {
            UnmapFile(info->lu_index_map, info->lu_index_length);
        }

        if (info->data_asset)
        {
            UnmapAsset(info->data_asset);
        }

        if (info->lu_data_map)
        {
            UnmapFile(info->lu_data_map, info->lu_data_map_lenght);
        }

        delete info;

        dmResourceArchive::Delete(archive);
    }
}
