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

#include <string>

#include "DataFile.h"
#include "PropertyValueAliases.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_PROPERTY_VALUE_ALIASES = "PropertyValueAliases.txt";
static const string PROPERTY_CANONICAL_COMBINING_CLASS = "ccc";
static const string PROPERTY_BIDI_CLASS = "bc";
static const string PROPERTY_SCRIPT = "sc";

PropertyValueAliases::PropertyValueAliases(const string &directory) :
    DataFile(directory, FILE_PROPERTY_VALUE_ALIASES)
{
    Line line;
    string property;
    string numeric;
    string shortName;
    string longName;

    if (readLine(line)) {
        m_version = line.scanVersion();
    }

    while (readLine(line)) {
        if (line.isEmpty() || line.match('#')) {
            continue;
        }

        line.getField(property);

        if (property != PROPERTY_CANONICAL_COMBINING_CLASS) {
            line.getField(shortName);
            line.getField(longName);
        } else {
            line.getField(numeric);
            line.getField(shortName);
            line.getField(longName);
        }

        if (property == PROPERTY_BIDI_CLASS) {
            m_bidiClassMap[longName] = shortName;
        } else if (property == PROPERTY_SCRIPT) {
            m_scriptMap[longName] = shortName;
        }
    }
}

const string &PropertyValueAliases::abbreviationForBidiClass(const string &bidiClass) const {
    return m_bidiClassMap.at(bidiClass);
}

const string &PropertyValueAliases::abbreviationForScript(const string &scriptName) const {
    return m_scriptMap.at(scriptName);
}
