#include "resource.h"
#include "resource_private.h"
#include "load_queue.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

namespace dmLoadQueue
{
    // Implementation of the LoadQueue API where all loads happen during the EndLoad call.
    // EndLoad thus never returns _PENDING
    const uint32_t RESOURCE_PATH_MAX = 1024;

    struct Request
    {
         char m_Name[RESOURCE_PATH_MAX];
         PreloadInfo m_PreloadInfo;
    };

    struct Queue
    {
        dmResource::HFactory m_Factory;
        Request m_SingleBuffer;
        Request* m_ActiveRequest;
    };

    HQueue CreateQueue(dmResource::HFactory factory)
    {
        Queue *q = new Queue();
        q->m_ActiveRequest = 0;
        q->m_Factory = factory;
        return q;
    }

    void DeleteQueue(HQueue queue)
    {
        delete queue;
    }

    HRequest BeginLoad(HQueue queue, const char* path, PreloadInfo* info)
    {
        if (queue->m_ActiveRequest != 0)
        {
            return 0;
        }

        if (strlen(path) >= RESOURCE_PATH_MAX)
        {
            dmLogWarning("Passed too long path into dmQueue::BeginLoad");
            return 0;
        }

        queue->m_ActiveRequest = &queue->m_SingleBuffer;
        dmStrlCpy(queue->m_ActiveRequest->m_Name, path, RESOURCE_PATH_MAX);
        queue->m_ActiveRequest->m_PreloadInfo = *info;
        return queue->m_ActiveRequest;
    }

    Result EndLoad(HQueue queue, HRequest request, void** buf, uint32_t* size, LoadResult* load_result)
    {
        if (!queue || !request || queue->m_ActiveRequest != request)
        {
            return RESULT_INVALID_PARAM;
        }

        char canonical_path[RESOURCE_PATH_MAX];
        GetCanonicalPath(request->m_Name, canonical_path);

        //
        load_result->m_LoadResult = dmResource::LoadResource(queue->m_Factory, canonical_path, request->m_Name, buf, size);
        load_result->m_PreloadResult = dmResource::RESULT_PENDING;
        load_result->m_PreloadData = 0;

        if (load_result->m_LoadResult == dmResource::RESULT_OK && request->m_PreloadInfo.m_Function)
        {
            dmResource::ResourcePreloadParams params;
            params.m_Factory = queue->m_Factory;
            params.m_Context = request->m_PreloadInfo.m_Context;
            params.m_Buffer = *buf;
            params.m_BufferSize = *size;
            params.m_HintInfo = &request->m_PreloadInfo.m_HintInfo;
            params.m_PreloadData = &load_result->m_PreloadData;
            load_result->m_PreloadResult = request->m_PreloadInfo.m_Function(params);
        }
        return RESULT_OK;
    }

    void FreeLoad(HQueue queue, HRequest request)
    {
        queue->m_ActiveRequest = 0;
    }
}
