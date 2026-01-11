// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifdef _WIN32

#include <launcher.h>
#include <ctype.h>

// CreateProcess argument quoting is bizarre, below based on:
// https://blogs.msdn.microsoft.com/twistylittlepassagesallalike/2011/04/23/everyone-quotes-command-line-arguments-the-wrong-way/

static char* QuoteArg(char const* arg, char* buffer)
{
    const char backslash = '\\';
    const char double_quote = '"';

    bool needs_quoting = false;

    for (char const* scan = arg; *scan != 0; ++scan)
    {
        if (isspace(*scan) || *scan == double_quote)
        {
            needs_quoting = true;
            break;
        }
    }

    if (!needs_quoting)
    {
        // no whitespace nor " in string: just copy
        while (*arg != 0)
        {
            *buffer++ = *arg++;
        }
    }
    else
    {
        *buffer++ = double_quote;

        while (*arg != 0)
        {
            int backslash_count = 0;

            while (*arg == backslash)
            {
                ++backslash_count;
                ++arg;
            }

            if (*arg == 0)
            {
                // reached end of arg: quote all backslashes
                while (backslash_count != 0)
                {
                    *buffer++ = backslash;
                    *buffer++ = backslash;
                    --backslash_count;
                }
            }
            else if (*arg == double_quote)
            {
                // reached " in arg: quote all backslashes ...
                while (backslash_count != 0)
                {
                    *buffer++ = backslash;
                    *buffer++ = backslash;
                    --backslash_count;
                }

                // ... and add a quoted "
                *buffer++ = backslash;
                *buffer++ = *arg++;
            }
            else
            {
                // backslashes should not be quoted unless terminated by " or end of arg, so no quoting
                while (backslash_count != 0)
                {
                    *buffer++ = backslash;
                    --backslash_count;
                }

                // copy next char
                *buffer++ = *arg++;
            }
        }

        *buffer++ = double_quote;
    }

    return buffer;
}

void QuoteArgv(const char* argv[], char* cmd_line)
{
    for (int i = 0; argv[i] != 0; ++i)
    {
	if (i != 0)
	{
	    *cmd_line++ = ' ';
	}

	cmd_line = QuoteArg(argv[i], cmd_line);
    }

    *cmd_line = 0;
}

#endif
