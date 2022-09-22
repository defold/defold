// Copyright 2020-2022 The Defold Foundation
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
#include <string.h>
#include "message.h"
#include "atomic.h"
#include "hash.h"
#include "hashtable.h"
#include "array.h"
#include "condition_variable.h"
#include "dstrings.h"
#include <dlib/mutex.h>
#include <dlib/static_assert.h>
#include <dlib/spinlock.h>
#include <dlib/profile/profile.h>

DM_PROPERTY_GROUP(rmtp_Message, "dmMessage");
DM_PROPERTY_U32(rmtp_Messages, 0, FrameReset, "# messages/frame", &rmtp_Message);

namespace dmMessage
{
    // Alignment of allocations
    const uint32_t DM_MESSAGE_ALIGNMENT = 16U;

    struct MemoryPage
    {
        uint8_t     m_Memory[DM_MESSAGE_PAGE_SIZE];
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

    struct GlobalInit
    {
        GlobalInit() {
            // Make sure the struct sizes are in sync! Think of potential save files!
            DM_STATIC_ASSERT(sizeof(dmMessage::URL) == 32, Invalid_Struct_Size);
        }

    } g_MessageInit;

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
        size += DM_MESSAGE_ALIGNMENT-1;
        size &= ~(DM_MESSAGE_ALIGNMENT-1);
        assert(size <= DM_MESSAGE_PAGE_SIZE);

        if (allocator->m_CurrentPage == 0 || (DM_MESSAGE_PAGE_SIZE-allocator->m_CurrentPage->m_Current) < size)
        {
            // No current page or allocation didn't fit.
            AllocateNewPage(allocator);
        }

        MemoryPage* page = allocator->m_CurrentPage;
        void* ret = (void*) ((uintptr_t) &page->m_Memory[0] + page->m_Current);
        page->m_Current += size;
        return ret;
    }

    struct MessageSocket
    {
        uint32_t        m_RefCount; // Is protected by "g_MessageContext->m_Spinlock"
        dmhash_t        m_NameHash;
        Message*        m_Header;
        Message*        m_Tail;
        const char*     m_Name;
        dmMutex::HMutex m_Mutex;
        dmConditionVariable::HConditionVariable m_Condition;
        MemoryAllocator m_Allocator;
    };

    const uint32_t MAX_SOCKETS = 256;

    struct MessageContext
    {
        dmHashTable64<MessageSocket> m_Sockets;
        dmSpinlock::Spinlock m_Spinlock;
    };

    MessageContext* g_MessageContext = 0;

    static MessageContext* Create(uint32_t max_sockets)
    {
        MessageContext* ctx = new MessageContext;
        ctx->m_Sockets.SetCapacity(max_sockets, max_sockets);
        dmSpinlock::Init(&ctx->m_Spinlock);
        return ctx;
    }

    // Until the Create/Destroy functions are exposed:
    // The context is created on demand, and we also need to destroy it automatically
    struct ContextDestroyer
    {
        ~ContextDestroyer()
        {
            if (g_MessageContext)
            {
                delete g_MessageContext;
                g_MessageContext = 0;
            }
        }
    } g_ContextDestroyer;

    Result NewSocket(const char* name, HSocket* socket)
    {
        if (g_MessageContext == 0)
        {
            g_MessageContext = Create(MAX_SOCKETS);
        }
        if (name == 0x0 || *name == 0 || strchr(name, '#') != 0x0 || strchr(name, ':') != 0x0)
        {
            return RESULT_INVALID_SOCKET_NAME;
        }

        HSocket tmp;
        if (GetSocket(name, &tmp) == RESULT_OK)
        {
            return RESULT_SOCKET_EXISTS;
        }

        dmhash_t name_hash = dmHashString64(name);

        DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);

        if (g_MessageContext->m_Sockets.Full())
        {
            return RESULT_SOCKET_OUT_OF_RESOURCES;
        }

        MessageSocket s;
        s.m_RefCount = 1;
        s.m_Header = 0;
        s.m_Tail = 0;
        s.m_NameHash = name_hash;
        s.m_Name = strdup(name);
        s.m_Mutex = dmMutex::New();
        s.m_Condition = dmConditionVariable::New();

        g_MessageContext->m_Sockets.Put(name_hash, s);
        *socket = name_hash;

        return RESULT_OK;
    }

    static void DisposeSocket(MessageSocket* s)
    {
        Message *message_object = s->m_Header;
        while (message_object)
        {
            if (message_object->m_DestroyCallback)
            {
                message_object->m_DestroyCallback(message_object);
            }
            message_object = message_object->m_Next;
        }

        free((void*) s->m_Name);

        MemoryPage* p = s->m_Allocator.m_FreePages;
        while (p)
        {
            MemoryPage* next = p->m_NextPage;
            delete p;
            p = next;
        }
        p = s->m_Allocator.m_FullPages;
        while (p)
        {
            MemoryPage* next = p->m_NextPage;
            delete p;
            p = next;
        }
        if (s->m_Allocator.m_CurrentPage)
        {
            delete s->m_Allocator.m_CurrentPage;
        }

        dmConditionVariable::Delete(s->m_Condition);

        dmMutex::Delete(s->m_Mutex);

        memset(s, 0, sizeof(*s));
    }

    static void ReleaseSocket(MessageSocket* s)
    {
        {
            DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);
            --s->m_RefCount;

            if (s->m_RefCount > 0)
            {
                return;
            }
        }
        DisposeSocket(s);
    }

    static MessageSocket* AcquireSocket(HSocket socket)
    {
        DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);

        MessageSocket* s = g_MessageContext->m_Sockets.Get(socket);

        if (s == 0x0)
        {
            return 0x0;
        }

        assert(s->m_RefCount >= 1);

        ++s->m_RefCount;

        return s;
    }

    Result DeleteSocket(HSocket socket)
    {
        MessageSocket* s = 0x0;
        {
            DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);
            s = g_MessageContext->m_Sockets.Get(socket);
            if (s == 0x0)
            {
                return RESULT_SOCKET_NOT_FOUND;
            }

            g_MessageContext->m_Sockets.Erase(s->m_NameHash);
            --s->m_RefCount;

            if(s->m_RefCount > 0)
            {
                // Defer deletion
                return RESULT_OK;
            }
        }
        DisposeSocket(s);
        return RESULT_OK;
    }

    Result GetSocket(const char *name, HSocket* out_socket)
    {
        DM_PROFILE("GetSocket");

        if (name == 0x0 || *name == 0 || strchr(name, '#') != 0x0 || strchr(name, ':') != 0x0)
        {
            return RESULT_INVALID_SOCKET_NAME;
        }

        dmhash_t name_hash = dmHashString64(name);
        *out_socket = name_hash;

        DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);

        MessageSocket* message_socket = g_MessageContext->m_Sockets.Get(name_hash);
        if (message_socket)
        {
            return RESULT_OK;
        }
        return RESULT_NAME_OK_SOCKET_NOT_FOUND;
    }

    const char* GetSocketName(HSocket socket)
    {
        DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);

        MessageSocket* message_socket = g_MessageContext->m_Sockets.Get(socket);
        if (message_socket != 0x0)
        {
            return message_socket->m_Name;
        }
        else
        {
            return 0x0;
        }
    }

    bool IsSocketValid(HSocket socket)
    {
        if (socket != 0)
        {
            DM_SPINLOCK_SCOPED_LOCK(g_MessageContext->m_Spinlock);
            MessageSocket* message_socket = g_MessageContext->m_Sockets.Get(socket);
            return message_socket != 0;
        }
        return false;
    }

    bool HasMessages(HSocket socket)
    {
        if (!socket)
            return false;

        MessageSocket* s = AcquireSocket(socket);
        if (s != 0)
        {
            bool has_messages;
            {
                DM_MUTEX_SCOPED_LOCK(s->m_Mutex);
                has_messages = s->m_Header != 0;
            }
            ReleaseSocket(s);
            return has_messages;
        }
        return false;
    }

    void ResetURL(URL* url)
    {
        memset((void*)url, 0, sizeof(URL));
    }

    void ResetURL(URL& url)
    {
        ResetURL(&url);
    }

    HSocket GetSocket(const URL* url)
    {
        return url->m_Socket;
    }

    void SetSocket(URL* url, HSocket socket)
    {
        url->m_Socket = socket;
    }

    dmhash_t GetPath(const URL* url)
    {
        return url->m_Path;
    }

    void SetPath(URL* url, dmhash_t path)
    {
        url->m_Path = path;
    }

    dmhash_t GetFragment(const URL* url)
    {
        return url->m_Fragment;
    }

    void SetFragment(URL* url, dmhash_t fragment)
    {
        url->m_Fragment = fragment;
    }

    Result Post(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t user_data2,
                    uintptr_t descriptor, const void* message_data, uint32_t message_data_size, MessageDestroyCallback destroy_callback)
    {
        DM_PROFILE("Post");
        //Currently called out by the Thread Sanitizer: DM_PROPERTY_ADD_U32(rmtp_Messages, 1);

        if (receiver == 0x0)
        {
            return RESULT_SOCKET_NOT_FOUND;
        }

        MessageSocket* s = AcquireSocket(receiver->m_Socket);
        if (s == 0x0)
        {
            return RESULT_SOCKET_NOT_FOUND;
        }

        dmMutex::Lock(s->m_Mutex);

        MemoryAllocator* allocator = &s->m_Allocator;
        uint32_t data_size = sizeof(Message) + message_data_size;
        Message *new_message = (Message *) AllocateMessage(allocator, data_size);
        if (sender != 0x0)
        {
            new_message->m_Sender = *sender;
        }
        else
        {
            ResetURL(&new_message->m_Sender);
        }
        new_message->m_Receiver = *receiver;
        new_message->m_Id = message_id;
        new_message->m_UserData1 = user_data1;
        new_message->m_UserData2 = user_data2;
        new_message->m_Descriptor = descriptor;
        new_message->m_DataSize = message_data_size;
        new_message->m_Next = 0;
        new_message->m_DestroyCallback = destroy_callback;
        memcpy(&new_message->m_Data[0], message_data, message_data_size);

        bool is_first_message = !s->m_Header;

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

        if (is_first_message)
        {
            dmConditionVariable::Signal(s->m_Condition);
        }
        dmMutex::Unlock(s->m_Mutex);

        ReleaseSocket(s);

        return RESULT_OK;
    }

    Result Post(const URL* sender, const URL* receiver, dmhash_t message_id, uintptr_t user_data1, uintptr_t descriptor,
                    const void* message_data, uint32_t message_data_size, MessageDestroyCallback destroy_callback)
    {
        return Post(sender, receiver, message_id, user_data1, 0, descriptor, message_data, message_data_size, destroy_callback);
    }


    // Fast length limited string concatenation that assume we already point to
    // the end of the string. Returns the new end of the string so we do not need
    // to calculate the length of the input string or output string
    static char* ConcatString(char* w_ptr, const char* w_ptr_end, const char* str)
    {
        while ((w_ptr != w_ptr_end) && *str)
        {
            *w_ptr++ = *str++;
        }
        return w_ptr;
    }

    // Low level string concatenation to void the overhead of dmSnPrintf and having to call strlen
    static const char* GetProfilerString(const char* socket_name, char* buffer, uint32_t buffer_size)
    {
        if (!dmProfile::IsInitialized())
            return 0;

        char* w_ptr = buffer;
        const char* w_ptr_end = buffer + buffer_size - 1;
        w_ptr = ConcatString(w_ptr, w_ptr_end, "Dispatch ");
        w_ptr = ConcatString(w_ptr, w_ptr_end, socket_name);
        *w_ptr++ = 0;
        return buffer;
    }

    uint32_t InternalDispatch(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr, bool blocking)
    {
        MessageSocket* s = AcquireSocket(socket);
        if (s == 0)
        {
            return 0;
        }

        dmMutex::Lock(s->m_Mutex);

        MemoryAllocator* allocator = &s->m_Allocator;

        if (!s->m_Header)
        {
            if (blocking) {
                dmConditionVariable::Wait(s->m_Condition, s->m_Mutex);
            } else {
                dmMutex::Unlock(s->m_Mutex);
                ReleaseSocket(s);
                return 0;
            }
        }


        char buffer[128];
        const char* profiler_string = GetProfilerString(s->m_Name, buffer, sizeof(buffer));
        DM_PROFILE_DYN(profiler_string, 0);

        uint32_t dispatch_count = 0;

        Message *message_object = s->m_Header;
        s->m_Header = 0;
        s->m_Tail = 0;

        // Unlink full pages
        MemoryPage* full_pages = allocator->m_FullPages;
        allocator->m_FullPages = 0;

        dmMutex::Unlock(s->m_Mutex);

        while (message_object)
        {
            dispatch_callback(message_object, user_ptr);
            if (message_object->m_DestroyCallback) {
                message_object->m_DestroyCallback(message_object);
            }
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

        ReleaseSocket(s);

        return dispatch_count;
    }

    uint32_t Dispatch(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr)
    {
        return InternalDispatch(socket, dispatch_callback, user_ptr, false);
    }

    uint32_t DispatchBlocking(HSocket socket, DispatchCallback dispatch_callback, void* user_ptr)
    {
        return InternalDispatch(socket, dispatch_callback, user_ptr, true);
    }

    static void ConsumeCallback(dmMessage::Message*, void*)
    {
    }

    uint32_t Consume(HSocket socket)
    {
        return Dispatch(socket, &ConsumeCallback, 0);
    }

    Result ParseURL(const char* uri, StringURL* out_url)
    {
        if (uri == 0x0)
        {
            *out_url = StringURL();
            return RESULT_OK;
        }
        const char* socket = 0x0;
        uint32_t socket_size = 0;
        const char* path = uri;
        uint32_t path_size = 0;
        const char* fragment = 0x0;
        uint32_t fragment_size = 0;

        const char* socket_end = 0;
        const char* fragment_start = 0;
        const char* scan = uri;
        while (*scan)
        {
            switch(*scan)
            {
                case ':':
                    if (socket_end != 0)
                    {
                        return RESULT_MALFORMED_URL;
                    }
                    if (fragment_start != 0)
                    {
                        return RESULT_MALFORMED_URL;
                    }
                    socket_end = scan;
                    break;
                case '#':
                    if (fragment_start != 0)
                    {
                        return RESULT_MALFORMED_URL;
                    }
                    fragment_start = scan;
                    break;
            }
            ++scan;
        };

        if (socket_end != 0x0)
        {
            socket_size = socket_end - uri;
            if (socket_size >= 64)
            {
                return RESULT_MALFORMED_URL;
            }
            socket = uri;
            path = socket_end + 1;
        }

        uint32_t uri_length = (uint32_t)(scan - uri);
        if (fragment_start != 0x0)
        {
            fragment = fragment_start + 1;
            fragment_size = uri_length - (fragment - uri);
            path_size = fragment_start - path;
        }
        else
        {
            path_size = uri_length - (path - uri);
        }
        out_url->m_Socket = socket;
        out_url->m_SocketSize = socket_size;
        out_url->m_Path = path;
        out_url->m_PathSize = path_size;
        out_url->m_Fragment = fragment;
        out_url->m_FragmentSize = fragment_size;
        return RESULT_OK;
    }
}

