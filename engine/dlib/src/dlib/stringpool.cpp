#include "stringpool.h"

#include "hash.h"
#include "hashtable.h"

#include <assert.h>
#include <stdint.h>

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
        dmHashTable64<const char*> m_StringTable;
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

    const char* Add(HPool pool, const char* string)
    {
        uint32_t n = strlen(string) + 1;
        if (n == 1)
            return "";

        uint64_t string_hash = dmHashString64(string);
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
