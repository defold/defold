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

#ifndef SHEENBIDI_PARSER_BIDI_CHARACTER_TEST_H
#define SHEENBIDI_PARSER_BIDI_CHARACTER_TEST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "DataFile.h"

namespace SheenBidi {
namespace Parser {

class BidiCharacterTest : public DataFile {
public:
    using CodePoint = uint32_t;
    using Level = uint8_t;
    using OrderIndex = size_t;

    static const uint8_t LEVEL_X = UINT8_MAX;

    enum ParagraphDirection {
        LTR = 0,
        RTL = 1,
        Auto = 2,
    };

    struct TestCase {
        ParagraphDirection paragraphDirection;
        Level paragraphLevel;
        std::vector<CodePoint> text;
        std::vector<Level> levels;
        std::vector<OrderIndex> order;
    };

    BidiCharacterTest(const std::string &directory);

    const TestCase &testCase() const { return m_testCase; }

    bool fetchNext();
    void reset();

private:
    Line m_line;
    TestCase m_testCase;

    void clearTestCase();
    void readText();
    void readParagraphDirection();
    void readParagraphLevel();
    void readLevels();
    void readOrder();
};

}
}

#endif
