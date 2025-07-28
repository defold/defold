/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Break processing in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2015-2024 Wu Yongwei <wuyongwei at gmail dot com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must
 *    not claim that you wrote the original software.  If you use this
 *    software in a product, an acknowledgement in the product
 *    documentation would be appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must
 *    not be misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 */

/**
 * @file    unibreakdef.c
 *
 * Definition of utility functions used by the libunibreak library.
 *
 * @author  Wu Yongwei
 */

#include <assert.h>
#include <stddef.h>
#include "unibreakdef.h"

/**
 * Gets the next Unicode character in a UTF-8 sequence.  The index will
 * be advanced to the next complete character, unless the end of string
 * is reached in the middle of a UTF-8 sequence.
 *
 * @param[in]     s    input UTF-8 string
 * @param[in]     len  length of the string in bytes
 * @param[in,out] ip   pointer to the index
 * @return             the Unicode character beginning at the index; or
 *                     #EOS if end of input is encountered
 */
utf32_t ub_get_next_char_utf8(
        const utf8_t *s,
        size_t len,
        size_t *ip)
{
    utf8_t ch;
    utf32_t res;

    assert(*ip <= len);
    if (*ip == len)
    {
        return EOS;
    }
    ch = s[*ip];

    if (ch < 0xC2 || ch > 0xF4)
    {   /* One-byte sequence, tail (should not occur), or invalid */
        *ip += 1;
        return ch;
    }
    else if (ch < 0xE0)
    {   /* Two-byte sequence */
        if (*ip + 2 > len)
        {
            return EOS;
        }
        res = ((ch & 0x1F) << 6) + (s[*ip + 1] & 0x3F);
        *ip += 2;
        return res;
    }
    else if (ch < 0xF0)
    {   /* Three-byte sequence */
        if (*ip + 3 > len)
        {
            return EOS;
        }
        res = ((ch & 0x0F) << 12) +
              ((s[*ip + 1] & 0x3F) << 6) +
              ((s[*ip + 2] & 0x3F));
        *ip += 3;
        return res;
    }
    else
    {   /* Four-byte sequence */
        if (*ip + 4 > len)
        {
            return EOS;
        }
        res = ((ch & 0x07) << 18) +
              ((s[*ip + 1] & 0x3F) << 12) +
              ((s[*ip + 2] & 0x3F) << 6) +
              ((s[*ip + 3] & 0x3F));
        *ip += 4;
        return res;
    }
}

/**
 * Gets the next Unicode character in a UTF-16 sequence.  The index will
 * be advanced to the next complete character, unless the end of string
 * is reached in the middle of a UTF-16 surrogate pair.
 *
 * @param[in]     s    input UTF-16 string
 * @param[in]     len  length of the string in words
 * @param[in,out] ip   pointer to the index
 * @return             the Unicode character beginning at the index; or
 *                     #EOS if end of input is encountered
 */
utf32_t ub_get_next_char_utf16(
        const utf16_t *s,
        size_t len,
        size_t *ip)
{
    utf16_t ch;

    assert(*ip <= len);
    if (*ip == len)
    {
        return EOS;
    }
    ch = s[(*ip)++];

    if (ch < 0xD800 || ch > 0xDBFF)
    {   /* If the character is not a high surrogate */
        return ch;
    }
    if (*ip == len)
    {   /* If the input ends here (an error) */
        --(*ip);
        return EOS;
    }
    if (s[*ip] < 0xDC00 || s[*ip] > 0xDFFF)
    {   /* If the next character is not the low surrogate (an error) */
        return ch;
    }
    /* Return the constructed character and advance the index again */
    return (((utf32_t)ch & 0x3FF) << 10) + (s[(*ip)++] & 0x3FF) + 0x10000;
}

/**
 * Gets the next Unicode character in a UTF-32 sequence.  The index will
 * be advanced to the next character.
 *
 * @param[in]     s    input UTF-32 string
 * @param[in]     len  length of the string in dwords
 * @param[in,out] ip   pointer to the index
 * @return             the Unicode character beginning at the index; or
 *                     #EOS if end of input is encountered
 */
utf32_t ub_get_next_char_utf32(
        const utf32_t *s,
        size_t len,
        size_t *ip)
{
    assert(*ip <= len);
    if (*ip == len)
    {
        return EOS;
    }
    return s[(*ip)++];
}

extern __inline const void *ub_bsearch(utf32_t ch, const void *ptr,
                                       size_t count, size_t size);
