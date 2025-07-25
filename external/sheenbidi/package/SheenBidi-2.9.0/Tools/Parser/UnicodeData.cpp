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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "DataFile.h"
#include "UnicodeData.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_UNICODE_DATA = "UnicodeData.txt";

static bool startsWith(const string &text, const string &matchString) {
    if (text.length() >= matchString.length()) {
        return text.compare(0, matchString.length(), matchString) == 0;
    }

    return false;
}

static bool endsWith(const string &text, const string &matchString) {
    if (text.length() >= matchString.length()) {
        return text.compare(text.length() - matchString.length(), matchString.length(), matchString) == 0;
    }

    return false;
}

UnicodeData::UnicodeData(const string &directory) :
    DataFile(directory, FILE_UNICODE_DATA),
    m_dataLines(CodePointCount)
{
    uint32_t startCodePoint = 0;

    Line line;
    string characterName;

    while (readLine(line)) {
        auto codePoint = line.parseSingleCodePoint();
        line.getField(characterName);

        m_dataLines.at(codePoint) = line.data();

        if (startsWith(characterName, "<")) {
            if (endsWith(characterName, ", First>")) {
                startCodePoint = codePoint;
            } else if (endsWith(characterName, ", Last>")) {
                while (++startCodePoint < codePoint) {
                    m_dataLines[codePoint] = line.data();
                }
            }
        }

        m_lastCodePoint = max(m_lastCodePoint, codePoint);
    }
}

void UnicodeData::getField(uint32_t codePoint, size_t index, std::string &field) const {
    Line line(m_dataLines.at(codePoint));

    for (size_t skip = 0; skip < index; ++skip) {
        line.skip(SkipMode::Field);
    }

    line.getField(field);
}

void UnicodeData::getCharacterName(uint32_t codePoint, string &characterName) const {
    getField(codePoint, 1, characterName);
}

void UnicodeData::getGeneralCategory(uint32_t codePoint, string &generalCategory) const {
    getField(codePoint, 2, generalCategory);
}

void UnicodeData::getCombiningClass(uint32_t codePoint, string &combiningClass) const {
    getField(codePoint, 3, combiningClass);
}

void UnicodeData::getBidirectionalCategory(uint32_t codePoint, string &bidirectionalCategory) const {
    getField(codePoint, 4, bidirectionalCategory);
}

void UnicodeData::getDecompositionMapping(uint32_t codePoint, string &decompositionMapping) const {
    getField(codePoint, 5, decompositionMapping);
}

void UnicodeData::getDecimalDigitValue(uint32_t codePoint, string &decimalDigitValue) const {
    getField(codePoint, 6, decimalDigitValue);
}

void UnicodeData::getDigitValue(uint32_t codePoint, string &digitValue) const {
    getField(codePoint, 7, digitValue);
}

void UnicodeData::getNumericValue(uint32_t codePoint, string &numericValue) const {
    getField(codePoint, 8, numericValue);
}

void UnicodeData::getMirrored(uint32_t codePoint, string &mirrored) const {
    getField(codePoint, 9, mirrored);
}

void UnicodeData::getOldName(uint32_t codePoint, string &oldName) const {
    getField(codePoint, 10, oldName);
}

void UnicodeData::getCommentField(uint32_t codePoint, string &commentField) const {
    getField(codePoint, 11, commentField);
}

void UnicodeData::getUppercaseMapping(uint32_t codePoint, string &uppercaseMapping) const {
    getField(codePoint, 12, uppercaseMapping);
}

void UnicodeData::getLowercaseMapping(uint32_t codePoint, string &lowercaseMapping) const {
    getField(codePoint, 13, lowercaseMapping);
}

void UnicodeData::getTitlecaseMapping(uint32_t codePoint, string &titlecaseMapping) const {
    getField(codePoint, 14, titlecaseMapping);
}
