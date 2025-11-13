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
#include <Source/PairingLookup.h>
}

#include <Parser/BidiMirroring.h>

#include "Utilities/Unicode.h"

#include "Configuration.h"
#include "MirrorLookupTests.h"

using namespace std;
using namespace SheenBidi;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Utilities;

MirrorLookupTests::MirrorLookupTests(const BidiMirroring &bidiMirroring) :
    m_bidiMirroring(bidiMirroring)
{
}

void MirrorLookupTests::run() {
#ifdef SB_CONFIG_UNITY
    cout << "Cannot run mirror lookup tests in unity mode." << endl;
#else
    cout << "Running mirror lookup tests." << endl;

    size_t failures = 0;

    for (uint32_t codePoint = 0; codePoint <= Unicode::MAX_CODE_POINT; codePoint++) {
        uint32_t expMirror = m_bidiMirroring.mirrorOf(codePoint);
        SBUInt32 genMirror = LookupMirror(codePoint);

        if (genMirror != expMirror) {
            if (Configuration::DISPLAY_ERROR_DETAILS) {
                cout << "Invalid mirror found: " << endl
                     << "  Code Point: " << codePoint << endl
                     << "  Generated Mirror: " << genMirror << endl
                     << "  Expected Mirror: " << expMirror << endl;
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

    BidiMirroring bidiMirroring(dir);

    MirrorLookupTests mirrorLookupTests(bidiMirroring);
    mirrorLookupTests.run();

    return 0;
}

#endif
