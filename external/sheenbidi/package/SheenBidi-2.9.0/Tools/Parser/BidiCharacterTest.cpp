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

#include <string>

#include "DataFile.h"
#include "BidiCharacterTest.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_BIDI_CHARACTER_TEST = "BidiCharacterTest.txt";

static inline void initializeTestCase(BidiCharacterTest::TestCase &testCase) {
    const int DefaultLength = 32;

    testCase.text.reserve(DefaultLength);
    testCase.levels.reserve(DefaultLength);
    testCase.order.reserve(DefaultLength);
}

BidiCharacterTest::BidiCharacterTest(const string &directory) :
    DataFile(directory, FILE_BIDI_CHARACTER_TEST)
{
    initializeTestCase(m_testCase);
}

bool BidiCharacterTest::fetchNext() {
    clearTestCase();

    while (readLine(m_line)) {
        if (m_line.isEmpty() || m_line.match('#')) {
            continue;
        }

        readText();
        readParagraphDirection();
        readParagraphLevel();
        readLevels();
        readOrder();

        return true;
    }

    return false;
}

void BidiCharacterTest::clearTestCase() {
    m_testCase.paragraphDirection = ParagraphDirection::LTR;
    m_testCase.paragraphLevel = 0;
    m_testCase.text.clear();
    m_testCase.levels.clear();
    m_testCase.order.clear();
}

void BidiCharacterTest::readText() {
    while (!m_line.match(';')) {
        CodePoint codePoint = m_line.parseNumber(16);
        m_testCase.text.push_back(codePoint);
    }
}

void BidiCharacterTest::readParagraphDirection() {
    auto paragraphDirection = m_line.parseNumber();
    m_line.skip(SkipMode::Field);

    m_testCase.paragraphDirection = static_cast<ParagraphDirection>(paragraphDirection);
}

void BidiCharacterTest::readParagraphLevel() {
    auto paragraphLevel = m_line.parseNumber();
    m_line.skip(SkipMode::Field);

    m_testCase.paragraphLevel = static_cast<Level>(paragraphLevel);
}

void BidiCharacterTest::readLevels() {
    while (!m_line.match(';')) {
        Level level;

        if (m_line.match("x", MatchRule::Trimmed)) {
            level = LEVEL_X;
        } else {
            level = static_cast<Level>(m_line.parseNumber());
        }

        m_testCase.levels.push_back(level);
    }
}

void BidiCharacterTest::readOrder() {
    while (m_line.hasMore()) {
        OrderIndex index = m_line.parseNumber();
        m_testCase.order.push_back(index);
    }
}

void BidiCharacterTest::reset() {
    DataFile::reset();
}
