/* vim: set expandtab softtabstop=4 shiftwidth=4: */

/*
 * Show possible linebreak points in a UTF-8 sequence.
 *
 * Compilation:
 *   cc linebreak_test.c -lunibreak -liconv
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  You may
 * freely use it without any restrictions, including copying the
 * whole or part of it to use in other programs.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linebreak.h>
#include <linebreakdef.h>
#include <iconv.h>

#define MAXCHARS    16384

/* Show usage */
void usage(const char *progname)
{
    fprintf(stderr,
"Usage: %s [-l en/de/es/fr/ru/zh] [-t output_encoding] input_utf8_file\n",
            progname);
}

/* Simplistic function to output a maximum four-byte sequence. */
void putchar_utf8(utf32_t ch)
{
    if (ch < 0x80)
        putchar(ch);
    else if (ch < 0x800)
    {
        putchar(0xC0 | (ch >> 6));
        putchar(0x80 | (ch & 0x3F));
    }
    else if (ch < 0x10000)
    {
        putchar(0xE0 | (ch >> 12));
        putchar(0x80 | ((ch >> 6) & 0x3F));
        putchar(0x80 | (ch & 0x3F));
    }
    else
    {
        putchar(0xF0 | (ch >> 18));
        putchar(0x80 | ((ch >> 12) & 0x3F));
        putchar(0x80 | ((ch >> 6) & 0x3F));
        putchar(0x80 | (ch & 0x3F));
    }
}

/* Output a UTF-8 character via libiconv */
void putchar_iconv(iconv_t ic, const char* buf, size_t count)
{
    char outbuf[5];
    char *inp;
    char *outp;
    size_t i;
    size_t bytes;

    /* Alas, most of the platforms I test now do not have const in the
     * second parameter of the iconv function.  I never understand why
     * someone chose to REMOVE the const in POSIX 2004 (so the standard
     * encourages const-incorrect code).  The decision looked really
     * foolish to me.  Anyway, it makes the cast necessary.  A big pain
     * is that this cast problem makes the current code cause a warning
     * when a platform declares a semantically correct iconv function,
     * as char** cannot be converted to const char** implicitly, or vice
     * versa.  An explanation is here:
     *
     *  http://mail-index.netbsd.org/tech-userlevel/2004/07/28/0006.html
     *
     * A useful trick for C++ (only) is here:
     *
     *  http://stackoverflow.com/questions/11421439/how-can-i-portably-call-a-c-function-that-takes-a-char-on-some-platforms-and
     */
    inp = (char*)buf;
    outp = outbuf;
    i = sizeof outbuf;
    if (iconv(ic, &inp, &count, &outp, &i) != (size_t)-1)
    {
        bytes = sizeof outbuf - i;
    }
    else
    {
        outbuf[0] = '?';
        bytes = 1;
    }
    for (i = 0; i < bytes; ++i)
    {
        putchar(outbuf[i]);
    }
}

/* Get the byte length of a UTF-8 sequence */
size_t get_utf8_char_len(char lead)
{
    unsigned char ch = (unsigned char)lead;
    if (ch < 0xC2 || ch > 0xF4) {
        /* One-byte sequence, tail (should not occur), or invalid */
        return 1;
    } else if (ch < 0xE0) {
        /* Two-byte sequence */
        return 2;
    } else if (ch < 0xF0) {
        /* Three-byte sequence */
        return 3;
    } else {
        /* Four-byte sequence */
        return 4;
    }
}

/* Output a UTF-8 string via libiconv */
void puts_iconv(const char *s, iconv_t ic)
{
    unsigned char ch;
    while ( (ch = (unsigned char)*s)) {
        size_t char_len = get_utf8_char_len(ch);
        putchar_iconv(ic, s, char_len);
        s += char_len;
    }
}

int main(int argc, char *argv[])
{
    utf8_t buffer[MAXCHARS];
    char brks[MAXCHARS];
    FILE *fp;
    size_t count;
    size_t i;
    size_t j;
    utf32_t ch;
    const char *lang = "";
    iconv_t ic = (iconv_t)-1;

    /* Parse options; done manually to ensure portability */
    i = 1;
    while (i + 1 < argc && argv[i][0] == '-')
    {
        if (strcmp(argv[i], "-l") != 0 &&
            strcmp(argv[i], "-t") != 0)
        {
            fprintf(stderr, "Invalid option: `%s'\n", argv[i]);
            exit(1);
        }
        j = i + 1;
        if (j >= argc)
        {
            fprintf(stderr, "Option value missing\n");
            exit(1);
        }
        switch (argv[i][1])
        {
        case 'l':
            lang = argv[j];
            i += 2;
            break;
        case 't':
            ic = iconv_open(argv[j], "utf-8");
            if (ic == (iconv_t)-1)
            {
                fprintf(stderr, "Unrecognized encoding: `%s'\n", argv[j]);
            }
            i += 2;
            break;
        }
    }

    /* Check for the filename argument */
    if (i + 1 != argc)
    {
        usage(argv[0]);
        exit(1);
    }

    /* Read the input file up to MAXCHARS bytes */
    if ( (fp = fopen(argv[i], "rb")) == NULL)
    {
        perror("Cannot open file");
        exit(1);
    }
    count = fread(buffer, sizeof(utf8_t), MAXCHARS, fp);
    fclose(fp);

    /* Show the breaking points */
    set_linebreaks_utf8(buffer, count, lang, brks);

    /* Output to stdout */
    for (i = 0;;)
    {
        j = i;
        ch = ub_get_next_char_utf8(buffer, count, &i);
        if (ch == EOS)
            break;
        if (ic != (iconv_t)-1)
        {
            putchar_iconv(ic, (char *)buffer + j, i - j);
        }
        else
        {
            putchar_utf8(ch);
        }
        switch (brks[i - 1])
        {
        case LINEBREAK_MUSTBREAK:
            if (ic != (iconv_t)-1)
            {
                puts_iconv("================================\n", ic);
            }
            else
            {
                printf    ("================================\n");
            }
            break;
        case LINEBREAK_ALLOWBREAK:
            if (ic != (iconv_t)-1)
            {
                puts_iconv("|\n", ic);
            }
            else
            {
                printf    ("|\n");
            }
            break;
        default:
            break;
        }
    }

    /* Clean up */
    if (ic != (iconv_t)-1)
    {
        iconv_close(ic);
    }

    return 0;
}
