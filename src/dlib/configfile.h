#include <stdint.h>

namespace dmConfigFile
{
    /**
     * Config file handle
     */
    typedef struct Config* HConfig;

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
    };

    /**
     * Load config file
     * @param file_name File name
     * @param config Config file handle (out)
     * @return RESULT_OK on success
     */
    Result Load(const char* file_name, HConfig* config);

    /**
     * Delete config file
     * @param config Config file handle
     */
    void Delete(HConfig config);

    /**
     * Get config value as string
     * @param config Config file handle
     * @param key Key in format section.key (.key for no section)
     * @param default_value Default value to return if key isn't found
     * @return Found value or default value
     */
    const char* GetString(HConfig config, const char* key, const char* default_value);

    /**
     * Get config value as int. NOTE: default_value is returned for invalid integer values
     * @param config Config file handle
     * @param key Key (see GetString())
     * @param default_value Default value to return if key isn't found
     * @return Found value or default value
     */
    int32_t GetInt(HConfig config, const char* key, int32_t default_value);
}
