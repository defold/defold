#include "crash_private.h"

#include <windows.h>

#include <dlib/log.h>

namespace dmCrash
{
    void WriteCrash(const char* file_name, AppState* data)
    {
        HANDLE fhandle = CreateFileA(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fhandle != NULL)
        {
            DWORD written;
            AppStateHeader header;
            header.version = AppState::VERSION;
            header.struct_size = sizeof(AppState);

            if (WriteFile(fhandle, &header, sizeof(AppStateHeader), &written, 0))
            {
                if (WriteFile(fhandle, data, sizeof(AppState), &written, 0))
                {
                    dmLogInfo("Successfully wrote Crashdump to file: %s", file_name);
                    CloseHandle(fhandle);
                }
                else
                {
                    dmLogError("Failed to write Crashdump content.");
                    CloseHandle(fhandle);
                }
            }
            else
            {
                dmLogError("Failed to write Crashdump header.");
                CloseHandle(fhandle);
            }
        }
        else
        {
            dmLogError("Failed to write Crashdump file.");
        }
    }
}
