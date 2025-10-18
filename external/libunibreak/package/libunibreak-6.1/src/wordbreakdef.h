/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Word breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2013-2019 Tom Hacohen <tom at stosb dot com>
 * Copyright (C) 2018 Wu Yongwei <wuyongwei at gmail dot com>
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
 * When this library was designed, this annex was at Revision 17, for
 * Unicode 6.0.0:
 *      <URL:http://www.unicode.org/reports/tr29/tr29-17.html>
 *
 * This library has been updated according to Revision 37, for
 * Unicode 13.0.0:
 *      <URL:http://www.unicode.org/reports/tr29/tr29-37.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    wordbreakdef.h
 *
 * Definitions of internal data structures, declarations of global
 * variables, and function prototypes for the word breaking algorithm.
 *
 * @author  Tom Hacohen
 */

#ifndef WORDBREAKDEF_H
#define WORDBREAKDEF_H

#include "unibreakdef.h"

/**
 * Word break classes.  This is a direct mapping of Table 3 of Unicode
 * Standard Annex 29, Revision 35.
 */
enum WordBreakClass
{
    WBP_Undefined,
    WBP_CR,
    WBP_LF,
    WBP_Newline,
    WBP_Extend,
    WBP_ZWJ,
    WBP_Regional_Indicator,
    WBP_Format,
    WBP_Katakana,
    WBP_Hebrew_Letter,
    WBP_ALetter,
    WBP_Single_Quote,
    WBP_Double_Quote,
    WBP_MidNumLet,
    WBP_MidLetter,
    WBP_MidNum,
    WBP_Numeric,
    WBP_ExtendNumLet,
    WBP_WSegSpace,
    WBP_Any
};

/**
 * Struct for entries of word break properties.  The array of the
 * entries \e must be sorted.
 */
struct WordBreakProperties
{
    utf32_t start;              /**< Start codepoint */
    utf32_t end;                /**< End codepoint, inclusive */
    enum WordBreakClass prop;   /**< The word breaking property */
};

#endif /* WORDBREAKDEF_H */
