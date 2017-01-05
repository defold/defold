#include <dlib/log.h>
#include <dlib/path.h>
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
    struct MountInfo2
    {
        void *index_map;
        uint64_t index_length;
        void *data_map;
        uint64_t data_length;
    };

    struct MountInfo
    {
        void *map;
        uint64_t length;
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

    Result MountArchiveInternal(const char* index_path, dmResourceArchive::HArchiveIndexContainer* archive, void** mount_info)
    {
        // Derive path of arcd file from path to arci
        char data_path[DMPATH_MAX_PATH];
        memcpy(&data_path, index_path, strlen(index_path)+1); // copy NULL terminator as well
        data_path[strlen(index_path)-1] = 'd';

        void* index_map = 0x0;
        uint32_t index_size = 0;
        Result r = MapFile(index_path, index_map, index_size);

        if (r != RESULT_OK)
        {
            dmLogInfo("Error when mapping index file, result: %i", r);
            return r;
        }

        void* data_map = 0x0;
        uint32_t data_size = 0;
        r = MapFile(data_path, data_map, data_size);

        if (r != RESULT_OK)
        {
            munmap(index_map, index_size);
            dmLogInfo("Error when mapping data file, result: %i", r);
            return r;
        }

        dmResourceArchive::Result res = WrapArchiveBuffer2(index_map, index_size, data_map, archive);
        if (res != dmResourceArchive::RESULT_OK)
        {
            munmap(index_map, index_size);
            munmap(data_map, data_size);
            return RESULT_IO_ERROR;
        }

        MountInfo2* info = new MountInfo2();
        info->index_map = index_map;
        info->index_length = index_size;
        info->data_map = data_map;
        info->data_length = data_size;
        *mount_info = (void*)info;

        return RESULT_OK;
    }

    Result MountArchiveInternal(const char* path, dmResourceArchive::HArchive* archive, void** mount_info)
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

        void *map = mmap(0, fs.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);

        if (!map)
        {
            return RESULT_IO_ERROR;
        }

        dmResourceArchive::Result r = WrapArchiveBuffer(map, fs.st_size, archive);
        if (r != dmResourceArchive::RESULT_OK)
        {
            munmap(map, fs.st_size);
            return RESULT_IO_ERROR;
        }

        MountInfo* info = new MountInfo();
        info->map = map;
        info->length = fs.st_size;
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

        if (info->index_map)
        {
            munmap(info->index_map, info->index_length);
        }

        if (info->data_map)
        {
            munmap(info->data_map, info->data_length);
        }

        delete info;
    }

    void UnmountArchiveInternal(dmResourceArchive::HArchive archive, void* mount_info)
    {
        MountInfo* info = (MountInfo*) mount_info;
        if (info)
        {
           munmap(info->map, info->length);
           delete info;
        }
    }
}
