#include "http_server_private.h"

#include "dstrings.h"

#include <stdio.h>
#include <string.h>

namespace dmHttpServerPrivate
{
    ParseResult ParseHeader(char* header_str,
                            void* user_data,
                            void (*request)(void* user_data, const char* request_method, const char* resource, int major, int minor),
                            void (*header)(void* user_data, const char* key, const char* value),
                            void (*response)(void* user_data, int offset))
    {
        char* end = strstr(header_str, "\r\n\r\n");
        if (end == 0)
            return PARSE_RESULT_NEED_MORE_DATA;

        char* end_version = strstr(header_str, "\r\n");
        *end_version = '\0';

        char* method = header_str;
        char* end_method = strchr(method, ' ');
        if (!end_method)
            return PARSE_RESULT_SYNTAX_ERROR;
        *end_method = '\0';

        char* resource = end_method + 1;
        char* end_resource = strchr(resource, ' ');
        if (!end_resource)
            return PARSE_RESULT_SYNTAX_ERROR;
        *end_resource = '\0';

        int major, minor;
        int count = sscanf(end_resource+1, "HTTP/%d.%d", &major, &minor);
        if (count != 2)
        {
            return PARSE_RESULT_SYNTAX_ERROR;
        }

        /*
        // Find status string, ie "OK" in "HTTP/1.1 200 OK"
        char* tok;
        char* last;
        tok = dmStrTok(version_str, " ", &last);
        tok = dmStrTok(0, " ", &last);
        tok = dmStrTok(0, " ", &last);
        if (tok == 0)
            return PARSE_RESULT_SYNTAX_ERROR;*/

        request(user_data, method, resource, major, minor);

        // Skip \r\n\r\n
        end += 4;
        int c = *end;
        *end = '\0'; // Terminate headers (up to body)
        char* last;
        char* tok = dmStrTok(end_version + 2, "\r\n", &last);
        while (tok)
        {
            char* colon = strstr(tok, ":");
            if (!colon)
                return PARSE_RESULT_SYNTAX_ERROR;

            char* value = colon + 2;
            int c = *colon;
            *colon = '\0';
            header(user_data, tok, value);
            *colon = c;
            tok = dmStrTok(0, "\r\n", &last);
        }
        *end = c;

        response(user_data, (int) (end - header_str));

        return PARSE_RESULT_OK;
    }

} // namespace dmHttpServerPrivate
