#include "poolallocator.h"

#include <assert.h>
#include <new>
#include <string.h>

namespace dmPoolAllocator
{
    struct Page
    {
        uint32_t  m_Current;
        Page*     m_Next;

        Page()
        {
            m_Current = 0;
            m_Next = 0;
        }

        char  m_Buffer[0];
    };

    struct Pool
    {
        Page*    m_CurrentPage;
        uint32_t m_PageSize;
    };

    HPool New(uint32_t page_size)
    {
        Pool* pool = new Pool();
        pool->m_PageSize = page_size;
        char* page_buffer = new char[sizeof(Page) + pool->m_PageSize];
        pool->m_CurrentPage = new (page_buffer) Page;
        return pool;
    }

    void Delete(HPool pool)
    {
        Page* p = pool->m_CurrentPage;
        while (p)
        {
            Page* tmp = p->m_Next;
            char* page_buffer = (char*) p;
            delete[] page_buffer;
            p = tmp;
        }
        delete pool;
    }

    void* Alloc(HPool pool, uint32_t size)
    {
        assert(size <= pool->m_PageSize);

        Page* p = pool->m_CurrentPage;
        uint32_t left = pool->m_PageSize - p->m_Current;
        if (left < size)
        {
            char* page_buffer = new char[sizeof(Page) + pool->m_PageSize];
            Page* new_page = new (page_buffer) Page;
            new_page->m_Next = p;
            pool->m_CurrentPage = new_page;
            p = new_page;
        }

        void* ret = (void*) &p->m_Buffer[p->m_Current];
        p->m_Current += size;
        return ret;
    }

    char* Duplicate(HPool pool, const char* string)
    {
        uint32_t size = strlen(string) + 1;
        char* buf = (char*) Alloc(pool, size);
        memcpy(buf, string, size);
        return buf;
    }

}
