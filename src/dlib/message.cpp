#include <assert.h>
#include <string.h>
#include "message.h"
#include "atomic.h"
#include "hash.h"
#include "profile.h"
#include "array.h"
#include "index_pool.h"
#include "mutex.h"

namespace dmMessage
{
    // Alignment of allocations
    const uint32_t ALIGNMENT = 4U;
    // Page size must be a multiple of ALIGNMENT. Currently 4 but could be changed to 16.
    // This simplifies the allocation scheme
    const uint32_t PAGE_SIZE = 4096U;

    struct MemoryPage
    {
        uint8_t     m_Memory[PAGE_SIZE];
        uint32_t    m_Current;
        MemoryPage* m_NextPage;
    };

    struct MemoryAllocator
    {
        MemoryAllocator()
        {
            m_CurrentPage = 0;
            m_FreePages = 0;
            m_FullPages = 0;
        }
        MemoryPage* m_CurrentPage;
        MemoryPage* m_FreePages;
        MemoryPage* m_FullPages;
    };

    static void AllocateNewPage(MemoryAllocator* allocator)
    {
        if (allocator->m_CurrentPage)
        {
            // Link current page to full pages
            allocator->m_CurrentPage->m_NextPage = allocator->m_FullPages;
            allocator->m_FullPages = allocator->m_CurrentPage;
        }

        MemoryPage* new_page = 0;

        if (allocator->m_FreePages)
        {
            // Free page to use
            new_page = allocator->m_FreePages;
            allocator->m_FreePages = new_page->m_NextPage;
        }
        else
        {
            // Allocate new page
            new_page = new MemoryPage;
        }

        new_page->m_Current = 0;
        new_page->m_NextPage = 0;

        allocator->m_CurrentPage = new_page;
    }

    static void* AllocateMessage(MemoryAllocator* allocator, uint32_t size)
    {
        // At least ALIGNMENT bytes alignment of size in order to ensure that the next allocation is aligned
        size += ALIGNMENT-1;
        size &= ~(ALIGNMENT-1);
        assert(size <= PAGE_SIZE);

        if (allocator->m_CurrentPage == 0 || (PAGE_SIZE-allocator->m_CurrentPage->m_Current) < size)
        {
            // No current page or allocation didn't fit.
            AllocateNewPage(allocator);
        }

        MemoryPage* page = allocator->m_CurrentPage;
        void*ret = (void*) ((uintptr_t) &page->m_Memory[0] + page->m_Current);
        page->m_Current += size;
        return ret;
    }

    struct MessageSocket
    {
        Message*        m_Header;
        Message*        m_Tail;
        uint32_t        m_NameHash;
        uint16_t        m_Version;
        const char*     m_Name;
        dmMutex::Mutex  m_Mutex;
        MemoryAllocator m_Allocator;
    };

    const uint32_t MAX_SOCKETS = 128;
    bool g_Initialized = false;
    int32_atomic_t g_NextVersionNumber = 0;
    dmArray<MessageSocket> g_Sockets;
    dmIndexPool16 g_SocketPool;

    HSocket GetSocket(const char *name);

    Result NewSocket(const char* name, HSocket* socket)
    {
        if (!g_Initialized)
        {
            g_Sockets.SetCapacity(MAX_SOCKETS);
            g_Sockets.SetSize(MAX_SOCKETS);
            memset(&g_Sockets[0], 0, sizeof(g_Sockets[0]) * MAX_SOCKETS);
            g_SocketPool.SetCapacity(MAX_SOCKETS);
            g_Initialized = true;
        }

        if (GetSocket(name))
        {
            return RESULT_SOCKET_EXISTS;
        }

        if (g_SocketPool.Remaining() == 0)
        {
            return RESULT_SOCKET_OUT_OF_RESOURCES;
        }

        uint16_t id  = g_SocketPool.Pop();
        uint32_t name_hash = dmHashString32(name);

        MessageSocket s;
        s.m_Header = 0;
        s.m_Tail = 0;
        s.m_NameHash = name_hash;
        s.m_Version = dmAtomicIncrement32(&g_NextVersionNumber);
        s.m_Name = strdup(name);
        s.m_Mutex = dmMutex::New();

        // 0 is an invalid handle. We can't use 0 as version number.
        if (s.m_Version == 0)
            s.m_Version = dmAtomicIncrement32(&g_NextVersionNumber);

        g_Sockets[id] = s;
        *socket = s.m_Version << 16 | id;

        return RESULT_OK;
    }

    static MessageSocket* GetSocketInternal(HSocket socket, uint16_t& id)
    {
        uint16_t version = socket >> 16;
        assert(version != 0);

        id = socket & 0xffff;

        MessageSocket* s = &g_Sockets[id];
        assert(s->m_Version == version);
        return s;
    }

    void DeleteSocket(HSocket socket)
    {
        uint16_t id;
        MessageSocket* s = GetSocketInternal(socket, id);
        free((void*) s->m_Name);

        MemoryPage* p = s->m_Allocator.m_FreePages;
        while (p)
        {
            MemoryPage* next = p->m_NextPage;
            delete p;
            p = next;
        }
        if (s->m_Allocator.m_CurrentPage)
            delete s->m_Allocator.m_CurrentPage;

        dmMutex::Delete(s->m_Mutex);

        memset(s, 0, sizeof(*s));
        g_SocketPool.Push(id);
    }

    HSocket GetSocket(const char *name)
    {
        DM_PROFILE(Message, "GetSocket")

        uint32_t name_hash = dmHashString32(name);
        for (uint32_t i = 0; i < g_Sockets.Size(); ++i)
        {
            MessageSocket* socket = &g_Sockets[i];
            if (socket->m_NameHash == name_hash)
            {
                return socket->m_Version << 16 | i;
            }
        }
        return 0;
    }

    uint32_t g_MessagesHash = dmHashString32("Messages");
    void Post(HSocket socket, uint32_t message_id, const void* message_data, uint32_t message_data_size)
    {
        DM_PROFILE(Message, "Post")
        DM_COUNTER_HASH("Messages", g_MessagesHash, 1)

        uint16_t id;
        MessageSocket*s = GetSocketInternal(socket, id);

        dmMutex::Lock(s->m_Mutex);
        MemoryAllocator* allocator = &s->m_Allocator;
        uint32_t data_size = sizeof(Message) + message_data_size;
        Message *new_message = (Message *) AllocateMessage(allocator, data_size);
        new_message->m_ID = message_id;
        new_message->m_DataSize = message_data_size;
        new_message->m_Next = 0;
        memcpy(&new_message->m_Data[0], message_data, message_data_size);

        if (!s->m_Header)
        {
            s->m_Header = new_message;
            s->m_Tail = new_message;
        }
        else
        {
            s->m_Tail->m_Next = new_message;
            s->m_Tail = new_message;
        }

        dmMutex::Unlock(s->m_Mutex);
    }

    uint32_t Dispatch(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr)
    {
        uint16_t id;
        MessageSocket*s = GetSocketInternal(socket, id);
        DM_PROFILE(Message, s->m_Name);

        dmMutex::Lock(s->m_Mutex);
        MemoryAllocator* allocator = &s->m_Allocator;

        if (!s->m_Header)
        {
            dmMutex::Unlock(s->m_Mutex);
            return 0;
        }
        uint32_t dispatch_count = 0;

        Message *message_object = s->m_Header;
        s->m_Header = 0;
        s->m_Tail = 0;

        // Unlink full pages
        MemoryPage*full_pages = allocator->m_FullPages;
        allocator->m_FullPages = 0;

        dmMutex::Unlock(s->m_Mutex);

        while (message_object)
        {
            dispatch_callback(message_object, user_ptr);
            message_object = message_object->m_Next;
            dispatch_count++;
        }

        // Reclaim all full pages active when dispatch started
        dmMutex::Lock(s->m_Mutex);
        MemoryPage* p = full_pages;
        while (p)
        {
            MemoryPage* next = p->m_NextPage;
            p->m_NextPage = allocator->m_FreePages;
            allocator->m_FreePages = p;
            p = next;
        }
        dmMutex::Unlock(s->m_Mutex);

        return dispatch_count;
    }

    static void ConsumeCallback(dmMessage::Message*, void*)
    {
    }

    uint32_t Consume(HSocket socket)
    {
        return Dispatch(socket, &ConsumeCallback, 0);
    }
}

