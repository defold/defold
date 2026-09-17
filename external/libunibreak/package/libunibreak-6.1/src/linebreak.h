/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Line breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2008-2024 Wu Yongwei <wuyongwei at gmail dot com>
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
 * The main reference is Unicode Standard Annex 14 (UAX #14):
 *      <URL:http://www.unicode.org/reports/tr14/>
 *
 * When this library was designed, this annex was at Revision 19, for
 * Unicode 5.0.0:
 *      <URL:http://www.unicode.org/reports/tr14/tr14-19.html>
 *
 * This library has been updated according to Revision 49, for
 * Unicode 15.0.0:
 *      <URL:http://www.unicode.org/reports/tr14/tr14-49.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    linebreak.h
 *
 * Header file for the line breaking algorithm.
 *
 * @author  Wu Yongwei
 */

#ifndef LINEBREAK_H
#define LINEBREAK_H

#include <stddef.h>
#include "unibreakbase.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LINEBREAK_MUSTBREAK      0  /**< Break is mandatory */
#define LINEBREAK_ALLOWBREAK     1  /**< Break is allowed */
#define LINEBREAK_NOBREAK        2  /**< No break is possible */
#define LINEBREAK_INSIDEACHAR    3  /**< A UTF-8/16 sequence is unfinished */
#define LINEBREAK_INDETERMINATE  4  /**< End of input on a non-EOL char */

void init_linebreak(void);
void set_linebreaks_utf8(
        const utf8_t *s, size_t len, const char *lang, char *brks);
void set_linebreaks_utf16(
        const utf16_t *s, size_t len, const char *lang, char *brks);
void set_linebreaks_utf32(
        const utf32_t *s, size_t len, const char *lang, char *brks);
size_t set_linebreaks_utf8_per_code_point(
        const utf8_t *s, size_t len, const char *lang, char *brks);
size_t set_linebreaks_utf16_per_code_point(
        const utf16_t *s, size_t len, const char *lang, char *brks);
int is_line_breakable(utf32_t char1, utf32_t char2, const char *lang);

#ifdef __cplusplus
}
#endif

#endif /* LINEBREAK_H */
