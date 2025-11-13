/*
 * Copyright (C) 2018-2025 Muhammad Tayyab Akram
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
#include <SheenBidi/SBGeneralCategory.h>

extern "C" {
#include <Source/GeneralCategoryLookup.h>
}

#include <Parser/DerivedGeneralCategory.h>

#include "Utilities/Convert.h"
#include "Utilities/Unicode.h"

#include "Configuration.h"
#include "GeneralCategoryLookupTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Utilities;

GeneralCategoryLookupTests::GeneralCategoryLookupTests(const DerivedGeneralCategory &derivedGeneralCategory) :
    m_derivedGeneralCategory(derivedGeneralCategory)
{
}

void GeneralCategoryLookupTests::run()
{
#ifdef SB_CONFIG_UNITY
    cout << "Cannot run general category lookup tests in unity mode." << endl;
#else
    cout << "Running general category lookup tests." << endl;

    size_t failures = 0;
    string uniGeneralCategory;

    for (uint32_t codePoint = 0; codePoint <= Unicode::MAX_CODE_POINT; codePoint++) {
        const string &expGeneralCategory = m_derivedGeneralCategory.generalCategoryOf(codePoint);

        SBGeneralCategory valGeneralCategory = LookupGeneralCategory(codePoint);
        const string &genGeneralCategory = Convert::generalCategoryToString(valGeneralCategory);

        if (genGeneralCategory != expGeneralCategory) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Invalid general category found: " << endl
                     << "  Code Point: " << codePoint << endl
                     << "  Expected General Category: " << expGeneralCategory << endl
                     << "  Generated General Category: " << genGeneralCategory << endl;
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

    DerivedGeneralCategory derivedGeneralCategory(dir);

    GeneralCategoryLookupTests generalCategoryLookupTests(derivedGeneralCategory);
    generalCategoryLookupTests.run();

    return 0;
}

#endif
