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

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

extern "C" {
#include <Source/BracketType.h>
}

#include "Utilities/Math.h"
#include "Utilities/Converter.h"
#include "Utilities/ArrayBuilder.h"
#include "Utilities/FileBuilder.h"

#include "PairingLookupGenerator.h"

using namespace std;
using namespace SheenBidi::Parser;
using namespace SheenBidi::Generator;
using namespace SheenBidi::Generator::Utilities;

static const size_t MIN_SEGMENT_SIZE = 8;
static const size_t MAX_SEGMENT_SIZE = 512;

static const string DIFFERENCES_ARRAY_TYPE = "static const SBInt16";
static const string DIFFERENCES_ARRAY_NAME = "PairDifferences";

static const string DATA_ARRAY_TYPE = "static const SBUInt8";
static const string DATA_ARRAY_NAME = "PairData";

static const string INDEXES_ARRAY_TYPE = "static const SBUInt16";
static const string INDEXES_ARRAY_NAME = "PairIndexes";

PairingLookupGenerator::DataSegment::DataSegment(size_t index, DataSet dataset)
    : index(index)
    , dataset(dataset)
{
}

const string PairingLookupGenerator::DataSegment::hintLine() const {
    return ("/* DATA_BLOCK: -- 0x" + Converter::toHex(index, 4) + "..0x" + Converter::toHex(index + dataset->size() - 1, 4) + " -- */");
}

PairingLookupGenerator::PairingLookupGenerator(const BidiMirroring& bidiMirroring, const BidiBrackets& bidiBrackets)
    : m_bidiMirroring(bidiMirroring)
    , m_bidiBrackets(bidiBrackets)
    , m_firstCodePoint(0)
    , m_lastCodePoint(max(bidiMirroring.lastCodePoint(), bidiBrackets.lastCodePoint()))
{
}

void PairingLookupGenerator::setSegmentSize(size_t segmentSize) {
    m_segmentSize = min(MAX_SEGMENT_SIZE, max(MIN_SEGMENT_SIZE, segmentSize));
}

void PairingLookupGenerator::analyzeData() {
    cout << "Analyzing data for pairing lookup." << endl;

    size_t minMemory = SIZE_MAX;
    size_t segmentSize = 0;

    m_segmentSize = MIN_SEGMENT_SIZE;
    while (m_segmentSize <= MAX_SEGMENT_SIZE) {
        collectData();

        size_t memory = m_dataSize + (m_differencesSize * 2) + (m_indexesSize * 2);
        if (memory < minMemory) {
            segmentSize = m_segmentSize;
            minMemory = memory;
        }

        m_segmentSize++;
    }
    m_segmentSize = segmentSize;

    cout << "  Segment Size: " << segmentSize << endl;
    cout << "  Required Memory: " << minMemory << " bytes" << endl;

    cout << "Finished analysis." << endl << endl;
}

void PairingLookupGenerator::collectData() {
    size_t pairCount = m_lastCodePoint - m_firstCodePoint;
    size_t maxSegments = Math::FastCeil(pairCount, m_segmentSize);

    m_data.clear();
    m_data.reserve(maxSegments);

    m_differences.clear();
    m_differences.reserve(32);
    m_differences.push_back(0);

    m_indexes.clear();
    m_indexes.reserve(maxSegments);

    m_dataSize = 0;

    for (size_t i = 0; i < maxSegments; i++) {
        uint32_t segmentStart = (uint32_t)(m_firstCodePoint + (i * m_segmentSize));
        uint32_t segmentEnd = min(m_lastCodePoint, (uint32_t)(segmentStart + m_segmentSize - 1));
        
        DataSet dataset(new UnsafeDataSet());
        dataset->reserve(m_segmentSize);

        for (uint32_t codePoint = segmentStart; codePoint <= segmentEnd; codePoint++) {
            uint32_t mirror = m_bidiMirroring.mirrorOf(codePoint);
            uint32_t bracket = m_bidiBrackets.pairedBracketOf(codePoint);
            char bracketType = m_bidiBrackets.pairedBracketTypeOf(codePoint);
            if (bracket && mirror != bracket) {
                cout << "Logic Broken:" << endl
                     << "  Code Point: " << codePoint << endl
                     << "  Mirror: " << mirror << endl
                     << "  Paired Bracket: " << bracket << endl;
            }

            int16_t difference = (mirror ? mirror - codePoint : 0);

            auto begin = m_differences.begin();
            auto end = m_differences.end();
            auto match = find(begin, end, difference);

            uint8_t data;
            if (match != end) {
                data = distance(begin, match);
            } else {
                data = m_differences.size();
                m_differences.push_back(difference);
            }

            switch (bracketType) {
            case 'o':
                data |= BracketTypeOpen;
                break;

            case 'c':
                data |= BracketTypeClose;
                break;
            }

            dataset->push_back(data);
        }

        size_t segmentIndex = SIZE_MAX;
        size_t segmentCount = m_data.size();
        for (size_t j = 0; j < segmentCount; j++) {
            if (*m_data[j].dataset == *dataset) {
                segmentIndex = j;
            }
        }

        if (segmentIndex == SIZE_MAX) {
            segmentIndex = m_data.size();
            m_data.push_back(DataSegment(m_dataSize, dataset));
            m_dataSize += dataset->size();
        }

        m_indexes.push_back(&m_data.at(segmentIndex));
    }

    m_differencesSize = m_differences.size();
    m_indexesSize = m_indexes.size();
}

void PairingLookupGenerator::generateFile(const std::string &directory) {
    collectData();

    ArrayBuilder arrDifferences;
    arrDifferences.setDataType(DIFFERENCES_ARRAY_TYPE);
    arrDifferences.setName(DIFFERENCES_ARRAY_NAME);
    arrDifferences.setElementSpace(5);
    arrDifferences.setSizeDescriptor(to_string(m_differencesSize));

    for (const auto &difference : m_differences) {
        auto element = to_string(difference);
        bool isLast = (&difference == &m_differences.back());

        arrDifferences.appendElement(element);
        if (!isLast) {
            arrDifferences.newElement();
        }
    }

    ArrayBuilder arrData;
    arrData.setDataType(DATA_ARRAY_TYPE);
    arrData.setName(DATA_ARRAY_NAME);
    arrData.setElementSpace(3);
    arrData.setSizeDescriptor(to_string(m_dataSize));

    for (const auto &data : m_data) {
        bool isLast = (&data == &m_data.back());

        arrData.append(data.hintLine());
        arrData.newLine();

        size_t length = data.dataset->size();

        for (size_t j = 0; j < length; j++) {
            const string &element = to_string(data.dataset->at(j));
            arrData.appendElement(element);
            arrData.newElement();
        }

        if (!isLast) {
            arrData.newLine();
        }
    }

    ArrayBuilder arrIndexes;
    arrIndexes.setDataType(INDEXES_ARRAY_TYPE);
    arrIndexes.setName(INDEXES_ARRAY_NAME);
    arrIndexes.setSizeDescriptor(to_string(m_indexesSize));

    for (const auto &index : m_indexes) {
        const auto &data = *index;
        bool isLast = (&index == &m_indexes.back());
        string element = "0x" + Converter::toHex(data.index, 4);

        arrIndexes.appendElement(element);
        if (!isLast) {
            arrIndexes.newElement();
        }
    }

    TextBuilder mirror;
    string maxCodePoint = "0x" + Converter::toHex(m_lastCodePoint, 4);
    string segmentSize = "0x" + Converter::toHex(m_segmentSize, 3);
    mirror.append("").newLine();
    mirror.append("SB_INTERNAL SBCodepoint LookupMirror(SBCodepoint codepoint)").newLine();
    mirror.append("{").newLine();
    mirror.appendTabs(1).append("if (codepoint <= " + maxCodePoint + ") {").newLine();
    mirror.appendTabs(2).append("SBInt16 diff = PairDifferences[").newLine();
    mirror.appendTabs(2).append("                PairData[").newLine();
    mirror.appendTabs(2).append("                 PairIndexes[").newLine();
    mirror.appendTabs(2).append("                      codepoint / " + segmentSize).newLine();
    mirror.appendTabs(2).append("                 ] + (codepoint % " + segmentSize + ")").newLine();
    mirror.appendTabs(2).append("                ] & BracketTypeInverseMask").newLine();
    mirror.appendTabs(2).append("               ];").newLine();
    mirror.newLine();
    mirror.appendTabs(2).append("if (diff != 0) {").newLine();
    mirror.appendTabs(3).append("return (codepoint + diff);").newLine();
    mirror.appendTabs(2).append("}").newLine();
    mirror.appendTabs(1).append("}").newLine();
    mirror.newLine();
    mirror.appendTabs(1).append("return 0;").newLine();
    mirror.append("}").newLine();

    TextBuilder bracket;
    bracket.append("SB_INTERNAL SBCodepoint LookupBracketPair(SBCodepoint codepoint, BracketType *type)").newLine();
    bracket.append("{").newLine();
    bracket.appendTabs(1).append("if (codepoint <= " + maxCodePoint + ") {").newLine();
    bracket.appendTabs(2).append("SBUInt8 data = PairData[").newLine();
    bracket.appendTabs(2).append("                PairIndexes[").newLine();
    bracket.appendTabs(2).append("                     codepoint / " + segmentSize).newLine();
    bracket.appendTabs(2).append("                ] + (codepoint % " + segmentSize + ")").newLine();
    bracket.appendTabs(2).append("               ];").newLine();
    bracket.appendTabs(2).append("*type = (data & BracketTypePrimaryMask);").newLine();
    bracket.newLine();
    bracket.appendTabs(2).append("if (*type != 0) {").newLine();
    bracket.appendTabs(3).append("SBInt16 diff = PairDifferences[").newLine();
    bracket.appendTabs(3).append("                data & BracketTypeInverseMask").newLine();
    bracket.appendTabs(3).append("               ];").newLine();
    bracket.appendTabs(3).append("return (codepoint + diff);").newLine();
    bracket.appendTabs(2).append("}").newLine();
    bracket.appendTabs(1).append("} else {").newLine();
    bracket.appendTabs(2).append("*type = BracketTypeNone;").newLine();
    bracket.appendTabs(1).append("}").newLine();
    bracket.newLine();
    bracket.appendTabs(1).append("return 0;").newLine();
    bracket.append("}").newLine();

    FileBuilder header(directory + "/PairingLookup.h");
    header.append("/*").newLine();
    header.append(" * Automatically generated by SheenBidiGenerator tool.").newLine();
    header.append(" * DO NOT EDIT!!").newLine();
    header.append(" */").newLine();
    header.newLine();
    header.append("#ifndef _SB_INTERNAL_PAIRING_LOOKUP_H").newLine();
    header.append("#define _SB_INTERNAL_PAIRING_LOOKUP_H").newLine();
    header.newLine();
    header.append("#include <SheenBidi/SBBase.h>").newLine();
    header.append("#include <SheenBidi/SBCodepoint.h>").newLine();
    header.append("#include <SheenBidi/SBConfig.h>").newLine();
    header.newLine();
    header.append("#include \"BracketType.h\"").newLine();
    header.newLine();
    header.append("SB_INTERNAL SBCodepoint LookupMirror(SBCodepoint codepoint);").newLine();
    header.append("SB_INTERNAL SBCodepoint LookupBracketPair(SBCodepoint codepoint, BracketType *bracketType);").newLine();
    header.newLine();
    header.append("#endif").newLine();

    FileBuilder source(directory + "/PairingLookup.c");
    source.append("/*").newLine();
    source.append(" * Automatically generated by SheenBidiGenerator tool.").newLine();
    source.append(" * DO NOT EDIT!!").newLine();
    source.append(" *").newLine();
    source.append(" * REQUIRED MEMORY: (" + to_string(m_differencesSize)
                  + ")+" + to_string((int)m_dataSize) + "+("
                  + to_string(m_indexesSize) + "*2) = "
                  + to_string((m_differencesSize*2 + m_dataSize + m_indexesSize*2))
                  + " Bytes").newLine();
    source.append(" */").newLine();
    source.newLine();
    source.append("#include \"PairingLookup.h\"").newLine();
    source.newLine();
    source.append(arrDifferences);
    source.newLine();
    source.append(arrData);
    source.newLine();
    source.append(arrIndexes);
    source.append(mirror).newLine();
    source.append(bracket);
}
