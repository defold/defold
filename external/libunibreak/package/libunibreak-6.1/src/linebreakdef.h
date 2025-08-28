/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Line breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2008-2020 Wu Yongwei <wuyongwei at gmail dot com>
 * Copyright (C) 2013 Petr Filipsky <philodej at gmail dot com>
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
 * @file    linebreakdef.h
 *
 * Definitions of internal data structures, declarations of global
 * variables, and function prototypes for the line breaking algorithm.
 *
 * @author  Wu Yongwei
 * @author  Petr Filipsky
 */

#ifndef LINEBREAKDEF_H
#define LINEBREAKDEF_H

#include "unibreakdef.h"

/**
 * Line break classes.  This is a mapping of Table 1 of Unicode
 * Standard Annex 14.
 */
enum LineBreakClass
{
    /* This is used to signal an error condition. */
    LBP_Undefined,  /**< Undefined */

    /* The following break classes are treated in the pair table. */
    LBP_OP,         /**< Opening punctuation */
    LBP_CL,         /**< Closing punctuation */
    LBP_CP,         /**< Closing parenthesis */
    LBP_QU,         /**< Ambiguous quotation */
    LBP_GL,         /**< Glue */
    LBP_NS,         /**< Non-starters */
    LBP_EX,         /**< Exclamation/Interrogation */
    LBP_SY,         /**< Symbols allowing break after */
    LBP_IS,         /**< Infix separator */
    LBP_PR,         /**< Prefix */
    LBP_PO,         /**< Postfix */
    LBP_NU,         /**< Numeric */
    LBP_AL,         /**< Alphabetic */
    LBP_HL,         /**< Hebrew letter */
    LBP_ID,         /**< Ideographic */
    LBP_IN,         /**< Inseparable characters */
    LBP_HY,         /**< Hyphen */
    LBP_BA,         /**< Break after */
    LBP_BB,         /**< Break before */
    LBP_B2,         /**< Break on either side (but not pair) */
    LBP_ZW,         /**< Zero-width space */
    LBP_CM,         /**< Combining marks */
    LBP_WJ,         /**< Word joiner */
    LBP_H2,         /**< Hangul LV */
    LBP_H3,         /**< Hangul LVT */
    LBP_JL,         /**< Hangul L Jamo */
    LBP_JV,         /**< Hangul V Jamo */
    LBP_JT,         /**< Hangul T Jamo */
    LBP_RI,         /**< Regional indicator */
    LBP_EB,         /**< Emoji base */
    LBP_EM,         /**< Emoji modifier */
    LBP_ZWJ,        /**< Zero width joiner */

    /* The following break class is treated in the pair table, but it is
     * not part of Table 2 of UAX #14-37. */
    LBP_CB,         /**< Contingent break */

    /* The following break classes are not treated in the pair table */
    LBP_AI,         /**< Ambiguous (alphabetic or ideograph) */
    LBP_BK,         /**< Break (mandatory) */
    LBP_CJ,         /**< Conditional Japanese starter */
    LBP_CR,         /**< Carriage return */
    LBP_LF,         /**< Line feed */
    LBP_NL,         /**< Next line */
    LBP_SA,         /**< South-East Asian */
    LBP_SG,         /**< Surrogates */
    LBP_SP,         /**< Space */
    LBP_XX          /**< Unknown */
};

enum BreakOutputType
{
    LBOT_PER_CODE_UNIT,
    LBOT_PER_CODE_POINT
};

/**
 * Struct for entries of line break properties.  The array of the
 * entries \e must be sorted.
 */
struct LineBreakProperties
{
    utf32_t start;              /**< Start codepoint */
    utf32_t end;                /**< End codepoint, inclusive */
    enum LineBreakClass prop;   /**< The line breaking property */
};

/**
 * Struct for association of language-specific line breaking properties
 * with language names.
 */
struct LineBreakPropertiesLang
{
    const char *lang;                      /**< Language name */
    size_t namelen;                        /**< Length of name to match */
    const struct LineBreakProperties *lbp; /**< Pointer to associated data */
};

/**
 * Context representing internal state of the line breaking algorithm.
 * This is useful to callers if incremental analysis is wanted.
 */
struct LineBreakContext
{
    const char *lang;               /**< Language name */
    const struct LineBreakProperties *lbpLang; /**< Pointer to
                                                    LineBreakProperties */
    enum LineBreakClass lbcCur;     /**< Breaking class of current codepoint */
    enum LineBreakClass lbcNew;     /**< Breaking class of next codepoint */
    enum LineBreakClass lbcLast;    /**< Breaking class of last codepoint */
    unsigned char eaNew;            /**< East Asian Width of next codepoint */
    unsigned char eaLast;           /**< East Asian Width of last codepoint */
    bool fLb8aZwj;                  /**< Flag for ZWJ (LB8a) */
    bool fLb21aHebrew;              /**< Flag for Hebrew letters (LB21a) */
    int cLb30aRI;                   /**< Count of RI characters (LB30a) */
};

/* Declarations */
extern const struct LineBreakProperties lb_prop_supplementary[];
extern const unsigned int lb_prop_supplementary_len;
extern const char lb_prop_bmp[];
extern const struct LineBreakPropertiesLang lb_prop_lang_map[];

/* Function Prototype */
void lb_init_break_context(
        struct LineBreakContext *lbpCtx,
        utf32_t ch,
        const char *lang);
int lb_process_next_char(
        struct LineBreakContext *lbpCtx,
        utf32_t ch);
enum LineBreakClass lb_get_char_class(
        const struct LineBreakContext *lbpCtx,
        utf32_t ch);
size_t set_linebreaks(
        const void *s,
        size_t len,
        const char *lang,
        enum BreakOutputType outputType,
        char *brks,
        get_next_char_t get_next_char);

#endif /* LINEBREAKDEF_H */
