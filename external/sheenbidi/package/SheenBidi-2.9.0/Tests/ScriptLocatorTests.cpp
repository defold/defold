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
#include <string>
#include <vector>

#include <SheenBidi/SBBase.h>
#include <SheenBidi/SBCodepointSequence.h>
#include <SheenBidi/SBScript.h>
#include <SheenBidi/SBScriptLocator.h>

#include "ScriptLocatorTests.h"

using namespace std;
using namespace SheenBidi;

struct run {
    SBUInteger offset;
    SBUInteger length;
    SBScript script;

    bool operator ==(const run& other) const {
        return offset == other.offset
            && length == other.length
            && script == other.script;
    }
};

static void u32Test(const u32string string, const vector<run> runs)
{
    SBCodepointSequence sequence;
    sequence.stringEncoding = SBStringEncodingUTF32;
    sequence.stringBuffer = (void *)&string[0];
    sequence.stringLength = string.length();

    SBScriptLocatorRef locator = SBScriptLocatorCreate();
    const SBScriptAgent *agent = SBScriptLocatorGetAgent(locator);

    SBScriptLocatorLoadCodepoints(locator, &sequence);

    vector<run> output;
    while (SBScriptLocatorMoveNext(locator)) {
        output.push_back({agent->offset, agent->length, agent->script});
    }

    SBScriptLocatorRelease(locator);

    assert(runs == output);
}

ScriptLocatorTests::ScriptLocatorTests()
{
}

void ScriptLocatorTests::run()
{
    /* Test with an empty string. */
    u32Test(U"", { });

    /* Test with a single script in ascii, UTF-16 and UTF-32 ranges. */
    u32Test(U"Script", { {0, 6, SBScriptLATN} });
    u32Test(U"تحریر", { {0, 5, SBScriptARAB} });
    u32Test(U"\U0001D84C\U0001D84D\U0001D84E\U0001D84F\U0001D850\U0001D851\U0001D852",
            { {0, 7, SBScriptSGNW} });

    /* Test with different scripts. */
    u32Test(U"Scriptتحریر\U0001D84C\U0001D84D\U0001D84E\U0001D84F\U0001D850\U0001D851\U0001D852",
            { {0, 6, SBScriptLATN}, {6, 5, SBScriptARAB}, {11, 7, SBScriptSGNW} });

    /* Test with a common script among strong scripts. */
    u32Test(U"A simple line.ایک سادہ سی لائن۔", { {0, 14, SBScriptLATN}, {14, 17, SBScriptARAB} });

    /* Test with a different script in brackets. */
    u32Test(U"اس نے کہا: (Haste makes waste)۔",
            { {0, 12, SBScriptARAB}, {12, 17, SBScriptLATN}, {29, 2, SBScriptARAB} });
    /* Test with nested bracket pairs. */
    u32Test(U"اس نے کہا: (I heard: {اتفاق میں برکت ہے})۔",
            { {0, 12, SBScriptARAB}, {12, 10, SBScriptLATN}, {22, 17, SBScriptARAB},
              {39, 1, SBScriptLATN}, {40, 2, SBScriptARAB} });
    /* Test with a starting bracket pair. */
    u32Test(U"[All is well]", { {0, 13, SBScriptLATN} });
}

#ifdef STANDALONE_TESTING

int main(int argc, const char *argv[]) {
    ScriptLocatorTests scriptLocatorTests;
    scriptLocatorTests.run();

    return 0;
}

#endif
