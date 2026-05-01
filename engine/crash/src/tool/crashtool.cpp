// Copyright 2020-2026 The Defold Foundation
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

#include <stdio.h>

#include "crash.h"
#include "crash_private.h"

#include <algorithm> // find_if
#include <cctype>
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <dlib/dstrings.h>


static void Usage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "\tcrashtool <crashlog> [<dmengine.exe>]\n");
    fprintf(stderr, "\n");
}


#if !defined(_WIN32)
// http://stackoverflow.com/a/478960
static int Exec(const char* cmd, std::string& result) {
    char buffer[128];
    result = "";

    FILE* pipe = popen(cmd, "r");
    if (!pipe)
    {
        fprintf(stderr, "popen() failed!");
        return 1;
    }
    while (!feof(pipe)) {
        if (fgets(buffer, sizeof(buffer), pipe) != NULL)
            result += buffer;
    }
    pclose(pipe);
    return 0;
}
#endif

static inline std::string&  rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}


#if defined(_WIN32)

#include <Windows.h>
#include <Dbghelp.h>
#include <psapi.h>

struct PlatformInfo
{
    STARTUPINFO         si;
    PROCESS_INFORMATION pi;
    SYMBOL_INFO*        symbol;
    char                buffer[2048];

    uintptr_t           old_base_address;    // old process
    uintptr_t           loaded_base_address;  // this process

    dmCrash::HDump      dump;

    uint32_t            num_modules;
    const char*         module_names[dmCrash::AppState::MODULES_MAX];
    uintptr_t           module_addresses[dmCrash::AppState::MODULES_MAX];
};


static void PrintErrorMessage(DWORD errorCode) {
    LPVOID errorMessageBuffer = nullptr;

    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr,
        errorCode,
        0, // Default language
        reinterpret_cast<LPWSTR>(&errorMessageBuffer),
        0,
        nullptr
    );

    std::wcerr << "Error: 0x" <<  std::hex << errorCode << ": ";
    if (errorMessageBuffer) {
        std::wcerr << reinterpret_cast<LPWSTR>(errorMessageBuffer);
        LocalFree(errorMessageBuffer);
    }
    else {
        std::wcerr << "<Unknown error>";
    }
    std::wcerr << std::endl;
}

static int FindModules(PlatformInfo& info)
{
    info.num_modules = 0;

    HMODULE hMods[1024];
    DWORD cbNeeded = 0;

    // Get a list of all the modules in this process.
    if(EnumProcessModules(info.pi.hProcess, hMods, sizeof(hMods), &cbNeeded))
    {
        info.num_modules = (cbNeeded / sizeof(HMODULE));
        for (uint32_t i = 0; i < info.num_modules; i++ )
        {
            char module_name[MAX_PATH];

            // Get the full path to the module's file.
            info.module_names[i] = 0;
            info.module_addresses[i] = 0;
            if (GetModuleFileNameExA(info.pi.hProcess, hMods[i], module_name, sizeof(module_name)))
            {
                // Print the module name and handle value.
                std::cout << "Name: " << module_name << "  " << std::hex << hMods[i] << std::endl;
                info.module_names[i] = strdup(module_name);
                info.module_addresses[i] = (uintptr_t)hMods[i];
            }
        }
    }
    else
    {
        printf("Module enumeration failed. The callstack may be incorrect.\n");
        DWORD errcode = GetLastError();
        PrintErrorMessage(errcode);
    }
    return 0;
}


static int PlatformInit(const char* path, PlatformInfo& info)
{
    if( path )
    {
        memset(&info.si, 0, sizeof(info.si));
        memset(&info.pi, 0, sizeof(info.pi));
        info.si.cb = sizeof(STARTUPINFO);
        info.si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        info.si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        info.si.dwFlags |= STARTF_USESTDHANDLES;

        printf("Opening path: '%s'\n", path);

        // Allow for the missing dll dialog to show, as there's no api to get this info
        SetErrorMode(SEM_NOALIGNMENTFAULTEXCEPT);

        if( !CreateProcessA(0, (LPSTR)path, 0, 0, TRUE, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, 0, (LPSTARTUPINFOA)&info.si, &info.pi) )
        {
            DWORD errcode = GetLastError();
            PrintErrorMessage(errcode);
            return 0;
        }

        Sleep(100);

        info.symbol = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + (MAX_SYM_NAME+1) * sizeof( wchar_t ), 1 );
        info.symbol->MaxNameLen = MAX_SYM_NAME;
        info.symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

        SymSetOptions(SYMOPT_LOAD_LINES);

        if (!SymInitialize( info.pi.hProcess, NULL, TRUE ))
        {
            DWORD errcode = GetLastError();
            PrintErrorMessage(errcode);
            return 0;
        }

        FindModules(info);
    }

    return 1;
}

static void PlatformExit(PlatformInfo& info)
{
    TerminateProcess( info.pi.hProcess, 0 );
    WaitForSingleObject( info.pi.hProcess, 0);
    CloseHandle( info.pi.hProcess );
    CloseHandle( info.pi.hThread );

    free( info.symbol );
}

// https://learn.microsoft.com/en-us/windows/win32/debug/retrieving-symbol-information-by-address
//  SymGetLineFromAddr64

const char* PlatformGetSymbol(PlatformInfo& info, uintptr_t ptr, uint32_t module_index)
{
    DWORD64 old_base_address = (DWORD64)dmCrash::GetModuleAddr(info.dump, module_index);
    DWORD64 new_base_address = module_index < info.num_modules
                                ? info.module_addresses[module_index]
                                : old_base_address; // fallback to old address

    DWORD64 relative = ptr - old_base_address;
    DWORD64 address = relative + new_base_address;

    DWORD64 dwDisplacement = 0;
    if(!SymFromAddr( info.pi.hProcess, address, &dwDisplacement, info.symbol ))
    {
        DWORD errcode = GetLastError();
        PrintErrorMessage(errcode);
        return 0;
    }
    //printf( "  %s - 0x%0llX  0x%0llx\n", info.symbol->Name, ptr, dwDisplacement);

    IMAGEHLP_LINE line = {0};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE);

    const char* filename = "<null>";
    int line_number = -1;
    DWORD lineDisplacement;
    if (SymGetLineFromAddr(info.pi.hProcess, address, &lineDisplacement, &line))
    {
        filename = line.FileName;
        line_number = line.LineNumber;
    }

    dmSnPrintf(info.buffer, sizeof(info.buffer), "%s  %s(%d)", info.symbol->Name, filename, line_number );
    info.buffer[sizeof(info.buffer)-1] = 0;

    return info.buffer;
}

#elif defined(__APPLE__)

struct PlatformInfo
{
    const char* executable_path;
    const char* basename; // "e.g. dmengine"

    dmCrash::HDump dump;

    char buffer[2048];
};


static int ExtractDSym(const char* executable_path, char* targetpath)
{
    printf("Extracting symbols from %s to %s\n", executable_path, targetpath);

    char cmd[2048] = {0};
    dmSnPrintf(cmd, sizeof(cmd), "dsymutil -o %s %s", targetpath, executable_path);

    printf("%s\n", cmd);

    std::string output;
    int result = Exec(cmd, output);
    return result;
}


int PlatformInit(const char* executable_path, PlatformInfo& info)
{
    info.basename = 0;
    info.executable_path = executable_path;
    if( executable_path )
    {
        info.basename = strrchr(info.executable_path, '/');
        if (!info.basename)
        {
            info.basename = info.executable_path;
        } else
        {
            info.basename++; // Step past the '/'
        }

        char buffer[1024] = {0};
        dmSnPrintf(buffer, sizeof(buffer), "%s.dSYM", info.basename);

        if( ExtractDSym(executable_path, buffer) )
        {
            fprintf(stderr, "Failed to extract symbols\n");
            return 0;
        }
    }
    return 1;
}

void PlatformExit(PlatformInfo& info)
{
}


const char* PlatformGetSymbol(PlatformInfo& info, uintptr_t ptr, uint32_t module_index)
{
    if( !info.executable_path )
    {
        return "<unknown>";
    }

    char cmd[2048];
    dmSnPrintf(cmd, sizeof(cmd), "atos -o %s.dSYM/Contents/Resources/DWARF/%s %p", info.basename, info.basename, (void*)ptr);

    std::string output;
    int result = Exec(cmd, output);
    if( result )
    {
        return "<unknown>";
    }

    output = rtrim(output);

    snprintf(info.buffer, sizeof(info.buffer), "%s", output.c_str() );
    info.buffer[sizeof(info.buffer)-1] = 0;

    return info.buffer;
}

#elif defined(__linux__)

struct PlatformInfo
{
    const char* executable_path;
    const char* basename; // "e.g. dmengine"

    dmCrash::HDump dump;

    char buffer[2048];
};

int PlatformInit(const char* executable_path, PlatformInfo& info)
{
    info.basename = 0;
    info.executable_path = executable_path;
    if( executable_path )
    {
        info.basename = strrchr(info.executable_path, '/');
        if (!info.basename)
        {
            info.basename = info.executable_path;
        } else
        {
            info.basename++; // Step past the '/'
        }
    }
    return 1;
}

void PlatformExit(PlatformInfo& info)
{
}

const char* PlatformGetSymbol(PlatformInfo& info, uintptr_t ptr, uint32_t module_index)
{
    if( !info.executable_path )
    {
        return "<unknown>";
    }

    char cmd[2048];
    dmSnPrintf(cmd, sizeof(cmd), "addr2line -e %s %p", info.executable_path, (void*)ptr);

    std::string output;
    int result = Exec(cmd, output);
    if( result )
    {
        return "<unknown>";
    }

    output = rtrim(output);

    snprintf(info.buffer, sizeof(info.buffer), "%s", output.c_str() );
    info.buffer[sizeof(info.buffer)-1] = 0;

    return info.buffer;
}

#endif


enum Platforms
{
    PLATFORM_UNKNOWN = -1,
    PLATFORM_WINDOWS,
    PLATFORM_DARWIN,
    PLATFORM_LINUX,
};



int main(int argc, char** argv)
{
    if( argc < 2 )
    {
        Usage();
        return 1;
    }

    PlatformInfo info;
    const char* path = argc >= 2 ? argv[2] : 0;
    if (!path)
    {
        fprintf(stderr, "No path to executable specified. Symbolication disabled.\n");
    }

    if( !PlatformInit(path, info) )
    {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }

    int ret = 0;

    dmCrash::HDump dump = dmCrash::LoadPreviousPath(argv[1]);
    if(dump)
    {
        info.dump = dump;

        const char* names[dmCrash::SYSFIELD_MAX] = {
           "ENGINE_VERSION",
           "ENGINE_HASH",
           "DEVICE_MODEL",
           "MANUFACTURER",
           "SYSTEM_NAME",
           "SYSTEM_VERSION",
           "LANGUAGE",
           "DEVICE_LANGUAGE",
           "TERRITORY",
           "ANDROID_BUILD_FINGERPRINT",
           };

        int platform = PLATFORM_UNKNOWN;

        for( int i = 0; i < dmCrash::SYSFIELD_MAX; ++i)
        {
            const char* value = dmCrash::GetSysField(info.dump, (dmCrash::SysField)i);
            printf("%26s: %s\n", names[i], value);

            if( strcmp(names[i], "SYSTEM_NAME") == 0 )
            {
                if( strcmp(value, "Darwin") == 0 )
                    platform = PLATFORM_DARWIN;
                else if( strcmp(value, "Linux") == 0 )
                    platform = PLATFORM_LINUX;
            }
        }
        (void)platform; // for later use

        printf("\n");
        printf("%26s: %d\n", "SIG", dmCrash::GetSignum(info.dump));

        printf("%26s:\n", "USERDATA");
        for( int i = 0; i < (int)dmCrash::AppState::USERDATA_SLOTS; ++i)
        {
            const char* value = dmCrash::GetUserField(info.dump, i);
            if(!value || strcmp(value, "")==0)
            {
                break;
            }
            printf("%02d: %s\n", i, value);
        }

        printf("\n%s:\n", "BACKTRACE");


        int frames = dmCrash::GetBacktraceAddrCount(info.dump);
        for( int i = 0; i < frames; ++i )
        {
            uintptr_t ptr = (uintptr_t)dmCrash::GetBacktraceAddr(info.dump, i);
            uint32_t module_index = dmCrash::GetBacktraceModuleIndex(info.dump, i);

            const char* value = PlatformGetSymbol(info, ptr, module_index);
            printf("%02d  0x%016llX: %s\n", frames - i - 1, (unsigned long long)ptr, value ? value : "<null>");
        }


        printf("\n%s:\n", "EXTRA DATA");

        const char* extra_data = dmCrash::GetExtraData(info.dump);
        printf("%s\n", extra_data ? extra_data : "<null>");


        printf("\n%s:\n", "MODULES");
        for( int i = 0; i < (int)dmCrash::AppState::MODULES_MAX; ++i)
        {
            const char* modulename = dmCrash::GetModuleName(info.dump, i);
            if( modulename == 0 )
            {
                break;
            }
            uintptr_t ptr = (uintptr_t)dmCrash::GetModuleAddr(info.dump, i);
            printf("%02d: %s  0x%016llx\n", i, modulename ? modulename : "<null>", (unsigned long long)ptr);
        }
        printf("\n");

        // // Win32 try outs
        // {
        //     uintptr_t moduleaddr = (uintptr_t)GetProcessBaseAddress(info, info.pi.dwProcessId);
        //     printf("MODULE BASE ADDR: 0x%016llx\n", moduleaddr);

        //     printf("%s:\n", "BACKTRACE");

        //     int frames = dmCrash::GetBacktraceAddrCount(info.dump);
        //     for( int i = 0; i < frames; ++i )
        //     {
        //      uintptr_t ptr = (uintptr_t)dmCrash::GetBacktraceAddr(info.dump, i);
        //      ptr -= 0x330000;

        //      ptr += moduleaddr;

        //      const char* value = PlatformGetSymbol(info, ptr);
        //      printf("0x%016llX: %02d: %s\n", (uintptr_t)ptr, frames - i - 1, value ? value : "<null>");
        //     }
        // }


        dmCrash::Release(info.dump);
    }
    else
    {
        fprintf(stderr, "Failed to load crash dump: %s\n", argv[2]);
        ret = 1;
    }

    PlatformExit(info);
    return ret;
}
