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
#include <string>

#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBBidiType.h>
#include <SheenBidi/SBConfig.h>

extern "C" {
#include <Source/BidiTypeLookup.h>
}

#include <Parser/DerivedBidiClass.h>
#include <Parser/DerivedCoreProperties.h>
#include <Parser/PropertyValueAliases.h>
#include <Parser/PropList.h>

#include "Utilities/Convert.h"
#include "Utilities/Unicode.h"

#include "Configuration.h"
#include "BidiTypeLookupTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Utilities;

BidiTypeLookupTests::BidiTypeLookupTests(const DerivedBidiClass &derivedBidiClass) :
    m_derivedBidiClass(derivedBidiClass)
{
}

void BidiTypeLookupTests::run() {
#ifdef SB_CONFIG_UNITY
    cout << "Cannot run bidi type lookup tests in unity mode." << endl;
#else
    cout << "Running bidi type lookup tests." << endl;

    size_t failures = 0;

    for (uint32_t codePoint = 0; codePoint <= Unicode::MAX_CODE_POINT; codePoint++) {
        const auto &expBidiType = m_derivedBidiClass.bidiClassOf(codePoint);

        SBBidiType valBidiType = LookupBidiType(codePoint);
        const string &genBidiType = Convert::bidiTypeToString(valBidiType);

        if (genBidiType != expBidiType) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Invalid char type found: " << endl
                     << "  Code Point: " << codePoint << endl
                     << "  Expected Char Type: " << expBidiType << endl
                     << "  Generated Char Type: " << genBidiType << endl;
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

    PropList propList(dir);
    DerivedCoreProperties derivedCoreProperties(dir);
    PropertyValueAliases propertyValueAliases(dir);
    DerivedBidiClass derivedBidiClass(dir, propList, derivedCoreProperties, propertyValueAliases);

    BidiTypeLookupTests bidiTypeLookupTests(derivedBidiClass);
    bidiTypeLookupTests.run();

    return 0;
}

#endif
