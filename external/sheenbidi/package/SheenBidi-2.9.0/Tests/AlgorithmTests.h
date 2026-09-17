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

#ifndef _SHEENBIDI__ALGORITHM_TESTS_H
#define _SHEENBIDI__ALGORITHM_TESTS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <SheenBidi/SheenBidi.h>

#include <Parser/BidiTest.h>
#include <Parser/BidiCharacterTest.h>
#include <Parser/BidiMirroring.h>

namespace SheenBidi {

class AlgorithmTests {
public:
    AlgorithmTests(Parser::BidiTest *bidiTest, Parser::BidiCharacterTest *bidiBracketTest, Parser::BidiMirroring *bidiMirroring);
    ~AlgorithmTests();

    void testAlgorithm();
    void testMulticharNewline();
    void run();

private:
    Parser::BidiTest *m_bidiTest;
    Parser::BidiCharacterTest *m_bidiCharacterTest;
    Parser::BidiMirroring *m_bidiMirroring;

    size_t testCounter = 0;
    size_t failures = 0;

    SBMirrorLocatorRef m_mirrorLocator = nullptr;

    SBCodepoint m_genChars[256] = { };
    SBUInteger m_charCount = 0;

    SBLevel m_inputLevel = SBLevelInvalid;
    uint8_t m_paragraphLevel = UINT8_MAX;

    SBUInteger m_runCount = 0;
    const SBRun *m_runArray = nullptr;

    uint32_t m_genMirrors[256] = { };
    size_t m_mirrorCount = 0;

    const std::vector<uint8_t> *m_levels = nullptr;
    const std::vector<size_t> *m_order = nullptr;

    void loadCharacters(const std::vector<std::string> &types);
    void loadCharacters(const std::vector<uint32_t> &codePoints);
    void loadMirrors();

    void displayBidiTestCase() const;
    void displayBidiCharacterTestCase() const;

    bool testLevels() const;
    bool testOrder() const;
    bool testMirrors() const;

    bool conductTest();
    void analyzeBidiTest();
    void analyzeBidiCharacterTest();
};

}

#endif
