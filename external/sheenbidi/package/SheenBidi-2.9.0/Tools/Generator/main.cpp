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

#include <cstdio>
#include <iostream>
#include <string>

#include <Parser/BidiBrackets.h>
#include <Parser/BidiMirroring.h>
#include <Parser/DerivedBidiClass.h>
#include <Parser/DerivedCoreProperties.h>
#include <Parser/DerivedGeneralCategory.h>
#include <Parser/PropertyValueAliases.h>
#include <Parser/PropList.h>
#include <Parser/Scripts.h>
#include <Parser/UnicodeVersion.h>

#include "BidiTypeLookupGenerator.h"
#include "GeneralCategoryLookupGenerator.h"
#include "PairingLookupGenerator.h"
#include "ScriptLookupGenerator.h"

using namespace std;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Generator;

int main(int argc, const char * argv[])
{
    if (argc > 3) {
        cerr << "Usage: " << argv[0] << " [input_dir] [output_dir]" << endl;
        return 64;
    }
    const string in = argc >= 2 ? argv[1] : "Tools/Unicode";
    const string out = argc == 3 ? argv[2] : "Source";

    BidiMirroring bidiMirroring(in);
    BidiBrackets bidiBrackets(in);
    PropList propList(in);
    DerivedCoreProperties derivedCoreProperties(in);
    PropertyValueAliases propertyValueAliases(in);
    DerivedBidiClass derivedBidiClass(in, propList, derivedCoreProperties, propertyValueAliases);
    DerivedGeneralCategory derivedGeneralCategory(in);
    Scripts scripts(in);

    cout << "Generating files." << endl;

    BidiTypeLookupGenerator bidiTypeLookup(derivedBidiClass);
    bidiTypeLookup.setMainSegmentSize(16);
    bidiTypeLookup.setBranchSegmentSize(64);
    bidiTypeLookup.generateFile(out);

    PairingLookupGenerator pairingLookup(bidiMirroring, bidiBrackets);
    pairingLookup.setSegmentSize(106);
    pairingLookup.generateFile(out);

    GeneralCategoryLookupGenerator generalCategoryLookup(derivedGeneralCategory);
    generalCategoryLookup.setMainSegmentSize(16);
    generalCategoryLookup.setBranchSegmentSize(64);
    generalCategoryLookup.generateFile(out);

    ScriptLookupGenerator scriptLookup(scripts, propertyValueAliases);
    scriptLookup.setMainSegmentSize(16);
    scriptLookup.setBranchSegmentSize(32);
    scriptLookup.generateFile(out);

    cout << "Finished.";

    getchar();

    return 0;
}
