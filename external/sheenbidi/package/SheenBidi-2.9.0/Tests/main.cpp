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

#include <iostream>

#include <SheenBidi/SheenBidi.h>

#include <Parser/BidiBrackets.h>
#include <Parser/BidiCharacterTest.h>
#include <Parser/BidiMirroring.h>
#include <Parser/BidiTest.h>
#include <Parser/DerivedBidiClass.h>
#include <Parser/DerivedCoreProperties.h>
#include <Parser/DerivedGeneralCategory.h>
#include <Parser/PropertyValueAliases.h>
#include <Parser/PropList.h>
#include <Parser/Scripts.h>
#include <Parser/UnicodeData.h>

#include "AlgorithmTests.h"
#include "BidiTypeLookupTests.h"
#include "BracketLookupTests.h"
#include "CodepointSequenceTests.h"
#include "CodepointTests.h"
#include "GeneralCategoryLookupTests.h"
#include "MirrorLookupTests.h"
#include "RunQueueTests.h"
#include "ScriptLocatorTests.h"
#include "ScriptLookupTests.h"
#include "ScriptTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;

int main(int argc, const char *argv[]) {
    const char *dir = argv[1];

    BidiMirroring bidiMirroring(dir);
    BidiBrackets bidiBrackets(dir);
    BidiTest bidiTest(dir);
    BidiCharacterTest bidiCharacterTest(dir);
    PropList propList(dir);
    DerivedCoreProperties derivedCoreProperties(dir);
    PropertyValueAliases propertyValueAliases(dir);
    DerivedBidiClass derivedBidiClass(dir, propList, derivedCoreProperties, propertyValueAliases);
    DerivedGeneralCategory derivedGeneralCategory(dir);
    Scripts scripts(dir);
    UnicodeData unicodeData(dir);

    BidiTypeLookupTests bidiTypeLookupTests(derivedBidiClass);
    MirrorLookupTests mirrorLookupTests(bidiMirroring);
    BracketLookupTests bracketLookupTests(bidiBrackets);
    GeneralCategoryLookupTests generalCategoryLookupTests(derivedGeneralCategory);
    ScriptLookupTests scriptLookupTests(scripts, propertyValueAliases);
    AlgorithmTests algorithmTests(&bidiTest, &bidiCharacterTest, &bidiMirroring);
    CodepointTests codepointTests(unicodeData, bidiBrackets);
    CodepointSequenceTests codepointSequenceTests;
    RunQueueTests runQueueTests;
    ScriptLocatorTests scriptLocatorTests;
    ScriptTests scriptTests;

    cout << "Testing SheenBidi " << SBVersionGetString() << endl;
    cout << endl;

    bidiTypeLookupTests.run();
    mirrorLookupTests.run();
    bracketLookupTests.run();
    generalCategoryLookupTests.run();
    scriptLookupTests.run();
    algorithmTests.run();
    codepointTests.run();
    codepointSequenceTests.run();
    runQueueTests.run();
    scriptLocatorTests.run();
    scriptTests.run();

    cout << "Finished." << endl;

    return 0;
}
