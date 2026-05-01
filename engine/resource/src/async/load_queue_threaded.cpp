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

#include "resource.h"
#include "resource_private.h"
#include "load_queue.h"
#include "load_queue_private.h" // Request

#include <dlib/array.h>
#include <dlib/condition_variable.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <dlib/time.h>

namespace dmLoadQueue
{
    // Implementation of dmLoadQueue with a thread that loads items in the order they are supplied,

    // Default to small buffers since a lot of what is loaded are just small objects anyway.
    // That way we can have more in flight, but throttle when max pending data grows too large anyway
    const uint64_t DEFAULT_CAPACITY = 5 * 1024;

    // Once the loader has this amount not picked up, it will stop loading more.
    // This sets the bandwidth of the loader.
    const uint64_t MAX_PENDING_DATA = 4 * 1024 * 1024;
    const uint32_t QUEUE_SLOTS      = 16;

    struct Queue
    {
        Request                                 m_Request[QUEUE_SLOTS];
        dmResource::HFactory                    m_Factory;
        dmMutex::HMutex                         m_Mutex;
        dmConditionVariable::HConditionVariable m_WakeupCond;
        dmThread::Thread                        m_Thread;
        uint32_t                                m_Front;
        uint32_t                                m_Back;
        uint32_t                                m_Loaded;
        uint64_t                                m_BytesWaiting;
        bool                                    m_Shutdown;

        // Circular queue with indexing as follow (exclusive end)
        //
        //          m_Back           m_Loaded   m_Front
        // [N/A]   [loaded] [loaded] [to-load]  [N/A]
        //
    };

    static Request* GetNextRequest(Queue* queue)
    {
        // Since we can be loading many things at once, track the total Capacity() for buffers
        // that are waiting to be picked up by the preloader. In the case of the queue being filled
        // with only large requests (say only 4Mb textures), this throttles a bit so memory consumption
        // does not run away.
        if (queue->m_BytesWaiting >= MAX_PENDING_DATA)
        {
            return 0x0;
        }

        if (queue->m_Loaded == queue->m_Front)
        {
            return 0x0;
        }

        return &queue->m_Request[queue->m_Loaded % QUEUE_SLOTS];
    }

    static void LoadThread(void* arg)
    {
        Queue* queue     = (Queue*)arg;
        Request* current = 0;
        LoadResult result;
        while (true)
        {
            {
                dmMutex::ScopedLock lk(queue->m_Mutex);
                if (current != 0)
                {
                    // Just finished one (from previous iteration)
                    queue->m_BytesWaiting += current->m_Buffer.Capacity();
                    queue->m_Loaded++;
                    current->m_Result = result;
                    current           = 0;
                }
                if (queue->m_Shutdown)
                {
                    return;
                }

                current = GetNextRequest(queue);
                if (current == 0x0)
                {
                    // Nothing to do, reset any buffers of inactive requests that are not at default capacity
                    for (uint32_t i = 0; i < QUEUE_SLOTS; ++i)
                    {
                        Request* r = &queue->m_Request[i];
                        if (r->m_Buffer.Size() == 0)
                        {
                            if (r->m_Buffer.Capacity() > DEFAULT_CAPACITY)
                            {
                                // Just free the memory here, no need to allocate while holding the mutex
                                r->m_Buffer.SetCapacity(0);
                            }
                        }
                    }
                    dmConditionVariable::Wait(queue->m_WakeupCond, queue->m_Mutex);
                    current = GetNextRequest(queue);
                }
            }

            if (current)
            {
                // We use the temporary result object here to fill in the data so it can be written with the mutex held.
                DoLoadResource(queue->m_Factory, current, &current->m_Buffer, &result);
            }
        }
    }

    HQueue CreateQueue(dmResource::HFactory factory)
    {
        Queue* q          = new Queue();
        q->m_Factory      = factory;
        q->m_Front        = 0;
        q->m_Back         = 0;
        q->m_Loaded       = 0;
        q->m_Shutdown     = false;
        q->m_BytesWaiting = 0;
        q->m_Mutex        = dmMutex::New();
        q->m_WakeupCond   = dmConditionVariable::New();
        q->m_Thread       = dmThread::New(&LoadThread, 128 * 1024, q, "AsyncLoad");

        return q;
    }

    void DeleteQueue(HQueue queue)
    {
        {
            dmMutex::ScopedLock lk(queue->m_Mutex);
            queue->m_Shutdown = true;
            // Wake up the worker so it can exit and allow us to join
            dmConditionVariable::Signal(queue->m_WakeupCond);
        }
        dmThread::Join(queue->m_Thread);
        dmConditionVariable::Delete(queue->m_WakeupCond);
        dmMutex::Delete(queue->m_Mutex);
        delete queue;
    }

    HRequest BeginLoad(HQueue queue, const char* name, const char* canonical_path, PreloadInfo* info)
    {
        assert(name != 0);
        assert(name[0] != 0);
        assert(canonical_path != 0);
        assert(canonical_path[0] != 0);

        dmMutex::ScopedLock lk(queue->m_Mutex);

        // Refuse more if full.
        if ((queue->m_Front - queue->m_Back) == QUEUE_SLOTS)
            return 0;

        if (queue->m_Loaded == queue->m_Front)
        {
            // The worker is sleeping waiting for request, wake it up
            dmConditionVariable::Signal(queue->m_WakeupCond);
        }

        Request* req         = &queue->m_Request[(queue->m_Front++) % QUEUE_SLOTS];
        req->m_Name          = name;
        req->m_CanonicalPath = canonical_path;
        req->m_ResourceSize  = 0;

        req->m_PreloadInfo         = *info;
        req->m_Result.m_LoadResult = dmResource::RESULT_PENDING;

        return req;
    }

    Result EndLoad(HQueue queue, HRequest request, void** buf, uint32_t* buffer_size, uint32_t* resource_size, LoadResult* load_result)
    {
        dmMutex::ScopedLock lk(queue->m_Mutex);
        if (request->m_Result.m_LoadResult == dmResource::RESULT_PENDING)
            return RESULT_PENDING;

        *buf            = request->m_Buffer.Begin();
        *buffer_size    = request->m_Buffer.Size();
        *resource_size  = request->m_ResourceSize;
        *load_result    = request->m_Result;
        return RESULT_OK;
    }

    void FreeLoad(HQueue queue, HRequest request)
    {
        dmMutex::ScopedLock lk(queue->m_Mutex);

        uint32_t old_bytes_waiting = queue->m_BytesWaiting;

        uint32_t buffer_capacity = request->m_Buffer.Capacity();
        queue->m_BytesWaiting -= buffer_capacity;

        if (request->m_Result.m_IsBufferOwnershipTransferred)
        {
            // we reset the dmArray (size = 0, capacity = 0)
            memset((void*)&request->m_Buffer, 0, sizeof(request->m_Buffer));
        }

        // Make sure we don't copy any data if we reallocate the buffer
        request->m_Buffer.SetSize(0);

        // If we either have blocked further processing by exceeding MAX_PENDING_DATA or
        // the buffer has a non-default capacity, we want to wake up the worker
        if (buffer_capacity != DEFAULT_CAPACITY || (old_bytes_waiting >= MAX_PENDING_DATA && queue->m_BytesWaiting < MAX_PENDING_DATA))
        {
            // Wake up thread, we can now fit a new request
            dmConditionVariable::Signal(queue->m_WakeupCond);
        }

        // Clean up picked up requests
        request->m_Name          = 0x0;
        request->m_CanonicalPath = 0x0;

        while (queue->m_Back != queue->m_Loaded && queue->m_Request[queue->m_Back % QUEUE_SLOTS].m_Name == 0x0)
        {
            queue->m_Back++;
        }
    }
} // namespace dmLoadQueue
