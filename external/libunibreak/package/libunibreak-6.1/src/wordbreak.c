/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Word breaking in a Unicode sequence.  Designed to be used in a
 * generic text renderer.
 *
 * Copyright (C) 2013-2019 Tom Hacohen <tom at stosb dot com>
 * Copyright (C) 2018-2024 Wu Yongwei <wuyongwei at gmail dot com>
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
 * This library has been updated according to Revision 43, for
 * Unicode 15.1.0:
 *      <URL:http://www.unicode.org/reports/tr29/tr29-43.html>
 *
 * The Unicode Terms of Use are available at
 *      <URL:http://www.unicode.org/copyright.html>
 */

/**
 * @file    wordbreak.c
 *
 * Implementation of the word breaking algorithm as described in Unicode
 * Standard Annex 29.
 *
 * @author  Tom Hacohen
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include "unibreakdef.h"
#include "wordbreak.h"
#include "wordbreakdata.c"
#include "emojidef.h"

/**
 * Initializes the wordbreak internals.  It currently does nothing, but
 * it may in the future.
 */
void init_wordbreak(void)
{
}

/**
 * Gets the word breaking class of a character.
 *
 * @param ch   character to check
 * @param wbp  pointer to the wbp breaking properties array
 * @param len  size of the wbp array in number of items
 * @return     the word breaking class if found; \c WBP_Any otherwise
 */
static enum WordBreakClass get_char_wb_class(utf32_t ch)
{
    const struct WordBreakProperties *result_ptr =
        ub_bsearch(ch, wb_prop_default, ARRAY_LEN(wb_prop_default) - 1,
                   sizeof(struct WordBreakProperties));
    if (result_ptr)
    {
        return result_ptr->prop;
    }

    return WBP_Any;
}

/**
 * Sets the word break types to a specific value in a range.
 *
 * It sets the inside chars to #WORDBREAK_INSIDEACHAR and the rest to brkType.
 * Assumes \a brks is initialized - all the cells with #WORDBREAK_NOBREAK are
 * cells that we really don't want to break after.
 *
 * @param[in]  s             input string
 * @param[out] brks          breaks array to fill
 * @param[in]  posStart      start position
 * @param[in]  posEnd        end position (exclusive)
 * @param[in]  len           length of the string
 * @param[in]  brkType       breaks type to use
 * @param[in] get_next_char  function to get the next UTF-32 character
 */
static void set_brks_to(
        const void *s,
        char *brks,
        size_t posStart,
        size_t posEnd,
        size_t len,
        char brkType,
        get_next_char_t get_next_char)
{
    size_t posNext = posStart;
    while (posNext < posEnd)
    {
        utf32_t ch;
        ch = get_next_char(s, len, &posNext);
        (void)ch;
        assert(ch != EOS);
        for (; posStart < posNext - 1; ++posStart)
        {
            brks[posStart] = WORDBREAK_INSIDEACHAR;
        }
        assert(posStart == posNext - 1);

        /* Only set it if we haven't set it not to break before. */
        if (brks[posStart] != WORDBREAK_NOBREAK)
        {
            brks[posStart] = brkType;
        }
        posStart = posNext;
    }
}

/* Checks to see if the class is newline, CR, or LF (rules WB3a and b). */
#define IS_WB3ab(cls) ((cls == WBP_Newline) || (cls == WBP_CR) || \
                       (cls == WBP_LF))

/**
 * Sets the word breaking information for a generic input string.
 *
 * @param[in]  s             input string
 * @param[in]  len           length of the input
 * @param[in]  lang          language of the input (reserved for future use)
 * @param[out] brks          pointer to the output breaking data, containing
 *                           #WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *                           #WORDBREAK_INSIDEACHAR
 * @param[in] get_next_char  function to get the next UTF-32 character
 */
static void set_wordbreaks(
        const void *s,
        size_t len,
        const char *lang,
        char *brks,
        get_next_char_t get_next_char)
{
    /* Counter of how many time we cam across RI */
    int riCounter = 0;
    enum WordBreakClass wbcLast = WBP_Undefined;
    /* wbcSeqStart is the class that started the current sequence.
     * WBP_Undefined is a special case that means "sot".
     * This value is the class that is at the start of the current rule
     * matching sequence. For example, in case of Numeric+MidNum+Numeric
     * it'll be Numeric all the way.
     */
    enum WordBreakClass wbcSeqStart = WBP_Undefined;
    utf32_t ch;
    size_t posNext = 0;
    size_t posCur = 0;
    size_t posLast = 0;

    /* TODO: Language-specific specialization. */
    (void) lang;

    /* Init brks. */
    memset(brks, WORDBREAK_BREAK, len);

    ch = get_next_char(s, len, &posNext);

    while (ch != EOS)
    {
        enum WordBreakClass wbcCur;
        wbcCur = get_char_wb_class(ch);

        switch (wbcCur)
        {
        case WBP_CR:
            /* WB3b */
            set_brks_to(s, brks, posLast, posCur, len,
                        WORDBREAK_BREAK, get_next_char);
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_LF:
            if (wbcSeqStart == WBP_CR) /* WB3 */
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
                break;
            }
            /* Fall through */

        case WBP_Newline:
            /* WB3a,3b */
            set_brks_to(s, brks, posLast, posCur, len,
                        WORDBREAK_BREAK, get_next_char);
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_ZWJ:
        case WBP_Extend:
        case WBP_Format:
            /* WB4 - If not the first char/after a newline (WB3a,3b), skip
             * this class, set it to be the same as the prev, and mark
             * brks not to break before them. */
            if ((wbcSeqStart == WBP_Undefined) || IS_WB3ab(wbcSeqStart))
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
            }
            else
            {
                /* It's surely not the first */
                brks[posCur - 1] = WORDBREAK_NOBREAK;
                /* WB3c and WB3d precede 4, so no intervening Extend
                 * chars allowed. */
                if (wbcCur != WBP_ZWJ && wbcSeqStart != WBP_ZWJ &&
                    wbcSeqStart != WBP_WSegSpace)
                {
                    /* "inherit" the previous class. */
                    wbcCur = wbcLast;
                }
            }
            break;

        case WBP_Katakana:
            if ((wbcSeqStart == WBP_Katakana) || /* WB13 */
                    (wbcSeqStart == WBP_ExtendNumLet)) /* WB13b */
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
            }
            /* No rule found, reset */
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
            }
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_Hebrew_Letter:
        case WBP_ALetter:
            if ((wbcSeqStart == WBP_Hebrew_Letter) &&
                    (wbcLast == WBP_Double_Quote)) /* WB7b,c */
            {
               if (wbcCur == WBP_Hebrew_Letter)
                 {
                     set_brks_to(s, brks, posLast, posCur, len,
                             WORDBREAK_NOBREAK, get_next_char);
                 }
               else
                 {
                     set_brks_to(s, brks, posLast, posCur, len,
                             WORDBREAK_BREAK, get_next_char);
                 }
            }
            else if (((wbcSeqStart == WBP_ALetter) ||
                        (wbcSeqStart == WBP_Hebrew_Letter)) || /* WB5,6,7 */
                    (wbcLast == WBP_Numeric) || /* WB10 */
                    (wbcSeqStart == WBP_ExtendNumLet)) /* WB13b */
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
            }
            /* No rule found, reset */
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
            }
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_Single_Quote:
            if (wbcLast == WBP_Hebrew_Letter) /* WB7a */
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
            }
            /* Fall through */

        case WBP_MidNumLet:
            if (((wbcLast == WBP_ALetter) ||
                        (wbcLast == WBP_Hebrew_Letter)) || /* WB6,7 */
                    (wbcLast == WBP_Numeric)) /* WB11,12 */
            {
                /* Go on */
            }
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
            }
            break;

        case WBP_MidLetter:
            if ((wbcLast == WBP_ALetter) ||
                    (wbcLast == WBP_Hebrew_Letter)) /* WB6,7 */
            {
                /* Go on */
            }
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
            }
            break;

        case WBP_MidNum:
            if (wbcLast == WBP_Numeric) /* WB11,12 */
            {
                /* Go on */
            }
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
            }
            break;

        case WBP_Numeric:
            if ((wbcSeqStart == WBP_Numeric) || /* WB8,11,12 */
                    ((wbcLast == WBP_ALetter) ||
                     (wbcLast == WBP_Hebrew_Letter)) || /* WB9 */
                    (wbcSeqStart == WBP_ExtendNumLet)) /* WB13b */
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
            }
            /* No rule found, reset */
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
            }
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_ExtendNumLet:
            /* WB13a,13b */
            if ((wbcSeqStart == wbcLast) &&
                ((wbcLast == WBP_ALetter) ||
                 (wbcLast == WBP_Hebrew_Letter) ||
                 (wbcLast == WBP_Numeric) ||
                 (wbcLast == WBP_Katakana) ||
                 (wbcLast == WBP_ExtendNumLet)))
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
            }
            /* No rule found, reset */
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
            }
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_Regional_Indicator:
            /* WB15,16 */
            if ((wbcSeqStart == WBP_Regional_Indicator) &&
                ((riCounter % 2) == 1))
            {
                set_brks_to(s, brks, posLast, posCur, len,
                        WORDBREAK_NOBREAK, get_next_char);
                riCounter = 0; /* Reset the sequence */
            }
            /* No rule found, reset */
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
                riCounter = 1;
            }
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        case WBP_Double_Quote:
            if (wbcLast == WBP_Hebrew_Letter) /* WB7b,c */
            {
               /* Go on */
            }
            else
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_BREAK, get_next_char);
                wbcSeqStart = wbcCur;
                posLast = posCur;
            }
            break;

        case WBP_WSegSpace:
            if (wbcLast == WBP_WSegSpace) /* WB3d */
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
                posLast = posCur;
                break;
            }
            /* Fall through */

        case WBP_Any:
            /* Check for rule WB3c */
            if (wbcLast == WBP_ZWJ && ub_is_extended_pictographic(ch))
            {
                set_brks_to(s, brks, posLast, posCur, len,
                            WORDBREAK_NOBREAK, get_next_char);
                posLast = posCur;
                break;
            }

            /* Allow breaks and reset */
            set_brks_to(s, brks, posLast, posCur, len,
                        WORDBREAK_BREAK, get_next_char);
            wbcSeqStart = wbcCur;
            posLast = posCur;
            break;

        default:
            /* Error, should never get here! */
            assert(0);
            break;
        }

        wbcLast = wbcCur;
        posCur = posNext;
        ch = get_next_char(s, len, &posNext);
    }

    /* WB2 */
    set_brks_to(s, brks, posLast, posNext, len,
                WORDBREAK_BREAK, get_next_char);
}

/**
 * Sets the word breaking information for a UTF-8 input string.
 *
 * @param[in]  s     input UTF-8 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input (reserved for future use)
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *                   #WORDBREAK_INSIDEACHAR
 */
void set_wordbreaks_utf8(
        const utf8_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_wordbreaks(s, len, lang, brks,
                   (get_next_char_t)ub_get_next_char_utf8);
}

/**
 * Sets the word breaking information for a UTF-16 input string.
 *
 * @param[in]  s     input UTF-16 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input (reserved for future use)
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *                   #WORDBREAK_INSIDEACHAR
 */
void set_wordbreaks_utf16(
        const utf16_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_wordbreaks(s, len, lang, brks,
                   (get_next_char_t)ub_get_next_char_utf16);
}

/**
 * Sets the word breaking information for a UTF-32 input string.
 *
 * @param[in]  s     input UTF-32 string
 * @param[in]  len   length of the input
 * @param[in]  lang  language of the input (reserved for future use)
 * @param[out] brks  pointer to the output breaking data, containing
 *                   #WORDBREAK_BREAK, #WORDBREAK_NOBREAK, or
 *                   #WORDBREAK_INSIDEACHAR
 */
void set_wordbreaks_utf32(
        const utf32_t *s,
        size_t len,
        const char *lang,
        char *brks)
{
    set_wordbreaks(s, len, lang, brks,
                   (get_next_char_t)ub_get_next_char_utf32);
}
