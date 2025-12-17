/*
 * Grapheme breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2016-2019 Andreas Röver <roever at users dot sf dot net>
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
 * @file    graphemebreak.c
 *
 * Implementation of the grapheme breaking algorithm as described in Unicode
 * Standard Annex 29.
 *
 * @author  Andreas Röver
 */

#include <string.h>
#include "graphemebreak.h"
#include "graphemebreakdata.c"
#include "indicconjunctbreakdata.c"
#include "indicconjunctbreakdef.h"
#include "emojidef.h"
#include "unibreakdef.h"

enum Rule9cStage
{
    R9C_INACTIVE,
    R9C_STARTED,
    R9C_LINKER,
    R9C_END
};

/**
 * Initializes the wordbreak internals.  It currently does nothing, but
 * it may in the future.
 */
void init_graphemebreak(void)
{
}

/**
 * Gets the grapheme breaking class of a character.
 *
 * @param[in] ch  character to check
 * @return        the grapheme breaking class if found; \c GBP_Other otherwise
 */
static enum GraphemeBreakClass get_char_gb_class(utf32_t ch)
{
    const struct GraphemeBreakProperties *result_ptr =
        ub_bsearch(ch, gb_prop_default, ARRAY_LEN(gb_prop_default) - 1,
                   sizeof(struct GraphemeBreakProperties));
    if (result_ptr)
    {
        return result_ptr->prop;
    }

    return GBP_Other;
}

/**
 * Gets the InCB class of a character.
 *
 * @param[in] ch  character to check
 * @return        the InCB class if found; \c InCB_None otherwise
 */
static enum IndicConjunctBreakClass get_char_incb_class(utf32_t ch)
{
    const struct IndicConjunctBreakProperties *result_ptr =
        ub_bsearch(ch, incb_prop, ARRAY_LEN(incb_prop),
                   sizeof(struct IndicConjunctBreakProperties));
    if (result_ptr)
    {
        return result_ptr->prop;
    }
    return InCB_None;
}

static enum Rule9cStage
update_rule9c_stage(enum Rule9cStage stage,
                    enum IndicConjunctBreakClass incb)
{
    static enum Rule9cStage states[R9C_END][InCB_None] = {
        /* R9C_INACTIVE */
        { R9C_INACTIVE, R9C_STARTED, R9C_INACTIVE },
        /* R9C_STARTED */
        { R9C_LINKER, R9C_STARTED, R9C_STARTED },
        /* R9C_LINKER */
        { R9C_LINKER, R9C_STARTED, R9C_LINKER }
    };
    if (incb == InCB_None) {
        return R9C_INACTIVE;
    }
    return states[stage][incb];
}

/**
 * Sets the grapheme breaking information for a generic input string.
 * It uses the extended grapheme cluster ruleset.
 *
 * @param[in]  s             input string
 * @param[in]  len           length of the input
 * @param[out] brks          pointer to the output breaking data, containing
 *                           #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK
 * @param[in] get_next_char  function to get the next UTF-32 character
 */
static void set_graphemebreaks(const void *s, size_t len, char *brks,
                               get_next_char_t get_next_char)
{
    size_t posNext = 0;
    enum Rule9cStage rule9cStage = R9C_INACTIVE;
    int rule11Detector = 0;
    bool evenRegionalIndicators = true;  // is the number of preceeding
                                         // GBP_RegionalIndicator characters
                                         // even

    utf32_t ch = get_next_char(s, len, &posNext);
    enum GraphemeBreakClass current_class = get_char_gb_class(ch);
    enum IndicConjunctBreakClass current_incb = get_char_incb_class(ch);

    // initialize whole output to inside char
    memset(brks, GRAPHEMEBREAK_INSIDEACHAR, len);

    while (true)
    {

        // this state-machine recognizes the following pattern:
        // extended_pictograph Extended* ZWJ
        // when that pattern has been detected rule11Detector will be
        // 3 and rule 11 can be applied below
        switch (current_class)
        {
        case GBP_ZWJ:
            if (rule11Detector == 1 || rule11Detector == 2)
            {
                rule11Detector = 3;
            }
            else
            {
                rule11Detector = 0;
            }
            break;

        case GBP_Extend:
            if (rule11Detector == 1 || rule11Detector == 2)
            {
                rule11Detector = 2;
            }
            else
            {
                rule11Detector = 0;
            }
            break;

        default:
            if (ub_is_extended_pictographic(ch))
            {
                rule11Detector = 1;
            }
            else
            {
                rule11Detector = 0;
            }
            break;
        }

        rule9cStage = update_rule9c_stage(rule9cStage, current_incb);

        enum GraphemeBreakClass prev_class = current_class;

        // safe position if current character so that we can store the
        // result there later on
        size_t brksPos = posNext - 1;

        // get nect character
        ch = get_next_char(s, len, &posNext);

        if (ch == EOS)
        {
            // done, place one final break after the last character as per
            // algorithm rule GB1
            brks[brksPos] = GRAPHEMEBREAK_BREAK;
            break;
        }

        // get class of current character
        current_class = get_char_gb_class(ch);
        current_incb = get_char_incb_class(ch);

        if (prev_class == GBP_Regional_Indicator)
        {
            evenRegionalIndicators = !evenRegionalIndicators;
        }
        else
        {
            evenRegionalIndicators = true;
        }

        // check all rules
        if (prev_class == GBP_CR && current_class == GBP_LF)
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB3
        }
        else if ((prev_class == GBP_CR) || (prev_class == GBP_LF) ||
                 (prev_class == GBP_Control) || (current_class == GBP_CR) ||
                 (current_class == GBP_LF) ||
                 (current_class == GBP_Control))
        {
            brks[brksPos] = GRAPHEMEBREAK_BREAK;  // Rule: GB4 + GB5
        }
        else if ((prev_class == GBP_L) &&
                 ((current_class == GBP_L) || (current_class == GBP_V) ||
                  (current_class == GBP_LV) || (current_class == GBP_LVT)))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB6
        }
        else if (((prev_class == GBP_LV) || (prev_class == GBP_V)) &&
                 ((current_class == GBP_V) || (current_class == GBP_T)))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB7
        }
        else if (((prev_class == GBP_LVT) || (prev_class == GBP_T)) &&
                 (current_class == GBP_T))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB8
        }
        else if ((current_class == GBP_Extend) ||
                 (current_class == GBP_ZWJ) ||
                 (current_class == GBP_Virama))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB9
        }
        else if (current_class == GBP_SpacingMark)
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB9a
        }
        else if (prev_class == GBP_Prepend)
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB9b
        }
        else if ((rule9cStage == R9C_LINKER) &&
                 (current_incb == InCB_Consonant))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB9c
        }
        else if ((rule11Detector == 3) && ub_is_extended_pictographic(ch))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB11
        }
        else if (!evenRegionalIndicators &&
                 (current_class == GBP_Regional_Indicator))
        {
            brks[brksPos] = GRAPHEMEBREAK_NOBREAK;  // Rule: GB12 + GB13
        }
        else
        {
            brks[brksPos] = GRAPHEMEBREAK_BREAK;  // Rule: GB999
        }
    }
}

/**
 * Sets the grapheme breaking information for a UTF-8 input string.
 *
 * @param[in]  s     input UTF-8 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input (reserved for future use)
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK.
 *                   First element in output array is for the break behind
 *                   the first character the pointer must point to an
 *                   array with at least as many elements as there
 *                   are characters in the string
 */
void set_graphemebreaks_utf8(const utf8_t *s, size_t len, const char *lang,
                             char *brks)
{
    (void)lang;
    set_graphemebreaks(s, len, brks,
                       (get_next_char_t)ub_get_next_char_utf8);
}

/**
 * Sets the grapheme breaking information for a UTF-16 input string.
 *
 * @param[in]  s     input UTF-16 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input (reserved for future use)
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK.
 *                   First element in output array is for the break behind
 *                   the first character the pointer must point to an
 *                   array with at least as many elements as there
 *                   are characters in the string
 */
void set_graphemebreaks_utf16(const utf16_t *s, size_t len,
                              const char *lang, char *brks)
{
    (void)lang;
    set_graphemebreaks(s, len, brks,
                       (get_next_char_t)ub_get_next_char_utf16);
}

/**
 * Sets the grapheme breaking information for a UTF-32 input string.
 *
 * @param[in]  s     input UTF-32 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input (reserved for future use)
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #GRAPHEMEBREAK_BREAK or #GRAPHEMEBREAK_NOBREAK.
 *                   First element in output array is for the break behind
 *                   the first character the pointer must point to an
 *                   array with at least as many elements as there
 *                   are characters in the string
 */
void set_graphemebreaks_utf32(const utf32_t *s, size_t len,
                              const char *lang, char *brks)
{
    (void)lang;
    set_graphemebreaks(s, len, brks,
                       (get_next_char_t)ub_get_next_char_utf32);
}
