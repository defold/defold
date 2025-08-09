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
 * This library has been updated according to Revision 43, for
 * Unicode 15.1.0:
 *      <URL:https://www.unicode.org/reports/tr29/tr29-43.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    graphemebreak.h
 *
 * Header file for the grapheme breaking algorithm.
 *
 * @author  Andreas Röver
 */

#ifndef GRAPHEMEBREAK_H
#define GRAPHEMEBREAK_H

#include <stddef.h>
#include "unibreakbase.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GRAPHEMEBREAK_BREAK        0  /**< Between two graphemes */
#define GRAPHEMEBREAK_NOBREAK      1  /**< Inside a grapheme */
#define GRAPHEMEBREAK_INSIDEACHAR  2  /**< Inside a Unicode character */

void init_graphemebreak(void);
void set_graphemebreaks_utf8(const utf8_t *s, size_t len, const char *lang,
                             char *brks);
void set_graphemebreaks_utf16(const utf16_t *s, size_t len,
                              const char *lang, char *brks);
void set_graphemebreaks_utf32(const utf32_t *s, size_t len,
                              const char *lang, char *brks);

#ifdef __cplusplus
}
#endif

#endif /* GRAPHEMEBREAK_H */
