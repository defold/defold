#include "template.h"

#include "dstrings.h"
#include "log.h"
#include "math.h"

#include <string.h>

namespace dmTemplate
{
    Result Format(void* user_data, char* buf, uint32_t buf_size, const char* fmt, ReplaceCallback call_back)
    {
        char* current = buf;
        char* end = buf + buf_size;
        char key[64];

#define WRITE_CHAR(c) if (current >= end) return RESULT_BUFFER_TOO_SMALL; *current = c; ++current;

        while (*fmt)
        {
            if (fmt[0] == '$' && fmt[1] == '{')
            {
                const char* brace = strchr(fmt + 2, '}');
                if (!brace)
                    return RESULT_SYNTAX_ERROR;

                fmt += 2;
                dmStrlCpy(key, fmt, dmMath::Min((int) sizeof(key), (int) (brace - fmt + 1)));

                const char* value = call_back(user_data, key);
                if (!value)
                {
                    dmLogInfo("Missing replacement for key '%s'", key);
                    return RESULT_MISSING_REPLACEMENT;
                }
                while (*value)
                {
                    WRITE_CHAR(*value);
                    ++value;
                }

                fmt = brace + 1;
            }
            else
            {
                WRITE_CHAR(*fmt);
                ++fmt;
            }
        }

        WRITE_CHAR('\0')

        return RESULT_OK;
    }

}  // namespace dmTemplate
