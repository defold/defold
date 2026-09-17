/*
 * Copyright (C) 2025 Muhammad Tayyab Akram
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

#ifndef SHEENBIDI_PARSER_DERIVED_CORE_PROPERTIES_H
#define SHEENBIDI_PARSER_DERIVED_CORE_PROPERTIES_H

#include <bitset>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "DataFile.h"
#include "UnicodeVersion.h"

namespace SheenBidi {
namespace Parser {

class DerivedCoreProperties : public DataFile {
public:
    DerivedCoreProperties(const std::string &directory);

    const UnicodeVersion &version() const { return m_version; }

    std::vector<std::string> getProperties(uint32_t codePoint) const;
    bool hasProperty(uint32_t codePoint, const std::string &property) const;
    bool isDefaultIgnorable(uint32_t codePoint) const;

private:
    using PropertyBit = uint8_t;

    static constexpr uint32_t MAX_PROPERTIES = 64;

    UnicodeVersion m_version;
    PropertyBit m_defaultIgnorableBit;

    std::vector<std::bitset<MAX_PROPERTIES>> m_codePointProperties;
    std::unordered_map<std::string, PropertyBit> m_propertyToBit;

    PropertyBit insertProperty(const std::string &property);
};

}
}

#endif
