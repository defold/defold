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
        AAsset* index_asset;
        AAsset* data_asset;
        void* data_map; // non null if retrieved by MapFile
        void* index_map; // non null if retrieved by MapFile
        uint32_t data_length;
        uint32_t index_length;
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

    Result MountArchiveInternal(const char* index_path, const char* data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        AAssetManager* am = g_AndroidApp->activity->assetManager;
        AAsset* index_asset = 0x0;
        uint32_t index_length = 0;
        void* index_map = 0x0;
        AAsset* data_asset = 0x0;
        uint32_t data_length = 0;
        void* data_map = 0x0;

        Result r;

        // Map data asset (if bundled) or file (if in local storage)
        bool mem_mapped_data = false;
        if (strcmp(data_path, "game.arcd") == 0)
        {
            r = MapAsset(am, data_path, (void*&)data_asset, data_length, data_map);
            if (r != RESULT_OK)
            {
                dmLogError("Error when mapping data file '%s', result = %i", data_path, r);
                return RESULT_IO_ERROR;
            }
        } else
        {
            r = MapFile(data_path, data_map, data_length);
            if (r != RESULT_OK)
            {
                dmLogError("Error mapping liveupdate data file, result = %i", r);
                return RESULT_IO_ERROR;
            }
            mem_mapped_data = true;
        }

        // Map index asset (if bundled) or file (if in local storage)
        bool mem_mapped_index = false;
        if (strcmp(index_path, "game.arci") == 0)
        {
            r = MapAsset(am, index_path, (void*&)index_asset, index_length, index_map);
            if (r != RESULT_OK)
            {
                if (mem_mapped_data)
                    UnmapFile(data_map, data_length);
                else
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
                if (mem_mapped_data)
                    UnmapFile(data_map, data_length);
                else
                    UnmapAsset(data_asset);
                dmLogError("Error mapping liveupdate index file, result = %i", r);
                return RESULT_IO_ERROR;
            }
            mem_mapped_index = true;
        }

        dmResourceArchive::Result res = WrapArchiveBuffer(index_map, index_length, true,
                                                          data_map, data_length, true,
                                                          archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            if (mem_mapped_data)
                UnmapFile(data_map, data_length);
            else
                UnmapAsset(data_asset);

            if (mem_mapped_index)
                UnmapFile(index_map, index_length);
            else
                UnmapAsset(index_asset);

            if (res == dmResourceArchive::RESULT_VERSION_MISMATCH)
                return RESULT_VERSION_MISMATCH;
            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->index_asset = index_asset;
        info->data_asset = data_asset;
        info->data_map = mem_mapped_data ? data_map : 0;
        info->data_length = data_length;
        info->index_map = mem_mapped_index ? index_map : 0;
        info->index_length = index_length;
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
        else if (info->index_map)
        {
            UnmapFile(info->index_map, info->index_length);
        }

        if (info->data_asset)
        {
            UnmapAsset(info->data_asset);
        } else if(info->data_map)
        {
            UnmapFile(info->data_map, info->data_length);
        }

        delete info;

        dmResourceArchive::Delete(archive);
    }
}
