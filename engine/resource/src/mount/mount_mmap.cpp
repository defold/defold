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
#include <dlib/dstrings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "resource.h"
#include "resource_archive.h"
#include "resource_private.h"

namespace dmResource
{
    struct MountInfo
    {
        void *index_map;
        uint64_t index_length;
        void *data_map;
        uint64_t data_length;
    };

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
        out_map = mmap(NULL, fs.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);

        if (!out_map || out_map == MAP_FAILED)
        {
            int err = errno;
            char errstr[128];
            dmStrError(errstr, sizeof(errstr), err);
            dmLogError("Failed to memory map file %s: errno: %d %s", path, err, errstr);
            return RESULT_IO_ERROR;
        }

        out_size = fs.st_size;

        return RESULT_OK;
    }

    Result UnmapFile(void*& map, uint32_t size)
    {
        if (map)
        {
            return munmap(map, size) == 0 ? RESULT_OK : RESULT_IO_ERROR;
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
        void* index_map = 0x0;
        uint32_t index_size = 0;
        Result r = MapFile(index_path, index_map, index_size);
        if (r != RESULT_OK)
        {
            dmLogError("Error when mapping index file, result: %i", r);
            return r;
        }

        void* data_map = 0x0;
        uint32_t data_size = 0;
        r = MapFile(data_path, data_map, data_size);
        if (r != RESULT_OK)
        {
            munmap(index_map, index_size);
            dmLogError("Error when mapping data file '%s', result = %i", data_path, r);
            return r;
        }

        dmResourceArchive::Result res = WrapArchiveBuffer(index_map, index_size, true,
                                                          data_map, data_size, true,
                                                          archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            munmap(index_map, index_size);
            munmap(data_map, data_size);
            if (res == dmResourceArchive::RESULT_VERSION_MISMATCH)
                return RESULT_VERSION_MISMATCH;
            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->index_map = index_map;
        info->index_length = index_size;
        info->data_map = data_map;
        info->data_length = data_size;
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

        if (info->index_map)
        {
            munmap(info->index_map, info->index_length);
        }

        if (info->data_map)
        {
            munmap(info->data_map, info->data_length);
        }

        delete info;

        dmResourceArchive::Delete(archive);
    }
}
