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

#ifndef DMRENDER_RENDER_SCRIPT_H
#define DMRENDER_RENDER_SCRIPT_H

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>

#include <graphics/graphics.h>
#include <script/script.h>

#include "render.h"
#include "render_command.h"
#include "render_private.h"

namespace dmRender
{
    enum RenderScriptFunction
    {
        RENDER_SCRIPT_FUNCTION_INIT,
        RENDER_SCRIPT_FUNCTION_UPDATE,
        RENDER_SCRIPT_FUNCTION_ONMESSAGE,
        RENDER_SCRIPT_FUNCTION_ONRELOAD,
        MAX_RENDER_SCRIPT_FUNCTION_COUNT
    };

    struct RenderScript
    {
        int             m_FunctionReferences[MAX_RENDER_SCRIPT_FUNCTION_COUNT];
        RenderContext*  m_RenderContext;
        const char*     m_SourceFileName;
        int             m_InstanceReference;
    };

    struct RenderResource
    {
        // Some types need an asset handle of 64-bit, and some needs a pointer
        // so this struct needs to handle both for 32-bit systems
        uint64_t           m_Resource;
        RenderResourceType m_Type;
    };

    static const uint32_t MAX_PREDICATE_COUNT = 64;
    struct RenderScriptInstance
    {
        dmArray<Command>              m_CommandBuffer;
        dmHashTable64<RenderResource> m_RenderResources;
        Predicate*                    m_Predicates[MAX_PREDICATE_COUNT];
        RenderContext*                m_RenderContext;
        HRenderScript                 m_RenderScript;
        dmScript::ScriptWorld*        m_ScriptWorld;
        uint32_t                      m_PredicateCount;
        uint32_t                      m_UniqueScriptId;
        int                           m_InstanceReference;
        int                           m_RenderScriptDataReference;
        int                           m_ContextTableReference;
    };

    void InitializeRenderScriptContext(RenderScriptContext& context, dmGraphics::HContext graphics_context, dmScript::HContext script_context, uint32_t command_buffer_size);
    void FinalizeRenderScriptContext(RenderScriptContext& context, dmScript::HContext script_context);

    void InitializeRenderScriptCameraContext(HRenderContext render_context, dmScript::HContext script_context);
    void FinalizeRenderScriptCameraContext(HRenderContext render_context);
}

#endif // DMRENDER_RENDER_SCRIPT_H
