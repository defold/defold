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

#ifndef SHEENBIDI_PARSER_UNICODE_DATA_H
#define SHEENBIDI_PARSER_UNICODE_DATA_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "DataFile.h"

namespace SheenBidi {
namespace Parser {

class UnicodeData : public DataFile {
public:
    UnicodeData(const std::string &directory);

    uint32_t firstCodePoint() const { return m_firstCodePoint; }
    uint32_t lastCodePoint() const { return m_lastCodePoint; }

    void getCharacterName(uint32_t codePoint, std::string &characterName) const;
    void getGeneralCategory(uint32_t codePoint, std::string &generalCategory) const;
    void getCombiningClass(uint32_t codePoint, std::string &combiningClass) const;
    void getBidirectionalCategory(uint32_t codePoint, std::string &bidirectionalCategory) const;
    void getDecompositionMapping(uint32_t codePoint, std::string &decompositionMapping) const;
    void getDecimalDigitValue(uint32_t codePoint, std::string &decimalDigitValue) const;
    void getDigitValue(uint32_t codePoint, std::string &digitValue) const;
    void getNumericValue(uint32_t codePoint, std::string &numericValue) const;
    void getMirrored(uint32_t codePoint, std::string &mirrored) const;
    void getOldName(uint32_t codePoint, std::string &oldName) const;
    void getCommentField(uint32_t codePoint, std::string &commentField) const;
    void getUppercaseMapping(uint32_t codePoint, std::string &uppercaseMapping) const;
    void getLowercaseMapping(uint32_t codePoint, std::string &lowercaseMapping) const;
    void getTitlecaseMapping(uint32_t codePoint, std::string &titlecaseMapping) const;

private:
    uint32_t m_firstCodePoint = 0;
    uint32_t m_lastCodePoint = 0;

    std::vector<std::string> m_dataLines;

    void getField(uint32_t codePoint, size_t index, std::string &field) const;
};

}
}

#endif
