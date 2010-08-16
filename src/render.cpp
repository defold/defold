#include <assert.h>
#include <dlib/hash.h>
#include <dlib/profile.h>
#include <graphics/graphics_device.h>
#include "renderinternal.h"
#include "model/model.h"


#include "rendertypes/rendertypemodel.h"

namespace dmRender
{
	struct RenderPass
	{
		RenderPassDesc			m_Desc;
        dmArray<RenderObject*>  m_InstanceList[2];
		uint16_t				m_InUse:1;
	};

    struct RenderWorld
    {
        dmArray<RenderObject*>  m_RenderObjectInstanceList;
        dmArray<RenderPass*>  	m_RenderPasses;
        RenderContext           m_RenderContext;
        uint16_t				m_RenderBuffer;

        SetObjectModel			m_SetObjectModel;
    };

    RenderWorld* m_RenderWorld;

    void FrameBeginDraw(RenderContext* rendercontext)
    {
    }
    void FrameEndDraw(RenderContext* rendercontext)
    {
    }

    void RenderPassBegin(RenderContext* rendercontext, RenderPass* rp)
    {
    }
    void RenderPassEnd(RenderContext* rendercontext, RenderPass* rp)
    {
    	rp->m_InstanceList[m_RenderWorld->m_RenderBuffer].SetSize(0);
    }

    void RenderTypeBegin(RenderContext* rendercontext, RenderObjectType type)
    {
    	switch (type)
    	{
    		case RENDEROBJECT_TYPE_MODEL:
    		{
    			RenderTypeModelSetup(rendercontext);
    			break;
    		}
    		default:
    		{
    			break;
    		}
    	}
    }

    void RenderTypeEnd(RenderContext* rendercontext, RenderObjectType type)
    {

    }

    void rptestBegin(const RenderContext* rendercontext, const void* userdata)
    {
    }

    void rptestEnd(const RenderContext* rendercontext, const void* userdata)
    {
    }

    HRenderPass testrp;


    void Initialize(uint32_t max_renderobjects, uint32_t max_renderpasses, SetObjectModel set_object_model)
    {
    	assert(max_renderpasses > 1);
    	m_RenderWorld = new RenderWorld;
    	m_RenderWorld->m_RenderBuffer = 1;
    	m_RenderWorld->m_RenderObjectInstanceList.SetCapacity(max_renderobjects);
    	m_RenderWorld->m_RenderPasses.SetCapacity(max_renderpasses);
    	m_RenderWorld->m_SetObjectModel = set_object_model;

        RenderPassDesc rp("rptest", 0x0, 0, 100, rptestBegin, rptestEnd);
        testrp = NewRenderPass(&rp);
    }

    void Finalize()
    {
    	delete m_RenderWorld;
    }

    void UpdateContext(RenderContext* rendercontext)
    {
        m_RenderWorld->m_RenderContext = *rendercontext;
    }

    static void UpdateDeletedInstances()
    {
    	uint32_t size = m_RenderWorld->m_RenderObjectInstanceList.Size();
    	RenderObject** mem = new RenderObject*[size+1];

    	dmArray<RenderObject*> temp_list(mem, 0, size+1);

    	for (uint32_t i=0; i<size; i++)
    	{
   			RenderObject* ro = m_RenderWorld->m_RenderObjectInstanceList[i];
    		if (ro->m_MarkForDelete)
    		{
    	    	delete ro;
    		}
    		else
    		{
    			temp_list.Push(ro);
    		}
    	}

    	size = temp_list.Size();
    	m_RenderWorld->m_RenderObjectInstanceList.SetSize(size);
    	for (uint32_t i=0; i<size; i++)
    	{
    		m_RenderWorld->m_RenderObjectInstanceList[i] = temp_list[i];
    	}

        delete [] mem;
    }


    void Update()
    {
        DM_PROFILE(Render, "Update");

    	UpdateDeletedInstances();


    	for (uint32_t i=0; i<m_RenderWorld->m_RenderObjectInstanceList.Size(); i++)
        {
			RenderObject* ro = m_RenderWorld->m_RenderObjectInstanceList[i];
			assert(!ro->m_MarkForDelete);

    		AddRenderObject(testrp, ro);

        }


    	m_RenderWorld->m_RenderBuffer = 1 - m_RenderWorld->m_RenderBuffer;
    	FrameBeginDraw(&m_RenderWorld->m_RenderContext);

    	for (uint32_t pass=0; pass<m_RenderWorld->m_RenderPasses.Size(); pass++)
    	{
    		RenderPass* rp = m_RenderWorld->m_RenderPasses[pass];
    		RenderPassBegin(&m_RenderWorld->m_RenderContext, rp);

    		if (rp->m_Desc.m_BeginFunc)
    			rp->m_Desc.m_BeginFunc(&m_RenderWorld->m_RenderContext, 0x0);


    		// array is now a list of instances for the active render pass
    		dmArray<RenderObject*> *array = &rp->m_InstanceList[m_RenderWorld->m_RenderBuffer];
    		if (array->Size())
    		{
    			int old_type = -1;
				RenderObject** rolist = &array->Front();
				for (uint32_t e=0; e < array->Size(); e++, rolist++)
				{
					RenderObject* ro = *rolist;
					if (!ro->m_Enabled)
					    continue;

					// check if we need to change render type and run its setup func
					if (old_type != (int)ro->m_Type)
					{
						// change type
						RenderTypeBegin(&m_RenderWorld->m_RenderContext, ro->m_Type);
					}
					switch (ro->m_Type)
					{
						case RENDEROBJECT_TYPE_MODEL:
						{
							m_RenderWorld->m_SetObjectModel(0x0, ro->m_Go, &ro->m_Rot, &ro->m_Pos);
							RenderTypeModelDraw(&m_RenderWorld->m_RenderContext, ro);
							break;
						}
						default:
						{
							break;
						}
					}

					if (old_type != (int)ro->m_Type)
					{
						RenderTypeEnd(&m_RenderWorld->m_RenderContext, ro->m_Type);
						old_type = ro->m_Type;
					}

				}
    		}


    		if (rp->m_Desc.m_EndFunc)
    			rp->m_Desc.m_EndFunc(&m_RenderWorld->m_RenderContext, 0x0);

    		RenderPassEnd(&m_RenderWorld->m_RenderContext, rp);
    	}

    	FrameEndDraw(&m_RenderWorld->m_RenderContext);

    	return;

    }

    HRenderObject NewRenderObjectInstance(void* resource, void* go, RenderObjectType type)
    {
    	RenderObject* ro = new RenderObject;
    	ro->m_Data = resource;
    	ro->m_Type = type;
    	ro->m_Go = go;
    	ro->m_MarkForDelete = 0;
    	ro->m_Enabled = 1;

    	if (type == RENDEROBJECT_TYPE_MODEL)
    	{
    		dmModel::Model* model = (dmModel::Model*)resource;
    		uint32_t reg;

    		reg = DIFFUSE_COLOR;
    		ro->m_Colour[reg] = dmGraphics::GetMaterialFragmentProgramConstant(dmModel::GetMaterial(model), reg);
            reg = EMISSIVE_COLOR;
            ro->m_Colour[reg] = dmGraphics::GetMaterialFragmentProgramConstant(dmModel::GetMaterial(model), reg);
            reg = SPECULAR_COLOR;
            ro->m_Colour[reg] = dmGraphics::GetMaterialFragmentProgramConstant(dmModel::GetMaterial(model), reg);
    	}


    	m_RenderWorld->m_RenderObjectInstanceList.Push(ro);
    	return ro;
    }

    void Disable(HRenderObject ro)
    {
        ro->m_Enabled = 0;
    }
    void Enable(HRenderObject ro)
    {
        ro->m_Enabled = 1;

    }
    bool IsEnabled(HRenderObject ro)
    {
        return ro->m_Enabled == true;
    }

    void DeleteRenderObject(HRenderObject ro)
    {
    	// mark for delete
    	ro->m_MarkForDelete = 1;
    }

    void SetPosition(HRenderObject ro, Vector4 pos)
    {
    	// double buffering
    }
    void SetRotation(HRenderObject ro, Quat rot)
    {
    	// double buffering
    }

    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type)
    {
        ro->m_Colour[color_type] = color;
    }


    HRenderPass NewRenderPass(RenderPassDesc* desc)
    {
    	RenderPass* rp = new RenderPass;
    	m_RenderWorld->m_RenderPasses.Push(rp);

    	memcpy((void*)&rp->m_Desc, (void*)desc, sizeof(RenderPassDesc) );
    	rp->m_InUse = 1;

    	rp->m_InstanceList[0].SetCapacity(desc->m_Capacity);
    	rp->m_InstanceList[1].SetCapacity(desc->m_Capacity);

    	return rp;
    }

    void DeleteRenderPass(HRenderPass renderpass)
    {
        delete renderpass;
    }

    void AddRenderObject(HRenderPass renderpass, HRenderObject renderobject)
    {
    	renderpass->m_InstanceList[!m_RenderWorld->m_RenderBuffer].Push(renderobject);
    }

}
