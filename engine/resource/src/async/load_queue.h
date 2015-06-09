#ifndef RESOURCE_LOAD_QUEUE
#define RESOURCE_LOAD_QUEUE

#include "resource.h"
#include "resource_private.h"

namespace dmLoadQueue
{
    // The following interface is used by resource.cpp to perform actual loading of files.
    // It is not exposed to the outside world.
    enum Result
    {
        RESULT_OK                 =  0,
        RESULT_PENDING            = -1,
        RESULT_INVALID_PARAM      = -2
    };

    typedef struct Queue* HQueue;
    typedef struct Request* HRequest;

    struct PreloadInfo
    {
        dmResource::FResourcePreload m_Function;
        dmResource::PreloadHintInfo m_HintInfo;
        void* m_Context;
    };

    struct LoadResult
    {
        dmResource::Result m_LoadResult;
        dmResource::Result m_PreloadResult;
        void* m_PreloadData;
    };

    HQueue CreateQueue(dmResource::HFactory factory);
    void DeleteQueue(HQueue queue);

    // If the queue does not want to accept any more requests at the moment, it returns 0
    HRequest BeginLoad(HQueue queue, const char* path, PreloadInfo* info);

    // Actual load result will be put in load_result. Ptrs can be handled until FreeLoad has been called.
    Result EndLoad(HQueue queue, HRequest request, void** buf, uint32_t* size, LoadResult* load_result);

    // Free once completed.
    void FreeLoad(HQueue queue, HRequest request);
}

#endif
