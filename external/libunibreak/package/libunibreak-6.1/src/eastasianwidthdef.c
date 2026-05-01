/*
 * Definitions of internal data types for Indic Conjunct Break.
 *
 * Copyright (C) 2024 Wu Yongwei <wuyongwei at gmail dot com>
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

#include "eastasianwidthdef.h"
#include "eastasianwidthdata.c"
#include "unibreakdef.h"

/**
 * Gets the East Asian Width class of a character.
 *
 * @param ch  character to check
 * @return    the East Asian Width class if found; \c EAW_N otherwise
 */
enum EastAsianWidthClass ub_get_char_eaw_class(utf32_t ch)
{
    const struct EastAsianWidthProperties *result_ptr =
        ub_bsearch(ch, eaw_prop, ARRAY_LEN(eaw_prop),
                   sizeof(struct EastAsianWidthProperties));
    if (result_ptr)
    {
        return result_ptr->prop;
    }
    return EAW_N;
}
