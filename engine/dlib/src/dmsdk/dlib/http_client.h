// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_HTTP_CLIENT_H
#define DMSDK_HTTP_CLIENT_H

/*# SDK Http Client API documentation
 * Http client functions.
 *
 * @document
 * @name Http Client
 * @namespace dmHttpClient
 * @path engine/dlib/src/dmsdk/dlib/hash.h
 */

namespace dmHttpClient
{
    /*# header parse result enumeration
     *
     * Header parse result enumeration.
     *
     * @enum
     * @name ParseResult
     * @member dmHttpClient::PARSE_RESULT_NEED_MORE_DATA = 1
     * @member dmHttpClient::PARSE_RESULT_OK = 0
     * @member dmHttpClient::PARSE_RESULT_SYNTAX_ERROR = -1
     */
    enum ParseResult
    {
        PARSE_RESULT_NEED_MORE_DATA = 1,
        PARSE_RESULT_OK = 0,
        PARSE_RESULT_SYNTAX_ERROR = -1,
    };

    /*# parse the headers
     * Parse the header data and make callbacks for each header/version entry and the start of the body.
     *
     * @name ParseHeader
     * @param header_str [type:char*] http response headers. Must be a null terminated string.
     * @param user_data [type:const void*] user data to the callbacks.
     * @param end_of_receive [type:bool] true if there is no more data
     * @param version_cbk [type:function] callback for the http version
     *     void (*version_cbk)(void* user_data, int major, int minor, int status, const char* status_str);
     * @param header_cbk [type:function] callback for each header/value pair
     *     void (*header_cbk)(void* user_data, const char* key, const char* value);
     * @param body_cbk [type:function] callback to note the start offset of the body data.
     *     void (*body_cbk)(void* user_data, int offset)
     * @return result [type:dmHttpClient::ParseResult] the parse result
     * @note This function is destructive to the input data.
     */
    ParseResult ParseHeader(char* header_str,
                            void* user_data,
                            bool end_of_receive,
                            void (*version_cbk)(void* user_data, int major, int minor, int status, const char* status_str),
                            void (*header_cbk)(void* user_data, const char* key, const char* value),
                            void (*body_cbk)(void* user_data, int offset));

} // namespace dmHttpClient

#endif // DMSDK_HTTP_CLIENT_H
