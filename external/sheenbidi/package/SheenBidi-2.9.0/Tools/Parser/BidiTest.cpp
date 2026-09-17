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
#include "BidiTest.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_BIDI_TEST = "BidiTest.txt";

static inline void initializeTestCase(BidiTest::TestCase &testCase) {
    const int DefaultLength = 96;

    testCase.types.reserve(DefaultLength);
    testCase.levels.reserve(DefaultLength);
    testCase.order.reserve(DefaultLength);
}

BidiTest::BidiTest(const string &directory) :
    DataFile(directory, FILE_BIDI_TEST)
{
    initializeTestCase(m_testCase);
}

bool BidiTest::fetchNext() {
    while (readLine(m_line)) {
        if (m_line.isEmpty() || m_line.match('#')) {
            continue;
        }

        if (m_line.match('@')) {
            if (m_line.match("Levels:", MatchRule::Trimmed)) {
                m_testCase.levels.clear();
                readLevels();
            } else if (m_line.match("Reorder:", MatchRule::Trimmed)) {
                m_testCase.order.clear();
                readOrder();
            }
        } else {
            m_testCase.directions = static_cast<ParagraphDirection>(0);
            m_testCase.types.clear();
            readData();

            return true;
        }
    }

    return false;
}

void BidiTest::readLevels() {
    while (m_line.hasMore()) {
        Level level;

        if (m_line.match("x", MatchRule::Trimmed)) {
            level = LEVEL_X;
        } else {
            level = static_cast<Level>(m_line.parseNumber());
        }

        m_testCase.levels.push_back(level);
    }
}

void BidiTest::readOrder() {
    while (m_line.hasMore()) {
        OrderIndex index = m_line.parseNumber();
        m_testCase.order.push_back(index);
    }
}

void BidiTest::readData() {
    string type;

    while (m_line.getUntil(WhitespaceOrSemicolon, type)) {
        m_testCase.types.push_back(type);

        if (m_line.match(Semicolon)) {
            break;
        }
    }

    auto directions = m_line.parseNumber();
    m_testCase.directions = static_cast<ParagraphDirection>(directions);
}

void BidiTest::reset() {
    DataFile::reset();
}
