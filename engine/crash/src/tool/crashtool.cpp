
#include <stdio.h>

#include "crash.h"
#include "crash_private.h"

#include <algorithm> // find_if
#include <cctype>
#include <iostream>
#include <functional>
#include <stdexcept>
#include <stdio.h>
#include <string>


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

static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}


#if defined(_WIN32)

#include <Windows.h>
#include <Dbghelp.h>
struct PlatformInfo
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    SYMBOL_INFO* symbol;
};

int PlatformInit(char* path, PlatformInfo& info)
{
    if( path )
    {
        memset(&info.si, 0, sizeof(info.si));
        memset(&info.pi, 0, sizeof(info.pi));
        info.si.cb = sizeof(STARTUPINFO);
        info.si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        info.si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
        info.si.dwFlags |= STARTF_USESTDHANDLES;

        //if( !CreateProcess(0, path, 0, 0, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, 0, 0, &info.si, &info.pi) )
        if( !CreateProcess(0, path, 0, 0, TRUE, PROCESS_VM_READ, 0, 0, &info.si, &info.pi) )
        {
            char* msg;
            DWORD err = GetLastError();
            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, err, LANG_NEUTRAL, (LPSTR) &msg, 0, 0);
            fprintf(stderr, "Failed to launch application %s: %s (%d)", path, msg, err);
            LocalFree((HLOCAL) msg);

            return 0;
        }

        info.symbol = ( SYMBOL_INFO * )calloc( sizeof( SYMBOL_INFO ) + 256 * sizeof( char ), 1 );
        info.symbol->MaxNameLen = 255;
        info.symbol->SizeOfStruct = sizeof( SYMBOL_INFO );

        SymInitialize( info.pi.hProcess, NULL, TRUE );
    }

    return 1;
}

void PlatformExit(PlatformInfo& info)
{
    TerminateProcess( info.pi.hProcess, 0 );
	WaitForSingleObject( info.pi.hProcess, 0);
    CloseHandle( info.pi.hProcess );
    CloseHandle( info.pi.hThread );

    free( info.symbol );
}

const char* PlatformGetSymbol(PlatformInfo& info, uintptr_t ptr)
{
	if(!SymFromAddr( info.pi.hProcess, ( DWORD64 )( ptr ), 0, info.symbol ))
	{
		return 0;
	}
    //printf( "%i: %s - 0x%0X\n", frames - i - 1, symbol->Name, symbol->Address );
    return info.symbol->Name;
}

// http://stackoverflow.com/questions/17389968/get-linux-executable-load-address-builtin-return-address-and-addr2line
// _AddressOfReturnAddress, GetModuleInformation + lpBaseOfDll

/*
#define PSAPI_VERSION 1
#include <Psapi.h>
DWORD_PTR GetProcessBaseAddress( PlatformInfo& info, DWORD processID )
{
    HMODULE hMods[1024];
    HANDLE hProcess;
    DWORD cbNeeded;

    hProcess = info.pi.hProcess;

printf("HELLO 2\n");
	if( EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded))
	{
    	for ( size_t i = 0; i < (cbNeeded / sizeof(HMODULE)); i++ )
        {
            TCHAR szModName[MAX_PATH];

            // Get the full path to the module's file.

            if ( GetModuleFileNameEx( hProcess, hMods[i], szModName,
                                      sizeof(szModName) / sizeof(TCHAR)))
            {
                // Print the module name and handle value.

                printf("\t%s (0x%08X)\n", szModName, (uint32_t)hMods[i] );
            }
        }
    }

printf("HELLO 3\n");
    //CloseHandle( hProcess );
    return 0;
}
*/

#include <Psapi.h>
uintptr_t GetProcessBaseAddress(PlatformInfo& pinfo, DWORD processID)
{
    HANDLE process = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID );;
    HMODULE mods[1024];
    DWORD needed;

    if (EnumProcessModules(process, mods, sizeof(mods), &needed))
    {
        uint32_t count = needed / sizeof(HANDLE);
        for (uint32_t i=0;i!=count;i++)
        {
        	char path[512];
            if (!GetModuleFileNameExA(process, mods[i], path, 512))
            {
            	fprintf(stderr, "Failed to get module filename");
            }
            
            fprintf(stderr, "module: %s\n", path);

            MODULEINFO info;
            if (GetModuleInformation(process, mods[i], &info, sizeof(info)))
            {
            fprintf(stderr, "  size: 0x%08x\n", (uintptr_t)info.lpBaseOfDll);
            }
        }
    }
    else
    {
        fprintf(stderr, "Failed to enumerate process modules\n");

        char* msg;
        DWORD err = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY, 0, err, LANG_NEUTRAL, (LPSTR) &msg, 0, 0);
        fprintf(stderr, "Error: %s (%d)\n", msg, err);
        LocalFree((HLOCAL) msg);
    }

    CloseHandle(process);

    return 0;
}

#elif defined(__APPLE__)

struct PlatformInfo
{
    const char* executable_path;
    const char* basename; // "e.g. dmengine"

    char buffer[2048];
};


static int ExtractDSym(char* executable_path, char* targetpath)
{
    printf("Extracting symbols from %s to %s\n", executable_path, targetpath);

    char cmd[2048] = {0};
    sprintf(cmd, "dsymutil -o %s %s", targetpath, executable_path);

    printf("%s\n", cmd);

    std::string output;
    int result = Exec(cmd, output);
    return result;
}


int PlatformInit(char* executable_path, PlatformInfo& info)
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
        sprintf(buffer, "%s.dSYM", info.basename);

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


const char* PlatformGetSymbol(PlatformInfo& info, uintptr_t ptr)
{
    if( !info.executable_path )
    {
        return "<unknown>";
    }

    char cmd[2048];
    sprintf(cmd, "atos -o %s.dSYM/Contents/Resources/DWARF/%s %p", info.basename, info.basename, (void*)ptr);

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

    char buffer[2048];
};

int PlatformInit(char* executable_path, PlatformInfo& info)
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

const char* PlatformGetSymbol(PlatformInfo& info, uintptr_t ptr)
{
    if( !info.executable_path )
    {
        return "<unknown>";
    }

    char cmd[2048];
    sprintf(cmd, "addr2line -e %s %p", info.executable_path, (void*)ptr);

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
	if( !PlatformInit( argc >= 2 ? argv[2] : 0, info) )
	{
        fprintf(stderr, "Failed to initialize.\n");
		return 1;
	}

    int ret = 0;

    dmCrash::HDump dump = dmCrash::LoadPreviousPath(argv[1]);
    if(dump)
    {
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
    		const char* value = dmCrash::GetSysField(dump, (dmCrash::SysField)i);
    		printf("%26s: %s\n", names[i], value);

            if( strcmp(names[i], "SYSTEM_NAME") == 0 )
            {
                if( strcmp(value, "Darwin") == 0 )
                    platform = PLATFORM_DARWIN;
                else if( strcmp(value, "Linux") == 0 )
                    platform = PLATFORM_LINUX;
            }
    	}

        printf("\n");
        printf("%26s: %d\n", "SIG", dmCrash::GetSignum(dump));

    	printf("%26s:\n", "USERDATA");
    	for( int i = 0; i < (int)dmCrash::AppState::USERDATA_SLOTS; ++i)
    	{
    		const char* value = dmCrash::GetUserField(dump, i);
    		if(!value || strcmp(value, "")==0)
    		{
    			break;
    		}
    		printf("%02d: %s\n", i, value);
    	}

        printf("\n%s:\n", "BACKTRACE");


        int frames = dmCrash::GetBacktraceAddrCount(dump);
        for( int i = 0; i < frames; ++i )
        {
            uintptr_t ptr = (uintptr_t)dmCrash::GetBacktraceAddr(dump, i);

            const char* value = PlatformGetSymbol(info, ptr);
            printf("%02d  0x%016llX: %s\n", frames - i - 1, (unsigned long long)ptr, value ? value : "<null>");
        }


        printf("\n%s:\n", "EXTRA DATA");

        const char* extra_data = dmCrash::GetExtraData(dump);
        printf("%s\n", extra_data ? extra_data : "<null>");


        printf("\n%s:\n", "MODULES");
        for( int i = 0; i < (int)dmCrash::AppState::MODULES_MAX; ++i)
        {
            const char* modulename = dmCrash::GetModuleName(dump, i);
            if( modulename == 0 )
            {
                break;
            }
            uintptr_t ptr = (uintptr_t)dmCrash::GetModuleAddr(dump, i);
            printf("%02d: %s  0x%016llx\n", i, modulename ? modulename : "<null>", (unsigned long long)ptr);
        }
        printf("\n");

        // Win32 try outs
    // uintptr_t moduleaddr = (uintptr_t)GetProcessBaseAddress(info, info.pi.dwProcessId);
    // printf("MODULE BASE ADDR: 0x%016lx\n", moduleaddr);

        // printf("%s:\n", "BACKTRACE");

        // int frames = dmCrash::GetBacktraceAddrCount(dump);
        // for( int i = 0; i < frames; ++i )
        // {
        //  uintptr_t ptr = (uintptr_t)dmCrash::GetBacktraceAddr(dump, i);
        //  ptr -= 0x330000;

        //  ptr += moduleaddr;

        //  const char* value = PlatformGetSymbol(info, ptr);
        //  printf("0x%016lX: %02d: %s\n", (uintptr_t)ptr, frames - i - 1, value ? value : "<null>");
        // }



    	dmCrash::Release(dump);
    }
    else
    {
    	fprintf(stderr, "Failed to load crash dump: %s\n", argv[2]);
    	ret = 1;
    }

    PlatformExit(info);
	return ret;
}




