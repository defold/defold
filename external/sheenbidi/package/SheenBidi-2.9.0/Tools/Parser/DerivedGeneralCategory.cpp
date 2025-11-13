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

#include <algorithm>
#include <cstdint>
#include <string>

#include "DataFile.h"
#include "DerivedGeneralCategory.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_DERIVED_GENERAL_CATEGORY = "DerivedGeneralCategory.txt";
static const string GENERAL_CATEGORY_DEFAULT = "Cn";

DerivedGeneralCategory::DerivedGeneralCategory(const string &directory) :
    DataFile(directory, FILE_DERIVED_GENERAL_CATEGORY),
    m_generalCategories(CodePointCount)
{
    // Insert the default GeneralCategoryID for all code points.
    insertGeneralCategory(GENERAL_CATEGORY_DEFAULT);

    Line line;
    string generalCategory;

    if (readLine(line)) {
        m_version = line.scanVersion();
    }

    while (readLine(line)) {
        if (line.isEmpty() || line.match('#')) {
            continue;
        }

        auto range = line.parseCodePointRange();
        line.getField(generalCategory);

        auto id = insertGeneralCategory(generalCategory);
        for (auto codePoint = range.first; codePoint <= range.second; codePoint++) {
            m_generalCategories.at(codePoint) = id;
        }

        m_lastCodePoint = max(m_lastCodePoint, range.second);
    }
}

DerivedGeneralCategory::GeneralCategoryID DerivedGeneralCategory::insertGeneralCategory(const string &generalCategory) {
    auto it = m_generalCategoryToID.find(generalCategory);
    if (it != m_generalCategoryToID.end()) {
        return it->second;
    }

    GeneralCategoryID id = static_cast<GeneralCategoryID>(m_idToGeneralCategory.size());
    m_generalCategoryToID[generalCategory] = id;
    m_idToGeneralCategory.push_back(generalCategory);

    return id;
}

const string &DerivedGeneralCategory::generalCategoryOf(uint32_t codePoint) const {
    return m_idToGeneralCategory.at(m_generalCategories.at(codePoint));
}
