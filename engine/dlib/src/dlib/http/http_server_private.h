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
