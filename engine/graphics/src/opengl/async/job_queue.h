#ifndef H_GRAPHICS_OPENGL_JOBQUEUE
#define H_GRAPHICS_OPENGL_JOBQUEUE


namespace dmGraphics
{
    struct JobDesc
    {
        void* m_Context;
        void (*m_Func)(void* context);
        void (*m_FuncComplete)(void* context);
    };

    void JobQueueInitialize();
    void JobQueueFinalize();
    bool JobQueueIsAsync();

    void JobQueuePush(const JobDesc& request);
};

#endif // H_GRAPHICS_OPENGL_JOBQUEUE

