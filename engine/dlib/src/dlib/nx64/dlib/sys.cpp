#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <time.h>
#include <unistd.h> // readlink
#include <libgen.h> // dirname
#include <sys/auxv.h> // getauxval
#include <sys/utsname.h>
#include <sys/stat.h>

#include <dlib/dlib.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/sys.h>

#include <nn/oe.h> // language
#include <nn/fs.h>
#include <nn/nifm.h>
#include <nn/crypto.h>
#include <nn/account/account_Api.h>
#include <nn/account/account_ApiForApplications.h>
#include <nn/account/account_Result.h>
#include <nn/account/account_Selector.h>

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
        if (dmStrlCpy(path_out, "data:/", path_len) >= path_len)
            return RESULT_INVAL;
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
        if (dmStrlCpy(path, "data:/", path_len) >= path_len)
            return RESULT_INVAL;
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

    bool ResourceExists(const char* path)
    {
        struct stat file_stat;
        return stat(path, &file_stat) == 0;
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

    Result MoveFile(const char* dst_filename, const char* src_filename)
    {
        nn::Result result = nn::fs::RenameFile(src_filename, dst_filename);

        if (result.IsSuccess()) {
            return RESULT_OK;
        }

        if (nn::fs::ResultPathNotFound::Includes(result)) {
            dmLogError("Failed to rename '%s' to '%s'. Source does not exist", src_filename, dst_filename);
            return RESULT_UNKNOWN;
        }

        if (nn::fs::ResultTargetLocked::Includes(result)) {
            dmLogError("Failed to rename '%s' to '%s'. Target was locked", src_filename, dst_filename);
            return RESULT_UNKNOWN;
        }

        if (!nn::fs::ResultPathAlreadyExists::Includes(result)) {
            dmLogError("Failed to rename '%s' to '%s'", src_filename, dst_filename);
            return RESULT_UNKNOWN;
        }

        // This behavior is specific to the NX platform
        // We need to handle renaming files gracefully

        char temp_file[DMPATH_MAX_PATH];
        dmSnPrintf(temp_file, sizeof(temp_file), "%s.mvtmp", dst_filename);

        // Move the target file to the temp file
        result = nn::fs::RenameFile(dst_filename, temp_file);
        if (result.IsFailure())
        {
            dmLogError("Failed to rename '%s' to '%s'. Couldn't move existing target to temp file '%s'", src_filename, dst_filename, temp_file);
            return RESULT_UNKNOWN;
        }

        // Move the source file to the target file
        result = nn::fs::RenameFile(src_filename, dst_filename);

        if (result.IsFailure()) {
            // If we failed, we need to move the original target file back
            result = nn::fs::RenameFile(temp_file, dst_filename);
            if (result.IsFailure()) {
                dmLogError("Failed to rename '%s' back to '%s'. And also failed to revert the naming of the old file '%s' back to '%s'", src_filename, dst_filename, temp_file, dst_filename);
            }
            else {
                dmLogError("Failed to rename '%s' to '%s'. Was able to restore old target file '%s' to '%s'", src_filename, dst_filename, temp_file, dst_filename);
            }
            return RESULT_UNKNOWN;
        }

        // We succeeded in renaming the sve file
        // We can now remove the temp file
        dmSys::Unlink(temp_file);
        return RESULT_OK;
    }

    Result ResolveMountFileName(char* buffer, size_t buffer_size, const char* path)
    {
        const char* initial_slash = path[0] != '/' ? "/" : "";

        if (dLib::IsDebugMode())
        {
            dmSnPrintf(buffer, buffer_size, "host:%s%s", initial_slash, path);
            if (dmSys::ResourceExists(buffer))
                return RESULT_OK;
        }

        dmSnPrintf(buffer, buffer_size, "data:%s%s", initial_slash, path);
        if (dmSys::ResourceExists(buffer))
            return RESULT_OK;

        return RESULT_NOENT;
    }

    void SetNetworkConnectivityHost(const char* host)
    {
        (void)host;
    }

    NetworkConnectivity GetNetworkConnectivity()
    {
        if (nn::nifm::IsNetworkRequestOnHold())
            return NETWORK_DISCONNECTED;
        return nn::nifm::IsNetworkAvailable() ? NETWORK_CONNECTED : NETWORK_DISCONNECTED;
    }


    struct NativeUserInfo
    {
        nn::account::Uid        m_User;
        nn::account::UserHandle m_UserHandle;
    };

    UserResult InitUser()
    {
        nn::account::Initialize();
        return dmSys::RESULT_USER_OK;
    }

    static nn::Result OpenUser(nn::account::Uid user, UserInfo* info)
    {
        nn::account::UserHandle userHandle;
        nn::Result result = nn::account::OpenUser(&userHandle, user);
        if (!result.IsSuccess())
        {
            dmLogWarning("Failed to open user handle\n");
            return result;
        }

        result = nn::fs::EnsureSaveData(user);
        if ( nn::fs::ResultUsableSpaceNotEnough::Includes(result) )
        {
            // Error handling when the application does not have enough memory is required for the nn::fs::EnsureSaveData() function.
            // The system automatically displays a message indicating that there was not enough capacity to create save data in the error viewer.
            // The application must offer options to cancel account selection and to return to the prior scene.
            dmLogError("Usable save space not enough.\n");
            nn::account::CloseUser(userHandle);
            return result;
        }

        result = nn::fs::MountSaveData("save", user);
        if (!result.IsSuccess())
        {
            dmLogError("Failed to mount save:/\n");
            nn::account::CloseUser(userHandle);
            return result;
        }

        NativeUserInfo* native_info = (NativeUserInfo*)malloc(sizeof(NativeUserInfo));
        native_info->m_UserHandle = userHandle;
        native_info->m_User = user;
        info->m_NativeInfo = native_info;

        nn::account::Nickname nickname;
        nn::account::GetNickname(&nickname, user);
        info->m_Name = strdup(nickname.name);

        return result;
    }

    UserResult SelectUser(UserInfo* info)
    {
        // Display the user account selection screen and make the user select a user account.
        // nn::account::ResultCancelledByUser is returned if the process is canceled by user operation.
        nn::account::Uid user;

        nn::Result result = nn::account::ShowUserSelector(&user);
        if( nn::account::ResultCancelledByUser::Includes(result) )
        {
            dmLogWarning("The user selection was canceled.\n");
            return dmSys::RESULT_USER_CANCELLED;
        }

        result = OpenUser(user, info);
        if (result.IsFailure())
        {
            dmLogWarning("Failed to open user\n");
            return dmSys::RESULT_USER_ERROR;
        }

        return dmSys::RESULT_USER_OK;
    }

    UserResult OpenLastUser(UserInfo* info)
    {
        nn::account::Uid user;
        nn::Result result = nn::account::GetLastOpenedUser(&user);
        if(result.IsFailure()) {
            return dmSys::RESULT_USER_ERROR;
        }

        if (!user) {
            dmLogError("The user does not exists anymore\n");
            return RESULT_USER_NOT_EXIST;
        }

        result = OpenUser(user, info);
        if (result.IsFailure())
        {
            dmLogWarning("Failed to open user\n");
            return dmSys::RESULT_USER_ERROR;
        }

        return dmSys::RESULT_USER_OK;
    }

    void CloseUser(UserInfo* info)
    {
        NativeUserInfo* native_info = (NativeUserInfo*)info->m_NativeInfo;
        free((void*)info->m_Name);
        free((void*)native_info);
        nn::account::CloseUser(native_info->m_UserHandle);
    }

} // namespace

// Used in mbedtls to generate random numbers
extern "C" void dlib_get_random(void* output, size_t len)
{
    nn::crypto::GenerateCryptographicallyRandomBytes(output, len);
}
