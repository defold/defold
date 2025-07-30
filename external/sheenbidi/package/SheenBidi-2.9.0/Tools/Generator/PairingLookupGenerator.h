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

#ifndef SHEENBIDI_GENERATOR_PAIRING_LOOKUP_GENERATOR_H
#define SHEENBIDI_GENERATOR_PAIRING_LOOKUP_GENERATOR_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Parser/BidiMirroring.h>
#include <Parser/BidiBrackets.h>

namespace SheenBidi {
namespace Generator {

class PairingLookupGenerator {
public:
    PairingLookupGenerator(const Parser::BidiMirroring &bidiMirroring, const Parser::BidiBrackets &bidiBrackets);

    void setSegmentSize(size_t);

    void analyzeData();
    void generateFile(const std::string &directory);

private:
    typedef std::vector<uint8_t> UnsafeDataSet;
    typedef std::shared_ptr<UnsafeDataSet> DataSet;

    struct DataSegment {
        const size_t index;
        const DataSet dataset;

        DataSegment(size_t index, DataSet dataset);

        const std::string hintLine() const;
    };

    const Parser::BidiMirroring &m_bidiMirroring;
    const Parser::BidiBrackets &m_bidiBrackets;

    const uint32_t m_firstCodePoint;
    const uint32_t m_lastCodePoint;

    size_t m_segmentSize = 0;

    std::vector<int16_t> m_differences;
    std::vector<DataSegment> m_data;
    std::vector<DataSegment *> m_indexes;
    
    size_t m_differencesSize = 0;
    size_t m_dataSize = 0;
    size_t m_indexesSize = 0;

    void collectData();
};

}
}

#endif
