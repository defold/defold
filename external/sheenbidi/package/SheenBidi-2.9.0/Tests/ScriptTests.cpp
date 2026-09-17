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

#include <algorithm>
#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBScript.h>

#include "Utilities/Convert.h"

#include "ScriptTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Utilities;

ScriptTests::ScriptTests()
{
}

void ScriptTests::run() {
    testGetOpenTypeTag();
    testGetUnicodeTag();
}

void ScriptTests::testGetOpenTypeTag() {
    const string defaultTag = "DFLT";
    const unordered_map<string, string> replacementTags = {
        {"zinh", defaultTag},
        {"zyyy", defaultTag},
        {"zzzz", defaultTag},
        {"beng", "bng2"},
        {"deva", "dev2"},
        {"gujr", "gjr2"},
        {"guru", "gur2"},
        {"hira", "kana"},
        {"knda", "knd2"},
        {"laoo", "lao "},
        {"mlym", "mlm2"},
        {"orya", "ory2"},
        {"taml", "tml2"},
        {"telu", "tel2"},
        {"mymr", "mym2"},
        {"yiii", "yi  "},
        {"nkoo", "nko "},
        {"vaii", "vai "}
    };

    for (size_t script = 0; script <= UINT8_MAX; script++) {
        auto &alias = Convert::scriptToString(script);
        auto match = alias;
        Convert::toLowercase(match);

        if (alias.empty()) {
            match = defaultTag;
        } else {
            auto it = replacementTags.find(match);
            if (it != replacementTags.end()) {
                match = it->second;
            }
        }

        auto tag = SBScriptGetOpenTypeTag(script);
        auto expected = Convert::stringToTag(match);

        assert(tag == expected);
    }
}

void ScriptTests::testGetUnicodeTag() {
    for (size_t script = 0; script <= UINT8_MAX; script++) {
        auto &alias = Convert::scriptToString(script);

        auto tag = SBScriptGetUnicodeTag(script);
        auto expected = (alias.empty() ? 0 : SBScriptGetUnicodeTag(script));

        assert(tag == expected);
    }
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    ScriptTests scriptTests;
    scriptTests.run();

    return 0;
}

#endif
