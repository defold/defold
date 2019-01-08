#include "resource.h"
#include "resource_private.h"
#include "load_queue.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/array.h>
#include <dlib/thread.h>
#include <dlib/mutex.h>
#include <dlib/time.h>

namespace dmLoadQueue
{
    // Implementation of dmLoadQueue with a thread that loads items in the order they are supplied,

    // Default to small buffers since a lot of what is loaded are just small objects anyway.
    // That way we can have more in flight, but throttle when max pending data grows too large anyway
    const uint64_t DEFAULT_CAPACITY = 8*1024;

    // Once the loader has this amount not picked up, it will stop loading more.
    // This sets the bandwidth of the loader.
    const uint64_t MAX_PENDING_DATA = 4*1024*1024;
    const uint32_t RESOURCE_PATH_MAX = 1024;
    const uint32_t QUEUE_SLOTS = 8;

    struct Request
    {
         char m_Name[RESOURCE_PATH_MAX];
         dmResource::LoadBufferType m_Buffer;
         PreloadInfo m_PreloadInfo;
         LoadResult m_Result;
    };

    struct Queue
    {
        dmResource::HFactory m_Factory;
        dmMutex::Mutex m_Mutex;
        dmThread::Thread m_Thread;
        Request m_Request[QUEUE_SLOTS];
        uint32_t m_Front, m_Back, m_Loaded;
        uint64_t m_BytesWaiting;
        bool m_Shutdown;

        // Circular queue with indexing as follow (exclusive end)
        //
        //          m_Back           m_Loaded   m_Front
        // [N/A]   [loaded] [loaded] [to-load]  [N/A]
        //
    };

    static void LoadThread(void *arg)
    {
        Queue* queue = (Queue*) arg;
        Request *current = 0;
        LoadResult result;
        while (true)
        {
            {
                dmMutex::ScopedLock lk(queue->m_Mutex);
                if (current != 0)
                {
                    // Just finished one (from previous iteratino)
                    queue->m_BytesWaiting += current->m_Buffer.Capacity();
                    queue->m_Loaded++;
                    current->m_Result = result;
                    current = 0;
                }
                if (queue->m_Shutdown)
                {
                    return;
                }

                // Since we can be loading many things at once, track the total Capacity() for buffers
                // that are waiting to be picked up by the preloader. In the case of the queue being filled
                // with only large requests (say only 4Mb textures), this throttles a bit so memory consumption
                // does not run away.
                if (queue->m_BytesWaiting < MAX_PENDING_DATA)
                {
                    if (queue->m_Loaded != queue->m_Front)
                    {
                        current = &queue->m_Request[queue->m_Loaded % QUEUE_SLOTS];
                    }
                }
            }

            if (current)
            {
                // We use the temporary result object here to fill in the data so it can be written with the mutex held.
                char canonical_path[RESOURCE_PATH_MAX];
                dmResource::GetCanonicalPath(current->m_Name, canonical_path);

                uint32_t size;

                current->m_Buffer.SetSize(0);
                result.m_LoadResult = DoLoadResource(queue->m_Factory, canonical_path, current->m_Name, &size, &current->m_Buffer);
                result.m_PreloadResult = dmResource::RESULT_PENDING;
                result.m_PreloadData = 0;

                if (result.m_LoadResult == dmResource::RESULT_OK)
                {
                    assert(current->m_Buffer.Size() == size);
                    if (current->m_PreloadInfo.m_Function)
                    {
                        dmResource::ResourcePreloadParams params;
                        params.m_Factory = queue->m_Factory;
                        params.m_Context = current->m_PreloadInfo.m_Context;
                        params.m_Buffer = current->m_Buffer.Begin();
                        params.m_BufferSize = current->m_Buffer.Size();
                        params.m_HintInfo = &current->m_PreloadInfo.m_HintInfo;
                        params.m_PreloadData = &result.m_PreloadData;
                        result.m_PreloadResult = current->m_PreloadInfo.m_Function(params);
                    }
                    else
                    {
                        result.m_PreloadResult = dmResource::RESULT_OK;
                    }
                }
            }
            else
            {
                dmTime::Sleep(1000);
                continue;
            }
        }
    }

    HQueue CreateQueue(dmResource::HFactory factory)
    {
        Queue *q = new Queue();
        q->m_Factory = factory;
        q->m_Front = 0;
        q->m_Back = 0;
        q->m_Loaded = 0;
        q->m_Shutdown = false;
        q->m_BytesWaiting = 0;
        q->m_Mutex = dmMutex::New();
        q->m_Thread = dmThread::New(&LoadThread, 65536, q, "AsyncLoad");
        return q;
    }

    void DeleteQueue(HQueue queue)
    {
        dmMutex::Lock(queue->m_Mutex);
        queue->m_Shutdown = true;
        dmMutex::Unlock(queue->m_Mutex);
        dmThread::Join(queue->m_Thread);
        dmMutex::Delete(queue->m_Mutex);
        delete queue;
    }

    HRequest BeginLoad(HQueue queue, const char* path, PreloadInfo* info)
    {
        dmMutex::ScopedLock lk(queue->m_Mutex);

        // Refuse more if full.
        if ((queue->m_Front - queue->m_Back) == QUEUE_SLOTS)
            return 0;

        if (strlen(path) >= RESOURCE_PATH_MAX)
        {
            dmLogWarning("Passed too long path into dmQueue::BeginLoad");
            return 0;
        }

        assert(path[0] != 0);

        Request *req = &queue->m_Request[(queue->m_Front++) % QUEUE_SLOTS];
        dmStrlCpy(req->m_Name, path, RESOURCE_PATH_MAX);

        req->m_PreloadInfo = *info;
        req->m_Result.m_LoadResult = dmResource::RESULT_PENDING;

        if (req->m_Buffer.Capacity() != DEFAULT_CAPACITY)
        {
            req->m_Buffer.SetCapacity(DEFAULT_CAPACITY);
        }

        return req;
    }

    Result EndLoad(HQueue queue, HRequest request, void** buf, uint32_t* size, LoadResult* load_result)
    {
        dmMutex::ScopedLock lk(queue->m_Mutex);
        if (request->m_Result.m_LoadResult == dmResource::RESULT_PENDING)
            return RESULT_PENDING;

        *buf = request->m_Buffer.Begin();
        *size = request->m_Buffer.Size();
        *load_result = request->m_Result;

        return RESULT_OK;
    }

    void FreeLoad(HQueue queue, HRequest request)
    {
        dmMutex::ScopedLock lk(queue->m_Mutex);

        queue->m_BytesWaiting -= request->m_Buffer.Capacity();

        if (request->m_Buffer.Capacity() > DEFAULT_CAPACITY)
        {
            request->m_Buffer.SetCapacity(DEFAULT_CAPACITY);
        }

        // Clean up picked up requests
        request->m_Name[0] = 0;

        while (queue->m_Back != queue->m_Loaded && !queue->m_Request[queue->m_Back % QUEUE_SLOTS].m_Name[0])
        {
            queue->m_Back++;
        }
    }
}

