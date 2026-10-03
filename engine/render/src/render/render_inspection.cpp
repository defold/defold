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

#include <string.h>

#include "render_private.h"

namespace dmRender
{
    struct InspectionContext
    {
        dmGraphics::HRenderTarget           m_RenderTarget;
        dmHashTable32<InspectionProjection> m_Projections;
        dmHashTable64<InspectionProjection> m_ComponentProjections;
        bool                               m_Enabled;
    };

    InspectionContext* NewInspectionContext()
    {
        InspectionContext* context = new InspectionContext;
        context->m_RenderTarget = 0;
        context->m_Enabled = false;
        return context;
    }

    void DeleteInspectionContext(InspectionContext* context)
    {
        delete context;
    }

    void BeginInspectionFrame(HRenderContext context)
    {
        if (context->m_Inspection->m_Enabled)
        {
            context->m_Inspection->m_Projections.Clear();
            context->m_Inspection->m_ComponentProjections.Clear();
        }
    }

    void SetInspectionRenderTarget(HRenderContext context, dmGraphics::HRenderTarget target)
    {
        context->m_Inspection->m_RenderTarget = target;
    }

    void EnableInspection(HRenderContext context, bool enable)
    {
        if (context->m_Inspection->m_Enabled != enable)
        {
            context->m_Inspection->m_Projections.Clear();
            context->m_Inspection->m_ComponentProjections.Clear();
        }
        context->m_Inspection->m_Enabled = enable;
    }

    bool IsInspectionEnabled(HRenderContext context)
    {
        return context->m_Inspection->m_Enabled;
    }

    void GetInspectionProjection(HRenderContext context, uint32_t material_tag, InspectionProjection* projection)
    {
        InspectionProjection* current = context->m_Inspection->m_Projections.Get(material_tag);
        if (current) *projection = *current;
        else memset(projection, 0, sizeof(*projection));
    }

    void GetComponentInspectionProjection(HRenderContext context, const void* component, InspectionProjection* projection)
    {
        InspectionProjection* current = context->m_Inspection->m_ComponentProjections.Get((uintptr_t)component);
        if (current) *projection = *current;
        else memset(projection, 0, sizeof(*projection));
    }

    static void AccumulateInspectionProjection(HRenderContext context, InspectionProjection* projection)
    {
        if (projection->m_State == 2) return;
        if (context->m_Inspection->m_RenderTarget || dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_NULL)
        {
            projection->m_State = 2;
            return;
        }
        InspectionProjection current = {};
        current.m_ViewProjection = context->m_ViewProj;
        dmGraphics::GetViewport(context->m_GraphicsContext, &current.m_ViewportX, &current.m_ViewportY,
                               &current.m_ViewportWidth, &current.m_ViewportHeight);
        current.m_State = 1;
        if (projection->m_State && (memcmp(&current.m_ViewProjection, &projection->m_ViewProjection, sizeof(dmVMath::Matrix4)) ||
            current.m_ViewportX != projection->m_ViewportX || current.m_ViewportY != projection->m_ViewportY ||
            current.m_ViewportWidth != projection->m_ViewportWidth || current.m_ViewportHeight != projection->m_ViewportHeight))
            projection->m_State = 2;
        else
            *projection = current;
    }

    void RecordInspectionProjection(HRenderContext context, const void* component)
    {
        if (!context->m_Inspection->m_Enabled) return;
        InspectionProjection* projection = context->m_Inspection->m_ComponentProjections.Get((uintptr_t)component);
        if (projection)
        {
            AccumulateInspectionProjection(context, projection);
            return;
        }
        InspectionProjection current = {};
        AccumulateInspectionProjection(context, &current);
        if (context->m_Inspection->m_ComponentProjections.Full())
        {
            uint32_t capacity = context->m_Inspection->m_ComponentProjections.Capacity() + 32;
            context->m_Inspection->m_ComponentProjections.SetCapacity(capacity * 2, capacity);
        }
        context->m_Inspection->m_ComponentProjections.Put((uintptr_t)component, current);
    }

    void RecordMaterialInspectionProjection(HRenderContext context, uint32_t tag)
    {
        InspectionProjection* projection = context->m_Inspection->m_Projections.Get(tag);
        if (projection)
        {
            AccumulateInspectionProjection(context, projection);
            return;
        }
        InspectionProjection current = {};
        AccumulateInspectionProjection(context, &current);
        if (context->m_Inspection->m_Projections.Full())
        {
            uint32_t capacity = context->m_Inspection->m_Projections.Capacity() + 32;
            context->m_Inspection->m_Projections.SetCapacity(capacity * 2, capacity);
        }
        context->m_Inspection->m_Projections.Put(tag, current);
    }
}
