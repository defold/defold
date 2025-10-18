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
 * @file    emojidef.h
 *
 * Definitions of internal data structure and function for extended
 * pictographs.
 *
 * @author  Andreas Röver
 */

#ifndef EMOJIDEF_H
#define EMOJIDEF_H

#include "unibreakdef.h"

/**
 * Struct for entries of extended pictographic properties.  The array of
 * the entries \e must be sorted.  All codepoints within this list have
 * the property of being extended pictographic.
 */
struct ExtendedPictograpic
{
    utf32_t start;                /**< Start codepoint */
    utf32_t end;                  /**< End codepoint, inclusive */
};

bool ub_is_extended_pictographic(utf32_t ch);

#endif /* EMOJIDEF_H */
