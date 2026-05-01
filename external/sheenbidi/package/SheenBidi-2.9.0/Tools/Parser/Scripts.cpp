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

#include <algorithm>
#include <cstdint>
#include <string>

#include "DataFile.h"
#include "Scripts.h"

using namespace std;
using namespace SheenBidi::Parser;

static const string FILE_SCRIPTS = "Scripts.txt";
static const string SCRIPT_UNKNOWN = "Unknown";
static const string SCRIPT_COMMON = "Common";
static const string SCRIPT_INHERITED = "Inherited";

Scripts::Scripts(const string &directory) :
    DataFile(directory, FILE_SCRIPTS),
    m_scripts(CodePointCount)
{
    insertScript(SCRIPT_UNKNOWN);
    insertScript(SCRIPT_COMMON);
    insertScript(SCRIPT_INHERITED);

    Line line;
    string script;

    if (readLine(line)) {
        m_version = line.scanVersion();
    }

    while (readLine(line)) {
        if (line.isEmpty() || line.match('#')) {
            continue;
        }

        auto range = line.parseCodePointRange();
        line.getField(script);

        auto id = insertScript(script);
        for (auto codePoint = range.first; codePoint <= range.second; codePoint++) {
            m_scripts.at(codePoint) = id;
        }

        m_lastCodePoint = max(m_lastCodePoint, range.second);
    }
}

Scripts::ScriptID Scripts::insertScript(const string &script) {
    auto it = m_scriptToID.find(script);
    if (it != m_scriptToID.end()) {
        return it->second;
    }

    ScriptID id = static_cast<ScriptID>(m_idToScript.size());
    m_scriptToID[script] = id;
    m_idToScript.push_back(script);

    return id;
}

const string &Scripts::scriptOf(uint32_t codePoint) const {
    return m_idToScript.at(m_scripts.at(codePoint));
}
