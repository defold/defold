#include "crash_private.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <stdio.h>

namespace dmCrash
{
    void WriteCrash(const char* file_name, AppState *data, void (*write_extra)(int))
    {
        int f = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (f < 0)
        {
            return;
        }

        struct
        {
            uint32_t version;
            uint32_t struct_size;
        } hdr;

        hdr.version = 1;
        hdr.struct_size = sizeof(AppState);

        if (write(f, &hdr, sizeof(hdr)) != sizeof(hdr) ||
            write(f, data, sizeof(AppState)) != sizeof(AppState))
        {
            // failed to write; just erase
            close(f);
            unlink(file_name);
        }

        if (write_extra)
        {
            write_extra(f);
        }

        close(f);
    }
}
