#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h> // readlink
#include <libgen.h> // dirname
#include <sys/auxv.h> // getauxval
#include <sys/utsname.h>
#include <sys/stat.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/sys.h>

#include <nn/oe.h> // language
#include <nn/crypto.h>

// void nn::fs::SetSaveDataRootPath	(	const char * 	rootPath	)
// https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Api/HtmlNX/namespacenn_1_1fs.html#a90c27aaf70aec66968715663cd8c5415

namespace dmSys
{
    void FillTimeZone(struct SystemInfo* info)
    {
        time_t t;
        time(&t);
        struct tm* lt = localtime(&t);
        info->m_GmtOffset = lt->tm_gmtoff / 60;
    }

    void GetSystemInfo(SystemInfo* info)
    {
        memset(info, 0, sizeof(*info));

        dmStrlCpy(info->m_SystemName, "Switch", sizeof(info->m_SystemName));
        dmStrlCpy(info->m_SystemVersion, "1.0", sizeof(info->m_SystemVersion)); // There's a debug fn for firmware, but I don't think we'll need that
        info->m_DeviceModel[0] = '\0';

        nn::settings::LanguageCode lang_code = nn::oe::GetDesiredLanguage();
        FillLanguageTerritory(lang_code.string, info);
        FillTimeZone(info);
    }

    bool GetApplicationInfo(const char* id, ApplicationInfo* info)
    {
        memset(info, 0, sizeof(*info));
        return false;
    }

    Result OpenURL(const char* url)
    {
    	// find a way to launch a browser window
        return RESULT_UNKNOWN;
    }

    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len)
    {
        (void)application_name;
        if (dmStrlCpy(path, "save:/", path_len) >= path_len)
            return RESULT_INVAL;
        return RESULT_OK;
    }

    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
        (void)application_name;
        if (dmStrlCpy(path, "fscache:/", path_len) >= path_len)
            return RESULT_INVAL;
        return RESULT_OK;
    }

    Result GetApplicationPath(char* path_out, uint32_t path_len)
    {
        ssize_t ret = readlink("/proc/self/exe", path_out, path_len);
        if (ret < 0 || ret > path_len)
        {
            const char* relative_path = (const char*)getauxval(AT_EXECFN); // Pathname used to execute program
            if (!relative_path)
            {
                path_out[0] = '.';
                path_out[1] = '\n';
            }
            else
            {
                char *absolute_path = realpath(relative_path, NULL); // realpath() resolve a pathname
                if (!absolute_path)
                {
                    path_out[0] = '.';
                    path_out[1] = '\n';
                }
                else
                {
                    if (dmStrlCpy(path_out, dirname(absolute_path), path_len) >= path_len) // dirname() returns the string up to, but not including, the final '/'
                    {
                        path_out[0] = '.';
                        path_out[1] = '\n';
                    }
                    free(absolute_path);
                }
            }
        }
        return RESULT_OK;
    }

    // Default
    Result GetLogPath(char* path, uint32_t path_len)
    {
        if (dmStrlCpy(path, "fscache:/", path_len) >= path_len)
            return RESULT_INVAL;

        return RESULT_OK;
    }

    Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len)
    {
        assert(path_len > 0);
        path[0] = '\0';
        dmPath::Dirname(argv[0], path, path_len);
        return RESULT_OK;
    }

    Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size)
    {
        *resource_size = 0;
        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {
            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }
            if ((uint32_t) file_stat.st_size > buffer_size) {
                return RESULT_INVAL;
            }
            FILE* f = fopen(path, "rb");
            size_t nread = fread(buffer, 1, file_stat.st_size, f);
            fclose(f);
            if (nread != (size_t) file_stat.st_size) {
                return RESULT_IO;
            }
            *resource_size = file_stat.st_size;
            return RESULT_OK;
        } else {
            return NativeToResult(errno);
        }
    }

    Result ResourceSize(const char* path, uint32_t* resource_size)
    {
        struct stat file_stat;
        if (stat(path, &file_stat) == 0) {

            if (!S_ISREG(file_stat.st_mode)) {
                return RESULT_NOENT;
            }
            *resource_size = (uint32_t) file_stat.st_size;
            return RESULT_OK;
        } else {
            return RESULT_NOENT;
        }
    }

} // namespace

// Used in mbedtls to generate random numbers
extern "C" void dlib_get_random(void* output, size_t len)
{
    nn::crypto::GenerateCryptographicallyRandomBytes(output, len);
}
