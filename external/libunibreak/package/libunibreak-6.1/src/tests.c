/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4: */

/*
 * Test executer for libunibreak.
 *
 * Copyright (C) 2016 Tom Hacohen <tom at stosb dot com>
 * Copyright (C) 2016 Andreas Röver <roever at users dot sf dot net>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "linebreak.h"
#include "wordbreak.h"
#include "graphemebreak.h"

#include "test_skips.h"

#define TEST_LINE_LEN 10000

typedef enum
{
    TEST_TYPE_LINE,
    TEST_TYPE_WORD,
    TEST_TYPE_GRAPHEME
} Test_Type;

int main(int argc, char *argv[])
{
    const char *filename = "";
    FILE *fp;
    char line[TEST_LINE_LEN];
    unsigned int linenumber = 0;
    unsigned int testsSkipped = 0;
    unsigned int testsFailed = 0;
    unsigned int testsTotal = 0;
    Test_Type testType;
    clock_t t1;
    clock_t t2;

    char noBreak, mustBreak, insideChar;
    const unsigned int *testSkips; /* Zero terminated array of line numbers to skip in the test. */

    if (argc != 2)
    {
        return 1;
    }

    switch (*argv[1])
    {
    case 'l':
        filename = "LineBreakTest.txt";
        testType = TEST_TYPE_LINE;
        noBreak = LINEBREAK_NOBREAK;
        mustBreak = LINEBREAK_MUSTBREAK;
        insideChar = LINEBREAK_INSIDEACHAR;
        testSkips = testSkipsLine;
        break;
    case 'w':
        filename = "WordBreakTest.txt";
        testType = TEST_TYPE_WORD;
        noBreak = WORDBREAK_NOBREAK;
        mustBreak = WORDBREAK_BREAK;
        insideChar = WORDBREAK_INSIDEACHAR;
        testSkips = testSkipsWord;
        break;
    case 'g':
        filename = "GraphemeBreakTest.txt";
        testType = TEST_TYPE_GRAPHEME;
        noBreak = GRAPHEMEBREAK_NOBREAK;
        mustBreak = GRAPHEMEBREAK_BREAK;
        insideChar = GRAPHEMEBREAK_INSIDEACHAR;
        testSkips = testSkipsGrapheme;
        break;
    default:
        return 1;
    }

    if (NULL == (fp = fopen(filename, "r")))
    {
        fprintf(stderr, "Error opening file");
        return 1;
    }

    t1 = clock();
    while (fgets(line, sizeof(line), fp))
    {
        char *linepos = line;
        char breaksDesired[TEST_LINE_LEN] = { 0 };
        char breaksActual[TEST_LINE_LEN] = { 0 };
        utf32_t txt[TEST_LINE_LEN] = { 0 };
        int i, len;

        linenumber++;
        if (strlen(line) == TEST_LINE_LEN - 1)
        {
            fprintf(stderr, "Input line %u is too long!\n", linenumber);
            break;
        }

        if (line[0] == '#')
            continue;

        if (linenumber == *testSkips)
        {
            testsSkipped++;
            testSkips++;
            continue;
        }

        len = 0;
        while (*linepos)
        {
            switch (*linepos)
            {
            case '\xC3':
                // Makeshift utf8 parser for the two relevant chars
                linepos++;
                if (*linepos == '\xB7')
                {
                    breaksDesired[len] = mustBreak;
                }
                else if (*linepos == '\x97')
                {
                    breaksDesired[len] = noBreak;
                }
                else
                {
                    return 2;
                }
                linepos++;
                break;

            case ' ':
            case '\t':
                // ignore this character
                linepos++;
                break;

            case '#':
                // End
                goto loop_break;
                break;

            default:
                txt[len] = strtol(linepos, &linepos, 16);
                len++;
                break;
            }
        }
    loop_break:  // Hard exit from the above loop

        // the test data contains information for a break BEFORE the string
        // which we suppose is always a break, so fill that in and let the
        // breaking function start with the break after that one
        switch (testType)
        {
        case TEST_TYPE_LINE:
            breaksActual[0] = noBreak;
            set_linebreaks_utf32(txt, len, "-strict", breaksActual + 1);
            break;
        case TEST_TYPE_WORD:
            breaksActual[0] = mustBreak;
            set_wordbreaks_utf32(txt, len, "", breaksActual + 1);
            break;
        case TEST_TYPE_GRAPHEME:
            breaksActual[0] = mustBreak;
            set_graphemebreaks_utf32(txt, len, "", breaksActual + 1);
            break;
        }

        testsTotal++;

        for (i = 0; i <= len; i++)
        {
            if ((testType == TEST_TYPE_LINE) &&
                (breaksActual[i] == LINEBREAK_ALLOWBREAK ||
                 breaksActual[i] == LINEBREAK_INDETERMINATE))
            {
                /* The Unicode test data doesn't differentiate between
                 * these states, so neither should we. */
                breaksActual[i] = LINEBREAK_MUSTBREAK;
            }

            if (breaksActual[i] != breaksDesired[i])
            {
                testsFailed++;
                printf("Issues in line %d:\n\t%s\tPosition %d: expected %d got %d\n",
                       linenumber, line, i, breaksDesired[i], breaksActual[i]);
            }
        }
    }
    t2 = clock();

    fclose(fp);

    unsigned int testsPassed = testsTotal - testsFailed;
    if (testsFailed > 0)
        putchar('\n');
    printf("%s: Passed %d out of %d (%d%%)", filename, testsPassed,
           testsTotal, testsPassed * 100 / testsTotal);
    if (testsSkipped > 0)
       printf(", and skipped %d", testsSkipped);
    printf("\nTesting takes %g ms\n\n", (t2 - t1) * 1000.0 / CLOCKS_PER_SEC);

    return (testsFailed > 0);
}
