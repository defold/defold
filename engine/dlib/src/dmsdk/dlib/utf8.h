// Copyright 2020-2025 The Defold Foundation
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

#ifndef DMSDK_UTF8_H
#define DMSDK_UTF8_H

#include <stdint.h>

/*# SDK Utf8 API documentation
 *
 * @document
 * @name Utf8
 * @namespace dmUtf8
 * @path engine/dlib/src/dmsdk/dlib/utf8.h
 */

namespace dmUtf8
{
    /*#
     * Get number of unicode characters in utf-8 string
     * @name StrLen
     * @param str [type: const char*] Utf8 string
     * @return length [type: uint32_t] Number of characters
     * @examples
     * ```c++
     * const char* s = "åäöÅÄÖ";
     * uint32_t count = dmUtf8::StrLen(s);
     * ```
     */
    uint32_t StrLen(const char* str);

    /*# get next unicode character in utf-8 string.
     * Get next unicode character in utf-8 string. Iteration terminates at '\0' and repeated invocations will return '\0'
     * @name NextChar
     * @param str [type: const char**] Pointer to string. The pointer value is updated
     * @return chr [type: uint32_t] Decoded unicode character
     * @examples
     * ```c++
     * const char* s = "åäöÅÄÖ";
     * char* cursor = s;
     * uint32_t codepoint = 0;
     * while (codepoint = dmUtf8::NextChar(&cursor))
     * {
     *     // ...
     * }
     * ```
     */
    uint32_t NextChar(const char** str);

    /*#
     * Convert a 16-bit unicode character to utf-8
     * @note Buffer must be of at least 4 characters. The string is *not* NULL-terminated
     * @name ToUtf8
     * @param chr [type: uint16_t] Character to convert
     * @param buf [type: char*] output Buffer (at least 4 bytes)
     * @return length [type: uint32_t] Number of characters in buffer
     */
    uint32_t ToUtf8(uint16_t chr, char* buf);
}

#endif // DMSDK_UTF8_H
