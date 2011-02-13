#ifndef DM_HTTP_SERVER_PRIVATE_H
#define DM_HTTP_SERVER_PRIVATE_H

namespace dmHttpServerPrivate
{
    enum ParseResult
    {
        PARSE_RESULT_NEED_MORE_DATA = 1,
        PARSE_RESULT_OK = 0,
        PARSE_RESULT_SYNTAX_ERROR = -1,
    };

    ParseResult ParseHeader(char* header_str,
                            void* user_data,
                            void (*request)(void* user_data, const char* request_method, const char* resource, int major, int minor),
                            void (*header)(void* user_data, const char* key, const char* value),
                            void (*response)(void* user_data, int offset));

} // namespace dmHttpServerPrivate

#endif // DM_HTTP_SERVER_PRIVATE_H
