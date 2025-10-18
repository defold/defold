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
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "Utilities/ArrayBuilder.h"
#include "Utilities/FileBuilder.h"
#include "Utilities/Converter.h"
#include "Utilities/Math.h"

#include "ScriptLookupGenerator.h"

using namespace std;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Generator;
using namespace SheenBidi::Generator::Utilities;

static const size_t MIN_MAIN_SEGMENT_SIZE = 8;
static const size_t MAX_MAIN_SEGMENT_SIZE = 512;

static const size_t MIN_BRANCH_SEGMENT_SIZE = 8;
static const size_t MAX_BRANCH_SEGMENT_SIZE = 512;

static const string DATA_ARRAY_TYPE = "static const SBUInt8";
static const string DATA_ARRAY_NAME = "PrimaryScriptData";

static const string MAIN_INDEXES_ARRAY_TYPE = "static const SBUInt16";
static const string MAIN_INDEXES_ARRAY_NAME = "MainScriptIndexes";

static const string BRANCH_INDEXES_ARRAY_TYPE = "static const SBUInt16";
static const string BRANCH_INDEXES_ARRAY_NAME = "BranchScriptIndexes";

ScriptLookupGenerator::MainDataSegment::MainDataSegment(size_t index, MainDataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string ScriptLookupGenerator::MainDataSegment::hintLine() const {
    return ("/* DATA_BLOCK: -- 0x" + Converter::toHex(index, 4) + "..0x" + Converter::toHex(index + dataset->size() - 1, 4) + " -- */");
}

ScriptLookupGenerator::BranchDataSegment::BranchDataSegment(size_t index, BranchDataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string ScriptLookupGenerator::BranchDataSegment::hintLine() const {
    return ("/* INDEX_BLOCK: -- 0x" + Converter::toHex(index, 4) + "..0x" + Converter::toHex(index + dataset->size() - 1, 4) + " -- */");
}

ScriptLookupGenerator::ScriptLookupGenerator(const Scripts &scripts, const PropertyValueAliases &propertyValueAliases) :
    m_scripts(scripts),
    m_propertyValueAliases(propertyValueAliases)
{
}

void ScriptLookupGenerator::setMainSegmentSize(size_t segmentSize) {
    m_mainSegmentSize = min(MAX_MAIN_SEGMENT_SIZE, max(MIN_MAIN_SEGMENT_SIZE, segmentSize));
}

void ScriptLookupGenerator::setBranchSegmentSize(size_t segmentSize) {
    m_branchSegmentSize = min(MAX_BRANCH_SEGMENT_SIZE, max(MIN_BRANCH_SEGMENT_SIZE, segmentSize));
}

void ScriptLookupGenerator::displayScriptsFrequency() {
    map<string, size_t> frequency;

    auto last = m_scripts.lastCodePoint();
    for (uint32_t codePoint = 0; codePoint < last; codePoint++) {
        frequency[m_scripts.scriptOf(codePoint)] += 1;
    }

    for (const auto &element : frequency) {
        cout << element.first << " Code Points: " << element.second << endl;
    }
}

void ScriptLookupGenerator::analyzeData() {
    cout << "Analyzing data for script lookup." << endl;

    size_t minMemory = SIZE_MAX;
    size_t mainSegmentSize = 0;
    size_t branchSegmentSize = 0;

    m_mainSegmentSize = MIN_MAIN_SEGMENT_SIZE;
    while (m_mainSegmentSize <= MAX_MAIN_SEGMENT_SIZE) {
        collectMainData();

        m_branchSegmentSize = MIN_BRANCH_SEGMENT_SIZE;
        while (m_branchSegmentSize <= MAX_BRANCH_SEGMENT_SIZE) {
            collectBranchData();

            size_t memory = m_dataSize + ((m_mainIndexesSize + m_branchIndexesSize) * 2);
            if (memory < minMemory) {
                mainSegmentSize = m_mainSegmentSize;
                branchSegmentSize = m_branchSegmentSize;
                minMemory = memory;
            }

            m_branchSegmentSize++;
        }

        m_mainSegmentSize++;
    }

    m_mainSegmentSize = mainSegmentSize;
    m_branchSegmentSize = branchSegmentSize;

    cout << "  Main Segment Size: " << mainSegmentSize << endl;
    cout << "  Branch Segment Size: " << branchSegmentSize << endl;
    cout << "  Required Memory: " << minMemory << " bytes";
}

void ScriptLookupGenerator::collectMainData() {
    auto firstCodePoint = m_scripts.firstCodePoint();
    auto lastCodePoint = m_scripts.lastCodePoint();
    auto unicodeCount = lastCodePoint - firstCodePoint + 1;
    size_t maxSegments = Math::FastCeil(unicodeCount, m_mainSegmentSize);

    m_dataSegments.clear();
    m_dataSegments.reserve(maxSegments);

    m_dataReferences.clear();
    m_dataReferences.reserve(maxSegments);

    m_dataSize = 0;

    for (size_t i = 0; i < maxSegments; i++) {
        uint32_t segmentStart = firstCodePoint + (uint32_t)(i * m_mainSegmentSize);
        uint32_t segmentEnd = min(lastCodePoint, (uint32_t)(segmentStart + m_mainSegmentSize - 1));

        MainDataSet dataset(new UnsafeMainDataSet());
        dataset->reserve(m_mainSegmentSize);

        for (uint32_t codePoint = segmentStart; codePoint <= segmentEnd; codePoint++) {
            dataset->push_back(&m_scripts.scriptOf(codePoint));
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_dataSegments.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_dataSegments[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_dataSegments.size();
            m_dataSegments.push_back(MainDataSegment(m_dataSize, dataset));
            m_dataSize += dataset->size();
        }

        m_dataReferences.push_back(&m_dataSegments.at(segmentIndex));
    }

    m_mainIndexesSize = m_dataReferences.size();
}

void ScriptLookupGenerator::collectBranchData() {
    size_t mainIndexesCount = m_dataReferences.size() - 1;
    size_t maxSegments = Math::FastCeil(mainIndexesCount, m_branchSegmentSize);

    m_branchSegments.clear();
    m_branchSegments.reserve(maxSegments);

    m_branchReferences.clear();
    m_branchReferences.reserve(maxSegments);

    m_mainIndexesSize = 0;

    for (size_t i = 0; i < maxSegments; i++) {
        size_t segmentStart = (i * m_branchSegmentSize);
        size_t segmentEnd = min(mainIndexesCount, segmentStart + m_branchSegmentSize - 1);

        BranchDataSet dataset(new UnsafeBranchDataSet());
        dataset->reserve(m_branchSegmentSize);

        for (size_t i = segmentStart; i <= segmentEnd; i++) {
            dataset->push_back(m_dataReferences.at(i));
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_branchSegments.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_branchSegments[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_branchSegments.size();
            m_branchSegments.push_back(BranchDataSegment(m_mainIndexesSize, dataset));
            m_mainIndexesSize += dataset->size();
        }

        m_branchReferences.push_back(&m_branchSegments.at(segmentIndex));
    }
    
    m_branchIndexesSize = m_branchReferences.size();
}

void ScriptLookupGenerator::generateFile(const std::string &directory) {
    collectMainData();
    collectBranchData();

    set<string> scriptAliases;

    ArrayBuilder arrData;
    arrData.setDataType(DATA_ARRAY_TYPE);
    arrData.setName(DATA_ARRAY_NAME);
    arrData.setElementSpace(3);
    arrData.setSizeDescriptor(to_string(m_dataSize));

    for (const auto &segment : m_dataSegments) {
        bool isLast = (&segment == &m_dataSegments.back());

        arrData.append(segment.hintLine());
        arrData.newLine();

        size_t length = segment.dataset->size();

        for (size_t j = 0; j < length; j++) {
            const auto &script = *segment.dataset->at(j);
            const auto &alias = m_propertyValueAliases.abbreviationForScript(script);
            scriptAliases.insert(alias);
            arrData.appendElement(alias);

            if (!isLast || j != (length - 1)) {
                arrData.newElement();
            }
        }

        if (!isLast) {
            arrData.newLine();
        }
    }

    ArrayBuilder arrMainIndexes;
    arrMainIndexes.setDataType(MAIN_INDEXES_ARRAY_TYPE);
    arrMainIndexes.setName(MAIN_INDEXES_ARRAY_NAME);
    arrMainIndexes.setSizeDescriptor(to_string(m_mainIndexesSize));

    for (const auto &segment : m_branchSegments) {
        bool isLast = (&segment == &m_branchSegments.back());

        arrMainIndexes.append(segment.hintLine());
        arrMainIndexes.newLine();

        size_t length = segment.dataset->size();

        for (size_t j = 0; j < length; j++) {
            string element = "0x" + Converter::toHex(segment.dataset->at(j)->index, 4);
            arrMainIndexes.appendElement(element);

            if (!isLast || j != (length - 1)) {
                arrMainIndexes.newElement();
            }
        }

        if (!isLast) {
            arrMainIndexes.newLine();
        }
    }

    ArrayBuilder arrBranchIndexes;
    arrBranchIndexes.setDataType(BRANCH_INDEXES_ARRAY_TYPE);
    arrBranchIndexes.setName(BRANCH_INDEXES_ARRAY_NAME);
    arrBranchIndexes.setSizeDescriptor(to_string(m_branchIndexesSize));

    for (const auto &branchReference : m_branchReferences) {
        const auto &segment = *branchReference;
        bool isLast = (&branchReference == &m_branchReferences.back());
        string element = "0x" + Converter::toHex(segment.index, 4);

        arrBranchIndexes.appendElement(element);
        if (!isLast) {
            arrBranchIndexes.newElement();
        }
    }

    TextBuilder lookupFunction;
    string maxUnicode = "0x" + Converter::toHex(m_scripts.lastCodePoint(), 6);
    string mainDivider = "0x" + Converter::toHex(m_mainSegmentSize, 4);
    string branchDivider = "0x" + Converter::toHex(m_mainSegmentSize * m_branchSegmentSize, 4);
    lookupFunction.append("SB_INTERNAL SBScript LookupScript(SBCodepoint codepoint)").newLine();
    lookupFunction.append("{").newLine();
    lookupFunction.appendTabs(1).append("if (codepoint <= " + maxUnicode + ") {").newLine();
    lookupFunction.appendTabs(2).append("return PrimaryScriptData[").newLine();
    lookupFunction.appendTabs(2).append("        MainScriptIndexes[").newLine();
    lookupFunction.appendTabs(2).append("         BranchScriptIndexes[").newLine();
    lookupFunction.appendTabs(2).append("              codepoint / " + branchDivider).newLine();
    lookupFunction.appendTabs(2).append("         ] + (codepoint % " + branchDivider + ") / " + mainDivider).newLine();
    lookupFunction.appendTabs(2).append("        ] + codepoint % " + mainDivider).newLine();
    lookupFunction.appendTabs(2).append("       ];").newLine();
    lookupFunction.appendTabs(1).append("}").newLine();
    lookupFunction.newLine();
    lookupFunction.appendTabs(1).append("return Zzzz;").newLine();
    lookupFunction.append("}").newLine();

    FileBuilder header(directory + "/ScriptLookup.h");
    header.append("/*").newLine();
    header.append(" * Automatically generated by SheenBidiGenerator tool.").newLine();
    header.append(" * DO NOT EDIT!!").newLine();
    header.append(" */").newLine();
    header.newLine();
    header.append("#ifndef _SB_INTERNAL_SCRIPT_LOOKUP_H").newLine();
    header.append("#define _SB_INTERNAL_SCRIPT_LOOKUP_H").newLine();
    header.newLine();
    header.append("#include <SheenBidi/SBBase.h>").newLine();
    header.append("#include <SheenBidi/SBCodepoint.h>").newLine();
    header.append("#include <SheenBidi/SBConfig.h>").newLine();
    header.append("#include <SheenBidi/SBScript.h>").newLine();
    header.newLine();
    header.append("SB_INTERNAL SBScript LookupScript(SBCodepoint codepoint);").newLine();
    header.newLine();
    header.append("#endif").newLine();

    FileBuilder source(directory + "/ScriptLookup.c");
    source.append("/*").newLine();
    source.append(" * Automatically generated by SheenBidiGenerator tool.").newLine();
    source.append(" * DO NOT EDIT!!").newLine();
    source.append(" *").newLine();
    source.append(" * REQUIRED MEMORY: " + to_string(m_dataSize)
                  + "+(" + to_string(m_mainIndexesSize) + "*2)+("
                  + to_string(m_branchIndexesSize) + "*2) = "
                  + to_string((m_dataSize + m_mainIndexesSize*2 + m_branchIndexesSize*2))
                  + " Bytes").newLine();
    source.append(" */").newLine();
    source.newLine();
    source.append("#include \"ScriptLookup.h\"").newLine();
    source.newLine();
    for (auto sa : scriptAliases) {
        string str = sa + string(4 - sa.length(), ' ');
        string upper = str;
        Converter::toUpper(upper);

        source.append("#define " + str).appendTab().append(" SBScript" + upper).newLine();
    }
    source.newLine();
    source.append(arrData).newLine();
    source.append(arrMainIndexes).newLine();
    source.append(arrBranchIndexes).newLine();
    source.append(lookupFunction).newLine();

    for (auto sa : scriptAliases) {
        source.append("#undef " + sa).newLine();
    }
}
