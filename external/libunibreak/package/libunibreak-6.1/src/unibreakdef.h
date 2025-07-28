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
 * @file    unibreakdef.h
 *
 * Header file for private definitions in the libunibreak library.
 *
 * @author  Wu Yongwei
 */

#ifndef UNIBREAKDEF_H
#define UNIBREAKDEF_H

#if defined(_MSC_VER) && _MSC_VER < 1800
typedef int bool;
#define false 0
#define true 1
#else
#include <stdbool.h>
#endif

#include <stddef.h>
#include "unibreakbase.h"

#define ARRAY_LEN(x) (sizeof(x) / sizeof(x[0]))

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Constant value to mark the end of string.  It is not a valid Unicode
 * character.
 */
#define EOS 0xFFFFFFFF

/**
 * Abstract function interface for #ub_get_next_char_utf8,
 * #ub_get_next_char_utf16, and #ub_get_next_char_utf32.
 */
typedef utf32_t (*get_next_char_t)(const void *, size_t, size_t *);

/* Function Prototype */
utf32_t ub_get_next_char_utf8(const utf8_t *s, size_t len, size_t *ip);
utf32_t ub_get_next_char_utf16(const utf16_t *s, size_t len, size_t *ip);
utf32_t ub_get_next_char_utf32(const utf32_t *s, size_t len, size_t *ip);

__inline const void *ub_bsearch(utf32_t ch, const void *ptr, size_t count,
                                size_t size)
{
    int min = 0;
    int max = count - 1;
    int mid;

    do
    {
        mid = (min + max) / 2;
        const unsigned char *mid_ptr =
            (const unsigned char *)ptr + mid * size;
        utf32_t mid_start = *(const utf32_t *)mid_ptr;
        utf32_t mid_end = *((const utf32_t *)mid_ptr + 1);

        if (ch < mid_start)
        {
            max = mid - 1;
        }
        else if (ch > mid_end)
        {
            min = mid + 1;
        }
        else
        {
            return mid_ptr;
        }
    } while (min <= max);

    return NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* UNIBREAKDEF_H */
