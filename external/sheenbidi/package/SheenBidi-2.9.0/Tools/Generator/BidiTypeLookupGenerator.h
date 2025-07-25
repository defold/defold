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

#ifndef SHEENBIDI_GENERATOR_BIDI_TYPE_LOOKUP_GENERATOR_H
#define SHEENBIDI_GENERATOR_BIDI_TYPE_LOOKUP_GENERATOR_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <Parser/DerivedBidiClass.h>

namespace SheenBidi {
namespace Generator {

class BidiTypeLookupGenerator { 
public:
    BidiTypeLookupGenerator(const Parser::DerivedBidiClass &derivedBidiClass);

    void setMainSegmentSize(size_t);
    void setBranchSegmentSize(size_t);

    void displayBidiClassesFrequency();

    void analyzeData();
    void generateFile(const std::string &directory);

private:
    typedef std::vector<const std::string *> UnsafeMainDataSet;
    typedef std::shared_ptr<UnsafeMainDataSet> MainDataSet;

    struct MainDataSegment {
        const size_t index;
        const MainDataSet dataset;

        MainDataSegment(size_t index, MainDataSet dataset);
        const std::string hintLine() const;
    };

    typedef std::vector<MainDataSegment *> UnsafeBranchDataSet;
    typedef std::shared_ptr<UnsafeBranchDataSet> BranchDataSet;

    struct BranchDataSegment {
        const size_t index;
        const BranchDataSet dataset;

        BranchDataSegment(size_t index, BranchDataSet dataset);
        const std::string hintLine() const;
    };

    const Parser::DerivedBidiClass &m_derivedBidiClass;

    size_t m_mainSegmentSize = 0;
    size_t m_branchSegmentSize = 0;

    std::vector<MainDataSegment> m_dataSegments;
    std::vector<MainDataSegment *> m_dataReferences;

    std::vector<BranchDataSegment> m_branchSegments;
    std::vector<BranchDataSegment *> m_branchReferences;

    size_t m_dataSize = 0;
    size_t m_mainIndexesSize = 0;
    size_t m_branchIndexesSize = 0;

    void collectMainData();
    void collectBranchData();
};

}
}

#endif
