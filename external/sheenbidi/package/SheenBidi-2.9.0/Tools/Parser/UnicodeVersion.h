/*
 * Copyright (C) 2015-2025 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SHEENBIDI_PARSER_UNICODE_VERSION_H
#define SHEENBIDI_PARSER_UNICODE_VERSION_H

#include <string>

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

namespace SheenBidi {
namespace Parser {

class UnicodeVersion {
public:
    UnicodeVersion() = default;
    UnicodeVersion(const std::string &versionLine);

    int major() const;
    int minor() const;
    int micro() const;
    const std::string &versionString() const;

private:
    int m_major = 0;
    int m_minor = 0;
    int m_micro = 0;
    std::string m_versionString;
};

}
}

#endif
