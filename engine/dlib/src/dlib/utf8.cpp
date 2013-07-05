#include "utf8.h"

/**
 * utf-8 support
 *
 * It's assumed that the supplied utf-8 sequences are well-formed and
 * invalid sequences are not detected. It's however guaranteed that
 * the library function will not read outside the buffer and the terminating
 * '\0' character is always respected.
 */
namespace dmUtf8
{
    static const uint32_t offsets[6] = {
        0x00000000UL,
        0x00003080UL,
        0x000e2080UL,
        0x03c82080UL,
        0xfa082080UL,
        0x82082080UL
    };

    uint32_t NextChar(const char** str) {
        uint32_t c = 0;
        int i = 0;
        const char* s = *str;

        do {
            c <<= 6;
            c += (uint8_t)(*s);
            ++s;
            ++i;
        } while (*s && (*s & 0xc0) == 0x80);

        c -= offsets[i-1];
        *str = s;
        return c;
    }

    uint32_t StrLen(const char* str) {
        int i = 0, j = 0;
        while (str[i]) {
            if ((str[i] & 0xc0) != 0x80) j++;
            i++;
        }
        return j;
    }

}
