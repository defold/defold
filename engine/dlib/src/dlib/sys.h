// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_SYS_H
#define DM_SYS_H

#include <string.h>
#include <stdlib.h> // free

#include <dmsdk/dlib/sys.h>

namespace dmSys
{
    /**
     * Network connectivity
     */
    enum NetworkConnectivity
    {
        NETWORK_DISCONNECTED       = 0,
        NETWORK_CONNECTED          = 1,
        NETWORK_CONNECTED_CELLULAR = 2
    };

    /**
     * System information
     */
    struct SystemInfo
    {
        SystemInfo()
        {
            memset(this, 0, sizeof(*this));
        }
        ~SystemInfo()
        {
            if(m_UserAgent)
            {
                free((void*)m_UserAgent); // Allocated in dmSysGetUserAgent
            }
        }

        /// Device model where applicable, e.g. iPhone3,1
        char m_DeviceModel[32];
        /// Device manufacturer if available
        char m_Manufacturer[32];
        /// System name, hardcoded per platform e.g. "Darwin", "iPhone OS", "Linux", etc
        char m_SystemName[32];
        /// System version, e.g. 12.0.1
        char m_SystemVersion[32];
        /// Api version, e.g. 23 for android, 9.1 for iOS, empty for platforms with no concept of an api or sdk
        char m_ApiVersion[32];
        /// ISO 639 language code
        char m_Language[8];
        /// ISO 639 device language code and optional dash (–) followed by an ISO 15924 script code. Reflects UI language and typically same as m_Language.
        char m_DeviceLanguage[16];
        ///  ISO 3166 country code
        char m_Territory[8];
        /// Offset to GMT in minutes
        int  m_GmtOffset;
        /// Unique device identifier, e.g. "identifierForVendor" on iOS
        char m_DeviceIdentifier[64];
        /// The string returned from the browser (allocated, has to be free'd)
        const char* m_UserAgent;
    };

    /**
     * Engine information
     */
    struct EngineInfo
    {
        EngineInfo()
        {
            memset(this, 0, sizeof(EngineInfo));
        }

        /// Engine platform
        char m_Platform[16];
        /// Engine version, e.g. 1.2.1
        char m_Version[16];
        /// Engine version hash string
        char m_VersionSHA1[40+1];
        /// Engine debug/release
        bool m_IsDebug;
    };

    /**
     * SetEngineInfo function parameters
     */
    struct EngineInfoParam
    {
        const char* m_Platform;
        const char* m_Version;
        const char* m_VersionSHA1;
        bool m_IsDebug;
    };

    /**
     * App information
     */
    struct ApplicationInfo
    {
        ApplicationInfo()
        {
            memset(this, 0, sizeof(ApplicationInfo));
        }

        /// True if the app is installed on this platform device
        bool m_Installed;
    };

    /**
     * Create directory.
     * @param path path to directory to create
     * @param mode initial unix file permissions. ignored on some platforms
     * @return RESULT_OK on success
     */
    Result Mkdir(const char* path, uint32_t mode);

    /**
     * Remove empty directory
     * @param path path to directory to remove
     * @return RESULT_OK on success
     */
    Result Rmdir(const char* path);

    /**
     * Check if path exists and is a directory
     * @param path path to directory
     * @return RESULT_OK on success
     */
    Result IsDir(const char* path);

    /**
     * Remove directory tree structure and files within
     * @param path path to directory to remove
     * @return RESULT_OK on success
     */
    Result RmTree(const char* path);

    /**
     * Iterate a file tree and callback for each file entry
     * @param path path to directory to iterate
     * @param recursive if true, the function traverses the child directories as well
     * @param call_before if true, the callback is called before entering the children
     * @param callback callback that gets called for each entry. The "isdir" is true for directories, and false for file entries.
     * @return RESULT_OK on success
     */
    Result IterateTree(const char* path, bool recursive, bool call_before, void* ctx, void (*callback)(void* ctx, const char* path, bool isdir));

    /**
     * Given a path, resolves the path to the path that is first found, given the current file mounts
     * @return RESULT_NOENT if no such file
     */
    Result ResolveMountFileName(char* buffer, size_t buffer_size, const char* path);

    /**
     * Prefixes a path with the correct host mount
     * Used during development to read/write files on the host PC
     */
    Result GetHostFileName(char* buffer, size_t buffer_size, const char* path);

    /**
     * Get the path where the application (exe) is located
     * Example paths:
     * Switch: data://
     * @param path path buffer
     * @path path_len path buffer length
     * @return RESULT_OK success. RESULT_INVAL if the supplied path is too short. Other error results are also possible.
     */
    Result GetApplicationPath(char* path, uint32_t path_len);

    /**
     * Get and create platform specific application support directory.
     * Example paths:
     * OSX: ~/Library/Application Support/APPLICATION_NAME
     * Windows: C:\Documents and Settings\USERNAME\Application Data\APPLICATION_NAME
     * Linux: ~/.APPLICATION_NAME
     * Switch: cache://
     * @param application_name application name to get path for. This is the name of "your" application.
     * @param path path buffer
     * @param path_len path buffer length
     * @return RESULT_OK success. RESULT_INVAL if the supplied path is too short. General IO-errors could result in other
     * codes, e.g. RESULT_ACCES if permission is denied.
     */
    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len);

    /**
     * Get and create platform specific application save directory.
     * For Switch, it returns the path, prefixed with "save://"
     * Example paths:
     * OSX: ~/Library/Application Support/APPLICATION_NAME
     * Windows: C:\Documents and Settings\USERNAME\Application Data\APPLICATION_NAME
     * Linux: ~/.APPLICATION_NAME
     * Switch: save:/
     * @param application_name application name to get path for. This is the name of "your" application.
     * @param path path buffer
     * @param path_len path buffer length
     * @return RESULT_OK success. RESULT_INVAL if the supplied path is too short. General IO-errors could result in other
     * codes, e.g. RESULT_ACCES if permission is denied.
     */
    Result GetApplicationSavePath(const char* application_name, char* path, uint32_t path_len);

    /**
     * Get resource directory path. On iOS the bundle directory is returned whereas on MacOSX
     * the "Resources"-directory within the bundle is returned. If no bundle exists the directory
     * to where the executable is located is returned.
     * On other platforms the directory to the executable is always returned.
     * @param argc argc from main()
     * @param argv argv from main()
     * @param path output path
     * @param path_len output path length
     * @return RESULT_OK on success
     */
    Result GetResourcesPath(int argc, char* argv[], char* path, uint32_t path_len);

    /**
     * Get path to where log-files should be written.
     * Platform notes:
     * <ul>
     * <li>iOS: Saved to documents folder. To sync with iTunes UIFileSharingEnabled must be set to true
     * <li>Android: Saved to external storage. android.permission.WRITE_EXTERNAL_STORAGE must be set in AndroidManifest.xml
     * <li>Other: Saved to current working directory
     * </ul>
     * @param path
     * @param path_len
     * @return
     */
    Result GetLogPath(char* path, uint32_t path_len);


    /**
     * Get system information
     * @param info input data
     */
    void GetSystemInfo(SystemInfo* info);

    /**
     * Get information which may be secure on some platforms
     * @param info input data
     */
    void GetSecureInfo(SystemInfo* info);

    /**
     * Get engine information
     * @param info input data
     */
    void GetEngineInfo(EngineInfo* info);

    /**
     * Get information of application with id if this is installed on this platform device
     * @param id appliaction id. Platform specific format
     * @param info input data
     * @return true if the application with id is installed on this platform device and information could successfully be retreived
     */
    bool GetApplicationInfo(const char* id, ApplicationInfo* info);

    /**
     * Set engine information
     * @param param engine info parameters
     */
    void SetEngineInfo(EngineInfoParam& param);

    /**
     * Check if a resource exists. That path supplied should
     * be prepended by the path returned from GetResourcesPath()
     * @note LoadResource can only operate on local filesystem
     * @param path path to check
     * @return true if resource exists
     */
    bool ResourceExists(const char* path);

    /**
     * Get resource sie
     * @param path
     * @param resource_size
     * @return RESULT_OK on success. RESULT_NOENT if the file doesn't exists or isn't a regular file.
     */
    Result ResourceSize(const char* path, uint32_t* resource_size);

    /**
     * Load resource. That path supplied should
     * be prepended by the path returned from GetResourcesPath()
     * @note LoadResource can only operate on local filesystem
     * @param path path
     * @param buffer buffer
     * @param buffer_size buffer size
     * @param resource_size actual resource size
     * @return RESULT_OK on success. RESULT_INVAL if the buffer is too small. RESULT_NOENT if the file doesn't exists or isn't a regular file.
     */
    Result LoadResource(const char* path, void* buffer, uint32_t buffer_size, uint32_t* resource_size);

    /**
     * Open URL in default application
     * @param url url to open
     * @param target target specifies where to open url
     * @return RESULT_OK on success
     */
    Result OpenURL(const char* url, const char* target);


    /**
     * Set host to check for connectivity against.
     * @param host host to check against
     */
    void SetNetworkConnectivityHost(const char* host);

    /**
     * Get current network connectivity status
     * @return NETWORK_DISCONNECTED if no network connection is found, NETWORK_CONNECTED_CELLULAR if the only network connection is through mobile cellular, otherwise NETWORK_CONNECTED.
     */
    NetworkConnectivity GetNetworkConnectivity();

    /**
     * Causes message events to be dispatched, on platforms that require it (currently only node/headless)
     */
    void PumpMessageQueue();

    // private functions
    void FillLanguageTerritory(const char* lang, struct SystemInfo* info);
    Result ErrnoToResult(int r);

    /**
     * Gets an environment variable from the system
     * @return variable or 0 if not found
     */
    char* GetEnv(const char* name);
}

#endif
