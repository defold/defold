#ifndef DM_HTTP_CLIENT_PRIVATE_H
#define DM_HTTP_CLIENT_PRIVATE_H

namespace dmHttpClientPrivate
{
    enum ParseResult
    {
        PARSE_RESULT_NEED_MORE_DATA = 1,
        PARSE_RESULT_OK = 0,
        PARSE_RESULT_SYNTAX_ERROR = -1,
    };

    ParseResult ParseHeader(char* header_str,
                            void* user_data,
                            bool end_of_receive,
                            void (*version)(void* user_data, int major, int minor, int status, const char* status_str),
                            void (*header)(void* user_data, const char* key, const char* value),
                            void (*body)(void* user_data, int offset));

} // namespace dmHttpClientPrivate

#endif // DM_HTTP_CLIENT_PRIVATE_H
