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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../dlib/hashtable.h"
#include "stringpool.h"

namespace dmStringPool
{
    const uint32_t PAGE_SIZE = 4096;
    struct Page
    {
        char      m_Buffer[PAGE_SIZE];
        uint32_t  m_Current;
        Page*     m_Next;

        Page()
        {
            m_Current = 0;
            m_Next = 0;
        }
    };

    struct Pool
    {
        dmHashTable32<const char*> m_StringTable;
        Page*                      m_CurrentPage;
    };

    HPool New()
    {
        Pool* pool = new Pool();
        pool->m_CurrentPage = new Page;
        return pool;
    }

    void Delete(HPool pool)
    {
        Page* p = pool->m_CurrentPage;
        while (p)
        {
            Page* tmp = p->m_Next;
            delete p;
            p = tmp;
        }
        delete pool;
    }

    const char* Add(HPool pool, const char* string, uint32_t string_length, uint32_t string_hash)
    {
        uint32_t n = string_length + 1;
        if (n == 1)
            return "";

        assert(n <= PAGE_SIZE);

        const char** tmp = pool->m_StringTable.Get(string_hash);
        if (tmp)
            return *tmp;

        Page* p = pool->m_CurrentPage;
        uint32_t left = PAGE_SIZE - p->m_Current;
        if (left < n)
        {
            Page* new_page = new Page;
            new_page->m_Next = p;
            pool->m_CurrentPage = new_page;
            p = new_page;
        }

        const char* ret = (const char*) &p->m_Buffer[p->m_Current];
        memcpy(&p->m_Buffer[p->m_Current], string, n);
        p->m_Current += n;

        if (pool->m_StringTable.Full())
        {
            uint32_t new_capacity = pool->m_StringTable.Capacity() + 512;
            uint32_t new_size = 2 * new_capacity / 3;
            pool->m_StringTable.SetCapacity(new_size, new_capacity);
        }
        pool->m_StringTable.Put(string_hash, ret);

        return ret;
    }
}
