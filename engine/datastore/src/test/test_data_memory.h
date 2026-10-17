// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef TEST_DATA_MEMORY_H
#define TEST_DATA_MEMORY_H

#include "../data.h"

// Test snapshot for ownership and allocation lifetime checks. Payload used/capacity bytes include chunk
// headers and alignment, excluding block headers. Sampling walks tables, rows and blocks.
struct DataMemoryStats
{
    uint64_t m_Tables;           // Live runtime table count.
    uint64_t m_Instances;        // Live blob registration count.
    uint64_t m_Rows;             // Live component row count.
    uint64_t m_StructRows;       // Live exclusively owned child rows, including reset defaults.
    uint64_t m_StructRowBytes;   // Child row capacity; included in m_ValueBytes.
    uint64_t m_ValueBytes;       // Mutable row capacity in bytes.
    uint64_t m_PayloadBlocks;    // Replacement arena block count.
    uint64_t m_PayloadUsed;      // Live replacement payload bytes, including allocation headers.
    uint64_t m_PayloadCapacity;  // Replacement payload capacity, excluding block headers.
    uint64_t m_BaseBlocks;       // Owned default-value arena block count.
    uint64_t m_BaseUsed;         // Live default payload bytes, including allocation headers.
    uint64_t m_BaseCapacity;     // Default payload capacity, excluding block headers.
    uint64_t m_StoreBytes;       // Store and table/query registry capacity in bytes.
    uint64_t m_TableBytes;       // Table objects and owned metadata/tag capacity in bytes.
    uint64_t m_RowMetadataBytes; // Runtime row identity capacity in bytes.
    uint64_t m_SlotBytes;        // Stable row ID slot capacity in bytes.
    uint64_t m_BaseRowBytes;     // Owned default row capacity in bytes.
    uint64_t m_InstanceBytes;    // Pools, registration pages and unused embedded storage in bytes.
    uint64_t m_QueryBytes;       // Query objects, copied filters, ranges and binding arenas in bytes.
    uint64_t m_TotalBytes;       // Retained store memory; excludes borrowed resource blobs.
};

void GetDataMemoryStats(HDataStore store, DataMemoryStats* out_stats);

#endif
