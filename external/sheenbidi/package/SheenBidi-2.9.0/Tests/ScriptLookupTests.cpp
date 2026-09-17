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
#include <SheenBidi/SBScript.h>

extern "C" {
#include <Source/ScriptLookup.h>
}

#include <Parser/PropertyValueAliases.h>
#include <Parser/Scripts.h>

#include "Utilities/Convert.h"
#include "Utilities/Unicode.h"

#include "Configuration.h"
#include "ScriptLookupTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Utilities;

ScriptLookupTests::ScriptLookupTests(const Scripts &scripts, const PropertyValueAliases &propertyValueAliases) :
    m_scripts(scripts),
    m_propertyValueAliases(propertyValueAliases)
{
}

void ScriptLookupTests::run() {
#ifdef SB_CONFIG_UNITY
    cout << "Cannot run script lookup tests in unity mode." << endl;
#else
    cout << "Running script lookup tests." << endl;

    size_t failures = 0;

    for (uint32_t codePoint = 0; codePoint <= Unicode::MAX_CODE_POINT; codePoint++) {
        const string &uniScript = m_scripts.scriptOf(codePoint);
        const string &expScript = m_propertyValueAliases.abbreviationForScript(uniScript);

        SBScript valScript = LookupScript(codePoint);
        const string &genScript = Convert::scriptToString(valScript);

        if (expScript != genScript) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Invalid script found: " << endl
                     << "  Code Point: " << codePoint << endl
                     << "  Expected Script: " << expScript << endl
                     << "  Generated Script: " << genScript << endl;
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

    PropertyValueAliases propertyValueAliases(dir);
    Scripts scripts(dir);

    ScriptLookupTests scriptLookupTests(scripts, propertyValueAliases);
    scriptLookupTests.run();

    return 0;
}

#endif
