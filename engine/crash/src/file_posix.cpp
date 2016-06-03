#include "crash_private.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlib/log.h>

#include <stdio.h>

namespace dmCrash
{
    void WriteCrash(const char* file_name, AppState* data)
    {
        int fhandle = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fhandle != -1)
        {
            AppStateHeader header;
            header.version = AppState::VERSION;
            header.struct_size = sizeof(AppState);

            if (write(fhandle, &header, sizeof(AppStateHeader)) == sizeof(AppStateHeader))
            {
                if (write(fhandle, data, sizeof(AppState)) == sizeof(AppState))
                {
                    dmLogInfo("Successfully wrote Crashdump to file: %s", file_name);
                    close(fhandle);
                }
                else
                {
                    dmLogError("Failed to write Crashdump content.");
                    close(fhandle);
                    unlink(file_name);
                }
            }
            else
            {
                dmLogError("Failed to write Crashdump header.");
                close(fhandle);
                unlink(file_name);
            }
        }
        else
        {
            dmLogError("Failed to write Crashdump file.");
        }
    }
}
