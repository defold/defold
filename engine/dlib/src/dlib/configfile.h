#ifndef DM_CONFIGFILE_H
#define DM_CONFIGFILE_H

#include <dmsdk/dlib/configfile.h>

namespace dmConfigFile
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK               = 0, //!< RESULT_OK
        RESULT_FILE_NOT_FOUND   = -1,//!< RESULT_FILE_NOT_FOUND
        RESULT_LITERAL_TOO_LONG = -2,//!< RESULT_LITERAL_TOO_LONG
        RESULT_SYNTAX_ERROR     = -3,//!< RESULT_SYNTAX_ERROR
        RESULT_UNEXPECTED_EOF   = -4,//!< RESULT_UNEXPECTED_EOF
        RESULT_INVALID_URI      = -5,//!< RESULT_INVALID_URI
    };

    /**
     * Load config file
     * @note Config values can be specified or overridden by command line arguments with the following syntax:
     *       --config=section.key=value
     * @param url URL to load the config-file from. Local path names are also supported.
     * @param argc Command line argument count (typically from main(...))
     * @param argv Command line arguments (typically from main(...))
     * @param config Config file handle (out)
     * @return RESULT_OK on success
     */
    Result Load(const char* url, int argc, const char** argv, HConfig* config);

    /**
     * Load config from buffer
     * @note Config values can be specified or overridden by command line arguments with the following syntax:
     *       --config=section.key=value
     * @param buffer Buffer to load from
     * @param buffer_size Buffer size
     * @param argc Command line argument count (typically from main(...))
     * @param argv Command line arguments (typically from main(...))
     * @param config Config file handle (out)
     * @return RESULT_OK on success
     */
    Result LoadFromBuffer(const char* buffer, uint32_t buffer_size, int argc, const char** argv, HConfig* config);

    /**
     * Delete config file
     * @param config Config file handle
     */
    void Delete(HConfig config);

}

#endif // DM_CONFIGFILE_H

