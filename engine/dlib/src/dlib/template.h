#ifndef DM_TEMPLATE
#define DM_TEMPLATE

#include <stdint.h>

/**
 * Simple template engine.
 * Variables are specified in the following form
 * ${KEY}
 */
namespace dmTemplate
{
    /**
     * Result codes
     */
    enum Result
    {
        RESULT_OK                   =  0,//!< RESULT_OK
        RESULT_SYNTAX_ERROR         = -1,//!< RESULT_SYNTAX_ERROR
        RESULT_MISSING_REPLACEMENT  = -2,//!< RESULT_MISSING_REPLACEMENT
        RESULT_BUFFER_TOO_SMALL     = -3,//!< RESULT_BUFFER_TOO_SMALL
    };

    /**
     * String replacement callback
     * @param user_data user data
     * @param key key
     * @return replacement string. null of no exists
     */
    typedef const char* (*ReplaceCallback)(void* user_data, const char* key);

    /**
     * Format string
     * @param user_data user data
     * @param buf buffer to format to
     * @param buf_size buffer size
     * @param fmt format string for variables in form ${KEY}
     * @param call_back call back
     * @return RESULT_OK on success
     */
    Result Format(void* user_data, char* buf, uint32_t buf_size, const char* fmt, ReplaceCallback call_back);
}


#endif // DM_TEMPLATE
