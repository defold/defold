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

#ifndef EASTASIANWIDTHDEF_H
#define EASTASIANWIDTHDEF_H

#include "unibreakdef.h"

/**
 * East Asian Width (ea) class.  This is defined in Unicode Standard
 * Annex 11.
 */
enum EastAsianWidthClass
{
    EAW_A,  /**< Ambiguous */
    EAW_F,  /**< Fullwidth */
    EAW_H,  /**< Halfwidth */
    EAW_Na, /**< Narrow */
    EAW_W,  /**< Wide */
    EAW_N,  /**< Neutral */
};

/**
 * Struct for entries of East Asian Width properties.  The array of the
 * entries \e must be sorted.
 */
struct EastAsianWidthProperties
{
    utf32_t start;                  /**< Start codepoint */
    utf32_t end;                    /**< End codepoint, inclusive */
    enum EastAsianWidthClass prop;  /**< The East Asian Width property */
};

enum EastAsianWidthClass ub_get_char_eaw_class(utf32_t ch);

#endif /* EASTASIANWIDTHDEF_H */
