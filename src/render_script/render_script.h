#ifndef DMENGINE_RENDER_SCRIPT_H
#define DMENGINE_RENDER_SCRIPT_H

#include <dlib/array.h>
#include <render/render.h>

namespace dmEngine
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

    struct RenderScriptInstance
    {
        HRenderScript       m_RenderScript;
        // TODO: This is needed since we are doing everything immediate atm, can probably be removed when that changes
        dmRender::RenderContext* m_RenderContext;
        int                 m_InstanceReference;
        int                 m_RenderScriptDataReference;
    };
    typedef RenderScriptInstance* HRenderScriptInstance;

    struct RenderScriptWorld
    {
        RenderScriptWorld();

        dmArray<RenderScriptInstance*> m_Instances;
    };

    void    InitializeRenderScript();
    void    FinalizeRenderScript();

    void    UpdateRenderScript();

    HRenderScript NewRenderScript(const void* buffer, uint32_t buffer_size, const char* filename);
    bool    ReloadRenderScript(HRenderScript render_script, const void* buffer, uint32_t buffer_size, const char* filename);
    void    DeleteRenderScript(HRenderScript render_script);

    HRenderScriptInstance   NewRenderScriptInstance(HRenderScript render_script, dmRender::RenderContext* render_context);
    void                    DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance);
}

#endif // DMENGINE_RENDER_SCRIPT_H
