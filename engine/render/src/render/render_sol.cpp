#include <assert.h>
#include <string.h>
#include <float.h>

#include <ddf/ddf.h>
#include <dlib/log.h>

#include "render_private.h"
#include "render.h"

namespace dmRender
{
    extern "C"
    {
        dmGraphics::HContext SolGetGraphicsContext(HRenderContext render_context)
        {
            return dmRender::GetGraphicsContext(render_context);
        }
        
        struct RenderListAllocation
        {
            RenderListEntry* entries;
            uint32_t begin;
            uint32_t end;
        };
        
        void SolRenderListAlloc(HRenderContext context, uint32_t entries, RenderListAllocation* result)
        {
            // Array pointers need to point to the first element so do it this way.
            RenderListEntry* cursor = RenderListAlloc(context, entries);
            result->entries = context->m_RenderList.Begin();
            result->begin = cursor - result->entries;
            result->end = result->begin + entries;
            memset(&result->entries[result->begin], 0x00, entries * sizeof(RenderListEntry));
        }
        
        void SolRenderListSubmit(HRenderContext render_context, RenderListAllocation* allocation, uint32_t end)
        {
            // Validate
            RenderListEntry *i = allocation->entries + allocation->begin;
            RenderListEntry *e = allocation->entries + allocation->end;
            while (i != e)
            {
                if (i->m_Dispatch >= render_context->m_RenderListDispatch.Size())
                {
                    dmLogError("Sol component generated invalid dispatch id, beyond range of allocated %d/%d.", i->m_Dispatch, render_context->m_RenderListDispatch.Size());
                    return;
                }
                else if (!render_context->m_RenderListDispatch[i->m_Dispatch].m_SolFn)
                {
                    dmLogError("Sol component generated render list entry with non-sol dispatch %d.", i->m_Dispatch)
                    return;
                }
                ++i;
            }
            RenderListSubmit(render_context, allocation->entries + allocation->begin, allocation->entries + end);
        }
        
        void SolRenderListEnd(HRenderContext render_context)
        {
            RenderListEnd(render_context);
        }

        uint32_t SolRenderGetMaterialTagMask(HMaterial material)
        {
            Material* mat = (Material*) material;
            if (!mat->m_Valid)
            {
                dmLogError("Invalid material in arguments");
                return 0;
            }
            return GetMaterialTagMask(material);
        }
        
        void SolRenderAddToRender(HRenderContext context, RenderObject* obj)
        {
            runtime_pin(obj);
            context->m_RenderObjectsSol.Push(obj);
            AddToRender(context, obj);
        }
        
        HRenderListDispatch SolRenderCreateDispatch(HRenderContext context, RenderListDispatchFnSol fn, ::Any any)
        {
            return RenderListMakeDispatchSol(context, fn, any);
        }
        
        void SolEnableRenderObjectConstant(RenderObject* ro, dmhash_t name_hash, const Vectormath::Aos::Vector4* value)
        {
            if (ro && value)
            {
                EnableRenderObjectConstant(ro, name_hash, *value);
            }
        }
        
        void SolDisableRenderObjectConstant(RenderObject* ro, dmhash_t name_hash)
        {
            if (ro)
            {
                DisableRenderObjectConstant(ro, name_hash);
            }
        }
        
        struct SolMaterialProgramConstantInfo
        {
            dmhash_t constant_id;
            dmhash_t element_ids[4];
            uint32_t element_index;
        };
         
        bool SolGetMaterialProgramConstantInfo(HMaterial material, dmhash_t name_hash, SolMaterialProgramConstantInfo* info)
        {
            if (!material || !material->m_Valid || !info)
            {
                dmLogWarning("Invalid values passed to GetMaterialProgramConstantInfo");
                return false;
            }
            
            memset(info, 0x00, sizeof(*info));
            dmhash_t* ids = 0;
            bool ret = GetMaterialProgramConstantInfo(material, name_hash, &info->constant_id, &ids, &info->element_index);
            if (ret && ids)
            {
                for (int i=0;i<4;i++)
                    info->element_ids[i] = ids[i];
            }
            return ret;
        }
        
        bool SolGetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, Constant* out_value)
        {
            if (!material || !material->m_Valid || !out_value)
            {
                dmLogError("Inavlid values passed to GetMaterialProgramConstant");
                return false;
            }
            return GetMaterialProgramConstant(material, name_hash, *out_value);
        }
        
        int32_t SolGetMaterialConstantLocation(HMaterial material, dmhash_t name_hash)
        {
            if (!material || !material->m_Valid)
            {
                return -1;
            }
            return GetMaterialConstantLocation(material, name_hash);
        }
    }
}
 