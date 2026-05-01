/*
 * Copyright (C) 2020-2025 Muhammad Tayyab Akram
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

#ifndef SHEENBIDI_PARSER_DERIVED_BIDI_CLASS_H
#define SHEENBIDI_PARSER_DERIVED_BIDI_CLASS_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "DataFile.h"
#include "DerivedCoreProperties.h"
#include "PropertyValueAliases.h"
#include "PropList.h"
#include "UnicodeVersion.h"

namespace SheenBidi {
namespace Parser {

class DerivedBidiClass : public DataFile {
public:
    DerivedBidiClass(const std::string &directory,
        const PropList &propList,
        const DerivedCoreProperties &derivedCoreProperties,
        const PropertyValueAliases &propertyValueAliases);

    const UnicodeVersion &version() const { return m_version; }

    uint32_t firstCodePoint() const { return m_firstCodePoint; }
    uint32_t lastCodePoint() const { return m_lastCodePoint; }

    const std::string &bidiClassOf(uint32_t codePoint) const;

private:
    using BidiClassID = uint8_t;

    UnicodeVersion m_version;

    uint32_t m_firstCodePoint = 0;
    uint32_t m_lastCodePoint = 0;

    std::vector<BidiClassID> m_bidiClasses;
    std::unordered_map<std::string, BidiClassID> m_bidiClassToID;
    std::vector<std::string> m_idToBidiClass;

    void readMissingBidiClasses(const PropertyValueAliases &propertyValueAliases);
    void applyBNRules(const PropList &propList, const DerivedCoreProperties &derivedCoreProperties);
    void readActualBidiClasses();

    BidiClassID insertBidiClass(const std::string &bidiClass);
};

}
}

#endif
