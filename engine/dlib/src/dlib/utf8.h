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

#ifndef DM_UTF8_H
#define DM_UTF8_H

#include <stddef.h>
#include <dmsdk/dlib/utf8.h>

namespace dmUtf8
{
    // Decodes the first Unicode scalar and returns its byte length. Empty,
    // invalid or truncated input returns zero and leaves codepoint untouched.
    uint32_t DecodeSequence(const char* text, size_t size, uint32_t* codepoint);
    // Length of the first valid Unicode scalar's UTF-8 sequence, or zero for
    // empty, invalid or truncated input. Reads no more than size bytes.
    uint32_t ValidateSequence(const char* text, uint32_t size);
    // Valid prefix length; size on success, first invalid sequence offset otherwise.
    uint32_t Validate(const char* text, uint32_t size);
}

#endif // DM_UTF8_H
