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

#ifndef DM_FONT_EFFECT_INDEX_H
#define DM_FONT_EFFECT_INDEX_H

#include <assert.h>
#include <string.h>
#include "../font_effect.h"
#include <dlib/array.h>
#include <dlib/hashtable.h>

// Maps content hashes to texture rows without owning or copying the records.
// Hash collisions chain row indices and compare the complete record. Callers
// erase a row before replacing it, and keep records alive during lookup.
struct FontEffectIndex
{
    dmHashTable64<uint32_t> m_Rows;
    dmArray<uint32_t>       m_Next;
};

static inline void FontResizeEffectIndex(FontEffectIndex* index, uint32_t capacity)
{
    index->m_Rows.SetCapacity(capacity, capacity);
    index->m_Next.EnsureSize(capacity);
}

static inline uint32_t FontFindEffect(const FontEffectIndex& index, uint64_t hash, const FontEffectRecord& record, const FontEffectRecord* records)
{
    const uint32_t* first = index.m_Rows.Get(hash);
    for (uint32_t row = first ? *first : 0; row; row = index.m_Next[row])
    {
        if (memcmp(&record, &records[row], sizeof(record)) == 0)
        {
            return row;
        }
    }
    return 0;
}

static inline void FontInsertEffect(FontEffectIndex* index, uint64_t hash, uint32_t row)
{
    const uint32_t* first = index->m_Rows.Get(hash);
    index->m_Next[row] = first ? *first : 0;
    index->m_Rows.Put(hash, row);
}

static inline void FontEraseEffect(FontEffectIndex* index, uint64_t hash, uint32_t row)
{
    uint32_t* link = index->m_Rows.Get(hash);
    assert(link);
    if (*link == row && !index->m_Next[row])
    {
        index->m_Rows.Erase(hash);
        return;
    }
    while (*link != row)
    {
        link = &index->m_Next[*link];
    }
    *link = index->m_Next[row];
}

#endif // DM_FONT_EFFECT_INDEX_H
