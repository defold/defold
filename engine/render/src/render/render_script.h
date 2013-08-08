#ifndef DMRENDER_RENDER_SCRIPT_H
#define DMRENDER_RENDER_SCRIPT_H

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>

#include <script/script.h>

#include "render.h"
#include "render_command.h"
#include "render_private.h"

namespace dmRender
{
    static const uint32_t MAX_PREDICATE_COUNT = 64;
    struct RenderScriptInstance
    {
        RenderScriptInstance();
        ~RenderScriptInstance();

        dmArray<Command>            m_CommandBuffer;
        dmHashTable64<HMaterial>    m_Materials;
        Predicate*                  m_Predicates[MAX_PREDICATE_COUNT];
        RenderContext*              m_RenderContext;
        HRenderScript               m_RenderScript;
        uint32_t                    m_PredicateCount;
        int                         m_InstanceReference;
        int                         m_RenderScriptDataReference;
    };

    void InitializeRenderScriptContext(RenderScriptContext& context, dmScript::HContext script_context, uint32_t command_buffer_size);
    void FinalizeRenderScriptContext(RenderScriptContext& context, dmScript::HContext script_context);
}

#endif // DMRENDER_RENDER_SCRIPT_H
