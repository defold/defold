#ifndef DM_UTF
#define DM_UTF8

#include <stdint.h>
#include <stdlib.h>

namespace dmUtf8
{
    /**
     * Get number of unicode characters in utf-8 string
     * @param str utf8 string
     * @return number of characters
     */
    uint32_t StrLen(const char* str);

    /**
     * Get next unicode character in utf-8 string
     * @param str pointer to string. the pointer value is updated
     * @return decoded unicode character
     */
    uint32_t NextChar(const char** str);
}

#endif // DM_UTF8
