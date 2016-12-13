#include <gtest/gtest.h>
#include <../launcher.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef _WIN32

#include <windows.h>
#include <assert.h>

#define MAX_ARG_LEN 20
#define MAX_ARGC 20
#define ITERATIONS 100000

static const char* bogus_content = " \t\n\v\"\\abc";

const char* char_str(char c)
{
    static char buf[100];

    switch(c)
    {
    case '\t': return "\\t";
    case '\n': return "\\n";
    case '\v': return "\\v";
    default:
        sprintf(buf, "%c", c);
        return buf;
    }
}

void dump_argv(int argc, char const* argv[])
{
    for (int a = 0; a < argc; ++a)
    {
        printf("argv[%d] = [", a);
        for (const char* b = argv[a]; *b; ++b) {
            printf("%x(%s) ", (int)*b, char_str(*b));
        }
        printf("]\n");
    }
}

void dump_wide_argv(int parsed_argc, LPWSTR* parsed_argv)
{
    for (int a = 0; a < parsed_argc; ++a)
    {
        char narrow_parsed_arg[(MAX_ARG_LEN) + 1];
        char* out = narrow_parsed_arg;

        for (wchar_t* wc = parsed_argv[a]; *wc != 0; ++wc) {
            assert(*wc < 256);
            *out++ = (char) *wc;
        }

        *out = 0;

        printf("argw %i = [", a);
        for (const char* b = narrow_parsed_arg; *b; ++b) {
             printf("%x(%s) ", (int)*b, char_str(*b));
        }
        printf("]\n");
    }
}

void dump_wide_buffer(wchar_t const* wide_buffer)
{
    printf("buffer: [");
    for (const wchar_t* b = wide_buffer; *b != 0; ++b) {
             printf("%2x(%s) ", (int)*b, char_str((char)*b));
    }
    printf("]\n");
}

void CheckRoundtrip(int argc, char const* argv[])
{
    char buffer[10000];

    QuoteArgv(argv, buffer);
        
    wchar_t wide_buffer[10000];

    char* b = buffer;
    wchar_t* wp = wide_buffer;

    while (*wp++ = *b++) {}

    int parsed_argc = 0;
    LPWSTR* parsed_argv = CommandLineToArgvW(wide_buffer, &parsed_argc);

    ASSERT_EQ(parsed_argc, argc);

    if (parsed_argc == argc)
    {
        for (int a = 0; a < argc; ++a)
        {
            char narrow_parsed_arg[(MAX_ARG_LEN) + 1];
            char* out = narrow_parsed_arg;

            for (wchar_t* wc = parsed_argv[a]; *wc != 0; ++wc) {
                ASSERT_LT((int) *wc, 256);
                *out++ = (char) *wc;
            }

            *out = 0;

            ASSERT_STREQ(argv[a], narrow_parsed_arg);
        }
    }

    LocalFree(parsed_argv);
}

TEST(Launcher, CreateProcessArgQuoting)
{
    unsigned int seed = (unsigned int) time(0);

    printf("Seed: %u\n", seed);
    fflush(stdout);

    srand(seed);
    
    for (int i = 0; i < (ITERATIONS); ++i)
    {
        int argc = rand() % (MAX_ARGC) + 1;

        const char** argv = new const char*[argc + 1];

        argv[0] = "dummy.exe"; // need a sane first argument, CommandLineToArgvW punts on f.i. '"' as name of application.

        for (int a = 1; a < argc; ++a)
        {
            int len = rand() % (MAX_ARG_LEN) + 1;
            char* bogus = new char[len + 1];

            for (int c = 0; c < len; ++c)
            {
                bogus[c] = bogus_content[rand() % 9];
            }

            bogus[len] = 0;

            argv[a] = bogus;
        }

        argv[argc] = 0;

        CheckRoundtrip(argc, argv);

        for (int a = 1; a < argc; ++a)
        {
            delete[] argv[a];
        }

        delete[] argv;
    }
}

#endif

int main(int argc, char* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
