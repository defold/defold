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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBConfig.h>

extern "C" {
#include <Source/BracketType.h>
#include <Source/PairingLookup.h>
}

#include <Parser/BidiBrackets.h>

#include "Utilities/Unicode.h"

#include "Configuration.h"
#include "BracketLookupTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Utilities;

BracketLookupTests::BracketLookupTests(const BidiBrackets &bidiBrackets) :
    m_bidiBrackets(bidiBrackets)
{
}

void BracketLookupTests::run() {
#ifdef SB_CONFIG_UNITY
    cout << "Cannot run bracket lookup tests in unity mode." << endl;
#else
    cout << "Running bracket lookup tests." << endl;

    size_t failures = 0;

    for (uint32_t codePoint = 0; codePoint <= Unicode::MAX_CODE_POINT; codePoint++) {
        uint32_t expBracket = m_bidiBrackets.pairedBracketOf(codePoint);
        char expType = m_bidiBrackets.pairedBracketTypeOf(codePoint);

        BracketType valType;
        SBUInt32 genBracket = LookupBracketPair(codePoint, &valType);

        char genType;
        switch (valType) {
        case BracketTypeOpen:
            genType = 'o';
            break;

        case BracketTypeClose:
            genType = 'c';
            break;

        default:
            genType = '\0';
            break;
        }

        if (genBracket != expBracket || genType != expType) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Invalid bracket pair found: " << endl
                     << "  Code Point: " << codePoint << endl
                     << "  Expected Bracket: " << expBracket << endl
                     << "  Expected Type: " << expBracket << endl
                     << "  Generated Bracket: " << genBracket << endl
                     << "  Generated Type: " << genType << endl;
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

    BidiBrackets bidiBrackets(dir);

    BracketLookupTests bracketLookupTests(bidiBrackets);
    bracketLookupTests.run();

    return 0;
}

#endif
