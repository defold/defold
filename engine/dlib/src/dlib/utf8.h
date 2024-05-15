// Copyright 2020-2024 The Defold Foundation
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

#ifndef DM_UTF8
#define DM_UTF8

#include <stdint.h>
#include <stdlib.h>

namespace dmUtf8
{
    /**
     * Get number of unicode characters in utf-8 string
     * @param str utf8 string
     * @return number of characters
     */
    uint32_t StrLen(const char* str);

    /**
     * Get next unicode character in utf-8 string. Iteration terminates at '\0' and repeated invocations will return '\0'
     * @param str pointer to string. the pointer value is updated
     * @return decoded unicode character
     */
    uint32_t NextChar(const char** str);

    /**
     * Convert a 16-bit unicode character to utf-8
     * @note Buffer must be of at least 4 characters. The string is *not* NULL-terminated
     * @param chr character to convert
     * @param buf output buffer
     * @return number of characters in buffer
     */
    uint32_t ToUtf8(uint16_t chr, char* buf);
}

#endif // DM_UTF8
