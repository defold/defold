#include <stdio.h>
#include <string.h>
#include "dlib/dstrings.h"
#include "http_client_private.h"

namespace dmHttpClientPrivate
{
    ParseResult ParseHeader(char* header_str,
                            void* user_data,
                            bool end_of_receive,
                            void (*version)(void* user_data, int major, int minor, int status, const char* status_str),
                            void (*header)(void* user_data, const char* key, const char* value),
                            void (*body)(void* user_data, int offset))
    {
        // Check if we have a body section by searching for two new-lines, do this before parsing version since we do destructive string termination
        char* body_start = strstr(header_str, "\r\n\r\n");

        // Always try to parse version and status
        char* version_str = header_str;
        char* end_version = strstr(header_str, "\r\n");
        if (end_version == 0)
            return PARSE_RESULT_NEED_MORE_DATA;
        
        char store_end_version = *end_version;
        *end_version = '\0';

        int major, minor, status;
        int count = sscanf(version_str, "HTTP/%d.%d %d", &major, &minor, &status);
        if (count != 3)
        {
            return PARSE_RESULT_SYNTAX_ERROR;
        }

        if (body_start != 0)
        {
            // Skip \r\n\r\n
            body_start += 4;
        }
        else
        {
            // According to the HTTP spec, all responses should end with double line feed to indicate end of headers
            // Unfortunately some server implementations only end with one linefeed if the response is '204 No Content' so we take special measures
            // to force parsing of headers if we have received no more data and the we get a 204 status
            if(end_of_receive && status == 204)
            {
                // Treat entire input as just headers
                body_start = (end_version + 1) + strlen(end_version + 1);
            }
            else
            {
                // Restore string termination since we need more data and will likely try again
                *end_version = store_end_version;
                return PARSE_RESULT_NEED_MORE_DATA;
            }
        }

        // Find status string, ie "OK" in "HTTP/1.1 200 OK"
        char* status_string = strchr(version_str, ' ');
        status_string = status_string ? strchr(status_string + 1, ' ') : 0;
        if (status_string == 0)
            return PARSE_RESULT_SYNTAX_ERROR;

        version(user_data, major, minor, status, status_string + 1);

        char store_body_start = *body_start;
        *body_start = '\0'; // Terminate headers (up to body)
        char* tok;
        char* last;
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
        *body_start = store_body_start;

        body(user_data, (int) (body_start - header_str));

        return PARSE_RESULT_OK;
    }

} // namespace dmHttpClientPrivate
