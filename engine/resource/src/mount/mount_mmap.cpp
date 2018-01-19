#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

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
        void *lu_data_map;
        uint64_t lu_data_length;
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
        if (map)
        {
            munmap(map, size);
        }
        return RESULT_OK;
    }

    Result MountArchiveInternal(const char* index_path, const char* data_path, const char* lu_data_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        dmLogInfo("=========================");
        dmLogInfo("MOUNTING MMAP");
        dmLogInfo("index: %s", index_path);
        dmLogInfo("data: %s", data_path);
        dmLogInfo("lu data: %s", lu_data_path);
        dmLogInfo("=========================");

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
            dmLogError("Error when mapping data file, result: %i", r);
            return r;
        }

        void* lu_data_map = 0x0;
        uint32_t lu_data_size = 0;
        FILE* lu_data_file = 0x0;
        if (lu_data_path != 0x0)
        {
            r = MapFile(lu_data_path, lu_data_map, lu_data_size);
            if (r != RESULT_OK)
            {
                munmap(index_map, index_size);
                munmap(data_map, data_size);
                dmLogError("Error mapping liveupdate data file");
                return RESULT_IO_ERROR;
            }

            lu_data_file = fopen(lu_data_path, "rb+");
            if (!lu_data_file)
            {
                munmap(index_map, index_size);
                munmap(data_map, data_size);
                munmap(lu_data_map, lu_data_size);
                dmLogError("Error opening liveupdate data file");
                return RESULT_IO_ERROR;
            }
        }

        dmResourceArchive::Result res = WrapArchiveBuffer(index_map, index_size, data_map, lu_data_path, lu_data_map, lu_data_file, archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            munmap(index_map, index_size);
            munmap(data_map, data_size);
            munmap(lu_data_map, lu_data_size);
            if (lu_data_file)
            {
                fclose(lu_data_file);
            }

            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->index_map = index_map;
        info->index_length = index_size;
        info->data_map = data_map;
        info->data_length = data_size;
        info->lu_data_map = lu_data_map;
        info->lu_data_length = lu_data_size;
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

        if (info->index_map)
        {
            munmap(info->index_map, info->index_length);
        }

        if (info->data_map)
        {
            munmap(info->data_map, info->data_length);
        }

        if (info->lu_data_map)
        {
            munmap(info->lu_data_map, info->lu_data_length);
        }

        delete info;

        dmResourceArchive::Delete(archive);
    }
}
