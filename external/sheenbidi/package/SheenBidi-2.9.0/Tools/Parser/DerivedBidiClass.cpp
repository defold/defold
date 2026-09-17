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

#include <algorithm>
#include <cstdint>
#include <string>

#include "DataFile.h"
#include "DerivedCoreProperties.h"
#include "PropertyValueAliases.h"
#include "PropList.h"
#include "DerivedBidiClass.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_DERIVED_BIDI_CLASS = "DerivedBidiClass.txt";
static const string BIDI_CLASS_MISSING = "L";
static const string BIDI_CLASS_BOUNDARY_NEUTRAL = "BN";

DerivedBidiClass::DerivedBidiClass(const string &directory,
    const PropList &propList,
    const DerivedCoreProperties &derivedCoreProperties,
    const PropertyValueAliases &propertyValueAliases) :
    DataFile(directory, FILE_DERIVED_BIDI_CLASS),
    m_bidiClasses(CodePointCount)
{
    // Insert the default BidiClassID for all code points.
    insertBidiClass(BIDI_CLASS_MISSING);

    Line line;
    string bidiClass;
    string fullName;

    if (readLine(line)) {
        m_version = line.scanVersion();
    }

    readMissingBidiClasses(propertyValueAliases);
    applyBNRules(propList, derivedCoreProperties);
    readActualBidiClasses();
}

void DerivedBidiClass::readMissingBidiClasses(const PropertyValueAliases &propertyValueAliases) {
    Line line;
    string fullName;

    while (readLine(line)) {
        if (line.isEmpty()) {
            continue;
        }

        if (line.match('#')) {
            if (!line.match("@missing:", MatchRule::Trimmed)) {
                continue;
            }

            auto range = line.parseCodePointRange();
            line.getField(fullName);

            const auto &abbreviation = propertyValueAliases.abbreviationForBidiClass(fullName);
            auto id = insertBidiClass(abbreviation);

            for (auto codePoint = range.first; codePoint <= range.second; codePoint++) {
                m_bidiClasses.at(codePoint) = id;
            }

            m_lastCodePoint = max(m_lastCodePoint, range.second);
        }
    }
}

void DerivedBidiClass::applyBNRules(const PropList &propList, const DerivedCoreProperties &derivedCoreProperties) {
    auto id = insertBidiClass(BIDI_CLASS_BOUNDARY_NEUTRAL);

    for (uint32_t codePoint = 0; codePoint < CodePointCount; codePoint++) {
        if (derivedCoreProperties.isDefaultIgnorable(codePoint)
            || propList.isNoncharacter(codePoint)) {
            m_bidiClasses.at(codePoint) = id;
            m_lastCodePoint = max(m_lastCodePoint, codePoint);
        }
    }
}

void DerivedBidiClass::readActualBidiClasses() {
    reset();

    Line line;
    string bidiClass;

    while (readLine(line)) {
        if (line.isEmpty() || line.match('#')) {
            continue;
        }

        auto range = line.parseCodePointRange();
        line.getField(bidiClass);

        auto id = insertBidiClass(bidiClass);
        for (auto codePoint = range.first; codePoint <= range.second; codePoint++) {
            m_bidiClasses.at(codePoint) = id;
        }

        m_lastCodePoint = max(m_lastCodePoint, range.second);
    }
}

DerivedBidiClass::BidiClassID DerivedBidiClass::insertBidiClass(const string &bidiClass) {
    auto it = m_bidiClassToID.find(bidiClass);
    if (it != m_bidiClassToID.end()) {
        return it->second;
    }

    BidiClassID id = static_cast<BidiClassID>(m_idToBidiClass.size());
    m_bidiClassToID[bidiClass] = id;
    m_idToBidiClass.push_back(bidiClass);

    return id;
}

const string &DerivedBidiClass::bidiClassOf(uint32_t codePoint) const {
    return m_idToBidiClass.at(m_bidiClasses.at(codePoint));
}
