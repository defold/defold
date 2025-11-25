/*
 * Emoji-related routine and data.
 *
 * Copyright (C) 2018 Andreas Röver <roever at users dot sf dot net>
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
 * @file    emojidef.c
 *
 * Emoji-related routine and data that are used internally.
 *
 * @author  Andreas Röver
 */

#include "emojidef.h"
#include "emojidata.c"
#include "unibreakdef.h"

/**
 * Finds out if a codepoint is extended pictographic.
 *
 * @param[in] ch  character to check
 * @return        \c true if the codepoint is extended pictographic;
 *                \c false otherwise
 */
bool ub_is_extended_pictographic(utf32_t ch)
{
    return ub_bsearch(ch, ep_prop, ARRAY_LEN(ep_prop),
                      sizeof(struct ExtendedPictograpic)) != NULL;
}
