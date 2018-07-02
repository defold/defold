#ifndef DM_SYS_H
#define DM_SYS_H

#include <string.h>
#include <stdlib.h> // free

namespace dmSys
{
    /**
     * Result code. Similar to standard posix result codes
     */
    enum Result
    {
        RESULT_OK     =  0, //!< RESULT_OK
        RESULT_PERM   = -1, //!< RESULT_PERM
        RESULT_NOENT  = -2, //!< RESULT_NOENT
        RESULT_SRCH   = -3, //!< RESULT_SRCH
        RESULT_INTR   = -4, //!< RESULT_INTR
        RESULT_IO     = -5, //!< RESULT_IO
        RESULT_NXIO   = -6, //!< RESULT_NXIO
        RESULT_2BIG   = -7, //!< RESULT_2BIG
        RESULT_NOEXEC = -8, //!< RESULT_NOEXEC
        RESULT_BADF   = -9, //!< RESULT_BADF
        RESULT_CHILD  = -10,//!< RESULT_CHILD
        RESULT_DEADLK = -11,//!< RESULT_DEADLK
        RESULT_NOMEM  = -12,//!< RESULT_NOMEM
        RESULT_ACCES  = -13,//!< RESULT_ACCES
        RESULT_FAULT  = -14,//!< RESULT_FAULT
        RESULT_BUSY   = -15,//!< RESULT_BUSY
        RESULT_EXIST  = -16,//!< RESULT_EXIST
        RESULT_XDEV   = -17,//!< RESULT_XDEV
        RESULT_NODEV  = -18,//!< RESULT_NODEV
        RESULT_NOTDIR = -19,//!< RESULT_NOTDIR
        RESULT_ISDIR  = -20,//!< RESULT_ISDIR
        RESULT_INVAL  = -21,//!< RESULT_INVAL
        RESULT_NFILE  = -22,//!< RESULT_NFILE
        RESULT_MFILE  = -23,//!< RESULT_MFILE
        RESULT_NOTTY  = -24,//!< RESULT_NOTTY
        RESULT_TXTBSY = -25,//!< RESULT_TXTBSY
        RESULT_FBIG   = -26,//!< RESULT_FBIG
        RESULT_NOSPC  = -27,//!< RESULT_NOSPC
        RESULT_SPIPE  = -28,//!< RESULT_SPIPE
        RESULT_ROFS   = -29,//!< RESULT_ROFS
        RESULT_MLINK  = -30,//!< RESULT_MLINK
        RESULT_PIPE   = -31,//!< RESULT_PIPE

        RESULT_UNKNOWN = -1000,//!< RESULT_UNKNOWN
    };

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
        /// System name, i.e. OS name, e.g. "Darwin", "iPhone OS", "Linux", etc
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
        /// Advertising identifier if available, e.g. "advertisingIdentifier" on iOS
        char m_AdIdentifier[64];
        /// True if advertising is enabled, e.g. "advertisingTrackingEnabled" on iOS
        bool m_AdTrackingEnabled;
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

        /// Engine version, e.g. 1.2.1
        char m_Version[32];
        /// Engine version hash string
        char m_VersionSHA1[256];
        /// Engine debug/release
        bool m_IsDebug;
    };

    /**
     * SetEngineInfo function parameters
     */
    struct EngineInfoParam
    {
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
     * Remove file
     * @param path path to file to remove
     * @return RESULT_OK on success
     */
    Result Unlink(const char* path);

    /**
     * Move a file
     * @param dst_filename the destination filename. The file which contents is to be overwritten.
     * @param src_filename the source filename. The contents will be written to the destination filename and the file unlinked if successful.
     * @return RESULT_OK on success
    */
    Result MoveFile(const char* dst_filename, const char* src_filename);

    /**
     * Get and create platform specific application support directory.
     * Example paths:
     * OSX: ~/Library/Application Support/APPLICATION_NAME
     * Windows: C:\Documents and Settings\USERNAME\Application Data\APPLICATION_NAME
     * Linux: ~/.APPLICATION_NAME
     * @param application_name application name to get path for. This is the name of "your" application.
     * @param path path buffer
     * @param path_len path buffer length
     * @return RESULT_OK success. RESULT_INVAL if the supplied path is too short. General IO-errors could result in other
     * codes, e.g. RESULT_ACCES if permission is denied.
     */
    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len);

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
     * @return RESULT_OK on success
     */
    Result OpenURL(const char* url);


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
}

#endif
