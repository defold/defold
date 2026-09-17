/*
 * Grapheme breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2016-2019 Andreas Röver <roever at users dot sf dot net>
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
 *
 * The main reference is Unicode Standard Annex 29 (UAX #29):
 *      <URL:http://unicode.org/reports/tr29>
 *
 * When this library was designed, this annex was at Revision 29, for
 * Unicode 9.0.0:
 *      <URL:http://www.unicode.org/reports/tr29/tr29-29.html>
 *
 * This library has been updated according to Revision 37, for
 * Unicode 13.0.0:
 *      <URL:http://www.unicode.org/reports/tr29/tr29-37.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    graphemebreakdef.h
 *
 * Definitions of internal data structures, declarations of global
 * variables, and function prototypes for the grapheme breaking algorithm.
 *
 * @author  Andreas Röver
 */

#ifndef GRAPHEMEBREAKDEF_H
#define GRAPHEMEBREAKDEF_H

#include "unibreakdef.h"

/**
 * Word break classes.  This is a direct mapping of Table 2 of Unicode
 * Standard Annex 29.
 */
enum GraphemeBreakClass
{
    GBP_CR,
    GBP_LF,
    GBP_Control,
    GBP_Virama,
    GBP_LinkingConsonant,
    GBP_Extend,
    GBP_ZWJ,
    GBP_Regional_Indicator,
    GBP_Prepend,
    GBP_SpacingMark,
    GBP_L,
    GBP_V,
    GBP_T,
    GBP_LV,
    GBP_LVT,
    GBP_Other,
    GBP_Undefined
};

/**
 * Struct for entries of grapheme break properties.  The array of the
 * entries \e must be sorted.
 */
struct GraphemeBreakProperties
{
    utf32_t start;                /**< Start codepoint */
    utf32_t end;                  /**< End codepoint, inclusive */
    enum GraphemeBreakClass prop; /**< The grapheme breaking property */
};

#endif /* GRAPHEMEBREAKDEF_H */
