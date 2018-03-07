#include <stdio.h>
#include <string.h>
#include "dlib/dstrings.h"
#include "http_client_private.h"

namespace dmHttpClientPrivate
{
    ParseResult ParseHeader(char* header_str,
                            void* user_data,
                            void (*version)(void* user_data, int major, int minor, int status, const char* status_str),
                            void (*header)(void* user_data, const char* key, const char* value),
                            void (*body)(void* user_data, int offset))
    {
        char* version_str = header_str;
        char* end_version = strstr(header_str, "\r\n");
        if (end_version == 0)
            return PARSE_RESULT_NEED_MORE_DATA;
        *end_version = '\0';

        int major, minor, status;
        int count = sscanf(version_str, "HTTP/%d.%d %d", &major, &minor, &status);
        if (count != 3)
        {
            return PARSE_RESULT_SYNTAX_ERROR;
        }

        // Find status string, ie "OK" in "HTTP/1.1 200 OK"
        char* tok;
        char* last;
        tok = dmStrTok(version_str, " ", &last);
        tok = dmStrTok(0, " ", &last);
        tok = dmStrTok(0, " ", &last);
        if (tok == 0)
            return PARSE_RESULT_SYNTAX_ERROR;

        version(user_data, major, minor, status, tok);

        char* end = strstr(header_str, "\r\n\r\n");
        if (end == 0)
        {
            // The response contains no headers and a zero-length body
            // Status '204 No Content' is an example of such a response
            body(user_data, 0);
            return PARSE_RESULT_OK;
        }

        // Skip \r\n\r\n
        end += 4;
        int c = *end;
        *end = '\0'; // Terminate headers (up to body)
        tok = dmStrTok(end_version + 2, "\r\n", &last);
        while (tok)
        {
            char* colon = strstr(tok, ":");
            if (!colon)
                return PARSE_RESULT_SYNTAX_ERROR;

            char* value = colon + 2;
            while (*value == ' ') {
                value++;
            }

            int c = *colon;
            *colon = '\0';
            header(user_data, tok, value);
            *colon = c;
            tok = dmStrTok(0, "\r\n", &last);
        }
        *end = c;

        body(user_data, (int) (end - header_str));

        return PARSE_RESULT_OK;
    }

} // namespace dmHttpClientPrivate
