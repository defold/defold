#ifndef DMRENDER_RENDER_SCRIPT_H
#define DMRENDER_RENDER_SCRIPT_H

#include <dlib/array.h>
#include <dlib/message.h>

#include <render/render.h>

namespace dmRender
{
    enum RenderScriptResult
    {
        RENDER_SCRIPT_RESULT_FAILED = -1,
        RENDER_SCRIPT_RESULT_NO_FUNCTION = 0,
        RENDER_SCRIPT_RESULT_OK = 1
    };

    enum RenderScriptFunction
    {
        RENDER_SCRIPT_FUNCTION_INIT,
        RENDER_SCRIPT_FUNCTION_UPDATE,
        RENDER_SCRIPT_FUNCTION_ONMESSAGE,
        MAX_RENDER_SCRIPT_FUNCTION_COUNT
    };

    extern const char* RENDER_SCRIPT_FUNCTION_NAMES[MAX_RENDER_SCRIPT_FUNCTION_COUNT];

    struct RenderScript
    {
        int m_FunctionReferences[MAX_RENDER_SCRIPT_FUNCTION_COUNT];
    };
    typedef RenderScript* HRenderScript;

    static const uint32_t MAX_PREDICATE_COUNT = 64;
    struct RenderScriptInstance
    {
        dmRender::Predicate* m_Predicates[MAX_PREDICATE_COUNT];
        uint32_t            m_PredicateCount;
        HRenderScript       m_RenderScript;
        // TODO: This is needed since we are doing everything immediate atm, can probably be removed when that changes
        dmRender::RenderContext* m_RenderContext;
        int                 m_InstanceReference;
        int                 m_RenderScriptDataReference;
    };
    typedef RenderScriptInstance* HRenderScriptInstance;

    struct Message
    {
        uint32_t m_Id;
        const dmDDF::Descriptor* m_DDFDescriptor;
        uint32_t m_BufferSize;
        void* m_Buffer;
    };

    void    InitializeRenderScript(dmMessage::DispatchCallback dispatch_callback);
    void    FinalizeRenderScript();

    HRenderScript NewRenderScript(const void* buffer, uint32_t buffer_size, const char* filename);
    bool    ReloadRenderScript(HRenderScript render_script, const void* buffer, uint32_t buffer_size, const char* filename);
    void    DeleteRenderScript(HRenderScript render_script);

    HRenderScriptInstance   NewRenderScriptInstance(HRenderScript render_script, dmRender::HRenderContext render_context);
    void                    DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      InitRenderScriptInstance(HRenderScriptInstance render_script_instance);
    RenderScriptResult      UpdateRenderScriptInstance(HRenderScriptInstance render_script_instance);
    void                    OnMessageRenderScriptInstance(HRenderScriptInstance render_script_instance, Message* message);
}

#endif // DMRENDER_RENDER_SCRIPT_H
