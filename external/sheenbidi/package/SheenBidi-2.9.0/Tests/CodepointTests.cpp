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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

#include <SheenBidi/SBConfig.h>

extern "C" {
#include <Source/SBCodepoint.h>
}

#include "Utilities/Unicode.h"

#include "Configuration.h"
#include "CodepointTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Utilities;

CodepointTests::CodepointTests(const UnicodeData &unicodeData, const BidiBrackets &bidiBrackets) :
    m_unicodeData(unicodeData),
    m_bidiBrackets(bidiBrackets)
{
}

void CodepointTests::run() {
    testIsCanonicalEquivalentBracket();
}

void CodepointTests::testIsCanonicalEquivalentBracket() {
#ifdef SB_CONFIG_UNITY
    cout << "Cannot run canonical equivalent bracket tests in unity mode." << endl;
#else
    cout << "Running canonical equivalent bracket tests." << endl;

    size_t failures = 0;
    string mapping;

    for (uint32_t codePoint = 0; codePoint <= Unicode::MAX_CODE_POINT; codePoint++) {
        // Skip if it's not a bracket.
        if (m_bidiBrackets.pairedBracketOf(codePoint) == 0) {
            continue;
        }

        if (!SBCodepointIsCanonicalEquivalentBracket(codePoint, codePoint)) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Matching with self (bracket) failed:" << endl
                     << "  Code Point: " << hex << codePoint << dec << endl;
            }

            failures += 1;
            continue;
        }

        m_unicodeData.getDecompositionMapping(codePoint, mapping);
        if (mapping.empty() || mapping[0] == '<') {
            continue;
        }

        uint32_t canonical = stol(mapping, nullptr, 16);

        if (!SBCodepointIsCanonicalEquivalentBracket(codePoint, canonical)) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Matching with canonical equivalent bracket failed:" << endl
                     << "  Bracket: " << hex << codePoint << dec << endl
                     << "  Canonical: " << hex << canonical << dec << endl;
            }

            failures += 1;
        }
    }

    cout << failures << " error/s." << endl;
    cout << endl;

    assert(failures == 0);
#endif
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    const char *dir = argv[1];

    UnicodeData unicodeData(dir);
    BidiBrackets bidiBrackets(dir);

    CodepointTests codepointTests(unicodeData, bidiBrackets);
    codepointTests.run();

    return 0;
}

#endif
