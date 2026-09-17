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

#include <algorithm>
#include <cstdint>
#include <string>

#include "DataFile.h"
#include "BidiMirroring.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_BIDI_MIRRORING = "BidiMirroring.txt";

BidiMirroring::BidiMirroring(const string &directory) :
    DataFile(directory, FILE_BIDI_MIRRORING),
    m_mirrors(CodePointCount)
{
    Line line;
    string type;

    if (readLine(line)) {
        m_version = line.scanVersion();
    }

    while (readLine(line)) {
        if (line.isEmpty() || line.match('#')) {
            continue;
        }

        uint32_t codePoint = line.parseSingleCodePoint();
        uint32_t mirror = line.parseSingleCodePoint();
        line.getField(type);

        m_mirrors.at(codePoint) = mirror;

        m_firstCodePoint = min(m_firstCodePoint, codePoint);
        m_lastCodePoint = max(m_lastCodePoint, codePoint);
    }
}

uint32_t BidiMirroring::mirrorOf(uint32_t codePoint) const {
    return m_mirrors.at(codePoint);
}
