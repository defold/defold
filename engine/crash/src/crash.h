#ifndef __CRASH_H__
#define __CRASH_H__

#include <stdint.h>

namespace dmCrash
{
     enum SysField
     {
           SYSFIELD_ENGINE_VERSION = 0,
           SYSFIELD_ENGINE_HASH,
           SYSFIELD_DEVICE_MODEL,
           SYSFIELD_MANUFACTURER,
           SYSFIELD_SYSTEM_NAME,
           SYSFIELD_SYSTEM_VERSION,
           SYSFIELD_LANGUAGE,
           SYSFIELD_DEVICE_LANGUAGE,
           SYSFIELD_TERRITORY,
           SYSFIELD_ANDROID_BUILD_FINGERPRINT,
           SYSFIELD_MAX
     };

     enum Result
     {
           RESULT_OK,
           RESULT_INVALID_PARAM
     };

    typedef int HDump;

    /**
     * Initializes the crash reporting library, installs crash signal handlers.
     * @param version string for SYSFIELD_ENGINE_VERSION field
     * @param hash string for SYSFIELD_ENGINE_HASH field
     * @return context
     */
    void Init(const char* version, const char *hash);

    /**
     * Help function to see if the library was ever initialized. The rest of the API
     * cannot be used unless it has been.
     * @return if Init has been called
     */
    bool IsInitialized();

    /**
     * Set the location where to the crash dump will be written. If a crash happens before
     * it has been set, it will use a default location. It is recommended to set this early.
     */
    void SetFilePath(const char *whereto);

    /**
     * Performs the same steps as if a crash had occured for real. This call is supplied
     * to aid with testing.
     */
    void WriteDump();

    /**
     * Record application defined data into the crash dump internal struct. These fields can
     * be read out once a crash dump has been recovered.
     * @param index Index of the user data slot to write into
     * @param value String value to write
     * @return RESULT_OK if successful
     */
    Result SetUserField(uint32_t index, const char* value);

    /**
     * Load a previously written crash dump. First, the default path is examined, and then
     * the path set by SetFilePath.
     * @return handle to loaded dump, 0 if no dump found
     */
    HDump LoadPrevious();


    /**
     * Load a previously written crash dump from a path.
     * @return handle to loaded dump, 0 if no dump found
     */
    HDump LoadPreviousPath(const char* path);

    /**
     * Unloads a previously loaded crash dump.
     * @param dump The crash dump to unload
     */
    void Release(HDump dump);

    /**
     * Checks if a handle is valid.
     * @param dump The hanlde to check
     * @return true if the handle is valid
     */
    bool IsValidHandle(HDump dump);

    /**
     * Deletes previously written crash dumps from disk. It is suggested that this is performed
     * immediately after loading.
     */
    void Purge();

    /**
     * Reads a system defined field from a loaded crash dump.
     * @param dump The dump to read from
     * @param field The field to read
     * @return The field value or null on error
     */
    const char* GetSysField(HDump dump, SysField field);

    /**
     * Reads a user data field from a loaded crash dump.
     * @param dump The dump to read from
     * @param index The field to read
     * @return The field value or null on error
     */
    const char* GetUserField(HDump dump, uint32_t index);

    /**
     * Get the number of recorded backtrace addresses stored in the
     * crash dump.
     * @param dump crash dump handle
     * @return Number of recorded backtrace addresses
     */
    uint32_t GetBacktraceAddrCount(HDump dump);

    /**
     * Get a backtrace address.
     * @param dump crash dump handle
     * @param index index of the address to get
     * @return callstack pointer
     */
    void* GetBacktraceAddr(HDump dump, uint32_t index);

    /**
     * Get the text format version of the dump. It is written by platform-specific
     * backtrace functions (backtrace_symbols_fd) and there are no guarantees on the format
     * @param dump crash dump handle
     * @return the extra data as a string
     */
    const char* GetExtraData(HDump dump);

    /**
     * Get the signal number from the dump.
     * @param dump crash dump handle
     * @return signum
     */
    int GetSignum(HDump dump);

    /**
     * Read out module name from the module/address table stored in a crash dump.
     * @param index index of the module to read
     * @return name of the module, null if there are no more
     */
    const char* GetModuleName(HDump dump, uint32_t index);

    /**
     * Read out module address from the module/address table stored in a crash dump.
     * @param index index of the module to read
     * @return address of the module, null if there are no more
     */
    void* GetModuleAddr(HDump dump, uint32_t index);
}

#endif
