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

#ifndef SHEENBIDI_PARSER_BIDI_MIRRORING_H
#define SHEENBIDI_PARSER_BIDI_MIRRORING_H

#include <cstdint>
#include <string>
#include <vector>

#include "DataFile.h"
#include "UnicodeVersion.h"

namespace SheenBidi {
namespace Parser {

class BidiMirroring : public DataFile {
public:
    BidiMirroring(const std::string &directory);

    const UnicodeVersion &version() const { return m_version; }

    uint32_t firstCodePoint() const { return m_firstCodePoint; }
    uint32_t lastCodePoint() const { return m_lastCodePoint; }

    uint32_t mirrorOf(uint32_t codePoint) const;

private:
    UnicodeVersion m_version;

    uint32_t m_firstCodePoint = UINT32_MAX;
    uint32_t m_lastCodePoint = 0;

    std::vector<uint32_t> m_mirrors;
};

}
}

#endif
