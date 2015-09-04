#include "crash_private.h"

#include <windows.h>


namespace dmCrash
{
    void WriteCrash(const char* file_name, AppState *data, void (*write_extra)(int))
    {
        HANDLE file = CreateFileA(file_name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (!file)
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

        DWORD written;
        WriteFile(file, &hdr, sizeof(hdr), &written, 0);
        WriteFile(file, data, sizeof(AppState), &written, 0);
        CloseHandle(file);
    }
}
