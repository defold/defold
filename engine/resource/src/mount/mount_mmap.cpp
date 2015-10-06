#include <dlib/log.h>
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
        void *map;
        uint64_t length;
    };

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
