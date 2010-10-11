#include <assert.h>
#include <dlib/hash.h>
#include <dlib/profile.h>
#include <graphics/graphics_device.h>
#include "renderinternal.h"
#include "model/model.h"


#include "rendertypes/rendertypemodel.h"
#include "rendertypes/rendertypetext.h"
#include "rendertypes/rendertypeparticle.h"

namespace dmRender
{
    struct RenderPass
    {
        RenderPassDesc          m_Desc;
        RenderContext           m_RenderContext;
        dmArray<RenderObject*>  m_InstanceList[2];
        uint16_t                m_RenderBuffer;
        uint16_t                m_InUse:1;
    };

    struct RenderWorld
    {
        struct Renderer
        {
            RenderTypeInstanceFunc  m_InstanceFunc;
            RenderTypeSetupFunc     m_SetupFunc;
        };

        dmArray<RenderObject*>  m_RenderObjectInstanceList;
        dmArray<RenderPass*>  	m_RenderPasses;
        dmArray<Renderer>       m_RenderTypeList;

        RenderContext           m_RenderContext;

        SetObjectModel			m_SetObjectModel;
    };



    void FrameBeginDraw(RenderContext* rendercontext)
    {
    }
    void FrameEndDraw(RenderContext* rendercontext)
    {
    }

    void RenderPassBegin(RenderPass* rp)
    {
        rp->m_RenderBuffer = 1 - rp->m_RenderBuffer;
    }

    void RenderPassEnd(RenderPass* rp)
    {
        rp->m_InstanceList[rp->m_RenderBuffer].SetSize(0);
    }


    void RenderTypeBegin(RenderWorld* world, RenderContext* rendercontext, uint32_t type)
    {
        world->m_RenderTypeList[type].m_SetupFunc(rendercontext);
    }

    void RenderTypeEnd(RenderWorld* world, RenderContext* rendercontext, uint32_t type)
    {

    }

    void rptestBegin(const RenderContext* rendercontext, const void* userdata)
    {
    }

    void rptestEnd(const RenderContext* rendercontext, const void* userdata)
    {
    }



    HRenderWorld NewRenderWorld(uint32_t max_instances, uint32_t max_renderpasses, SetObjectModel set_object_model)
    {
        RenderWorld* world = new RenderWorld;

        world->m_RenderObjectInstanceList.SetCapacity(max_instances);
        world->m_RenderObjectInstanceList.SetSize(0);
        world->m_RenderPasses.SetCapacity(max_renderpasses);
        world->m_RenderPasses.SetSize(0);

        const uint32_t max_renderers = 100;
        world->m_RenderTypeList.SetCapacity(max_renderers);
        world->m_RenderTypeList.SetSize(max_renderers);

        world->m_SetObjectModel = set_object_model;
        world->m_RenderContext.m_GFXContext = dmGraphics::GetContext();

        RegisterRenderer(world, RENDEROBJECT_TYPE_MODEL,    RenderTypeModelSetup,       RenderTypeModelDraw);
        RegisterRenderer(world, RENDEROBJECT_TYPE_TEXT,     RenderTypeTextSetup,        RenderTypeTextDraw);
        RegisterRenderer(world, RENDEROBJECT_TYPE_PARTICLE, RenderTypeParticleSetup,    RenderTypeParticleDraw);

        return world;
    }

    // this needs to go...
    static void UpdateDeletedInstances(HRenderWorld world)
    {
        uint32_t size = world->m_RenderObjectInstanceList.Size();
        RenderObject** mem = new RenderObject*[size+1];

        dmArray<RenderObject*> temp_list(mem, 0, size+1);

        for (uint32_t i=0; i<size; i++)
        {
            RenderObject* ro = world->m_RenderObjectInstanceList[i];
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
        world->m_RenderObjectInstanceList.SetSize(size);
        for (uint32_t i=0; i<size; i++)
        {
            world->m_RenderObjectInstanceList[i] = temp_list[i];
        }

        delete [] mem;
    }


    void DeleteRenderWorld(HRenderWorld world)
    {
        UpdateDeletedInstances(world);
        delete world;
    }

    void RegisterRenderer(HRenderWorld world, uint32_t type, RenderTypeSetupFunc rendertype_setup, RenderTypeInstanceFunc rendertype_instance)
    {
        RenderWorld::Renderer renderer;
        renderer.m_InstanceFunc = rendertype_instance;
        renderer.m_SetupFunc = rendertype_setup;

        world->m_RenderTypeList[type] = renderer;
    }

    void AddRenderPass(HRenderWorld world, HRenderPass renderpass)
    {
        world->m_RenderPasses.Push(renderpass);
    }



    void UpdateContext(HRenderWorld world, RenderContext* rendercontext)
    {
        world->m_RenderContext = *rendercontext;
    }



    void AddToRender(HRenderWorld local_world, HRenderWorld world)
    {
        UpdateDeletedInstances(local_world);

        for (uint32_t i=0; i<local_world->m_RenderObjectInstanceList.Size(); i++)
        {
            RenderObject* ro = local_world->m_RenderObjectInstanceList[i];
            assert(ro->m_MarkForDelete == 0);

            for (uint32_t j=0; j<world->m_RenderPasses.Size(); j++)
            {
                RenderPass* rp = world->m_RenderPasses[j];
                if (rp->m_Desc.m_Predicate & ro->m_Mask)
                {
                    if (local_world->m_SetObjectModel)
                        local_world->m_SetObjectModel(0x0, ro->m_Go, &ro->m_Rot, &ro->m_Pos);
                    AddRenderObject(rp, ro);
                }
            }
        }
    }



    void Update(HRenderWorld world, float dt)
    {
        DM_PROFILE(Render, "Update");

    	FrameBeginDraw(&world->m_RenderContext);

    	for (uint32_t pass=0; pass<world->m_RenderPasses.Size(); pass++)
    	{
    		RenderPass* rp = world->m_RenderPasses[pass];
    		RenderPassBegin(rp);

    		if (rp->m_Desc.m_BeginFunc)
    			rp->m_Desc.m_BeginFunc(&world->m_RenderContext, 0x0);


    		// array is now a list of instances for the active render pass
    		dmArray<RenderObject*> *array = &rp->m_InstanceList[rp->m_RenderBuffer];
    		if (array->Size())
    		{
    			int old_type = -1;
				RenderObject** rolist = &array->Front();
				for (uint32_t e=0; e < array->Size(); e++, rolist++)
				{
					RenderObject* ro = *rolist;
					if (!ro->m_Enabled)
					    continue;

					// ro's can be marked for delete between this code and being added for rendering
					if (ro->m_MarkForDelete)
					    continue;

					// check if we need to change render type and run its setup func
					if (old_type != (int)ro->m_Type)
					{
						// change type
						RenderTypeBegin(world, &rp->m_RenderContext, ro->m_Type);
					}

					// dispatch
					world->m_RenderTypeList[ro->m_Type].m_InstanceFunc(&rp->m_RenderContext, &ro, 1);

					if (old_type != (int)ro->m_Type)
					{
						RenderTypeEnd(world, &rp->m_RenderContext, ro->m_Type);
						old_type = ro->m_Type;
					}

				}
    		}


    		if (rp->m_Desc.m_EndFunc)
    			rp->m_Desc.m_EndFunc(&world->m_RenderContext, 0x0);

    		RenderPassEnd(rp);
    	}

    	FrameEndDraw(&world->m_RenderContext);

    	world->m_RenderObjectInstanceList.SetSize(0);
    	return;

    }

    HRenderObject NewRenderObject(HRenderWorld world, void* resource, void* go, uint64_t mask, uint32_t type)
    {
    	RenderObject* ro = new RenderObject;
    	ro->m_Data = resource;
    	ro->m_Type = type;
    	ro->m_Go = go;
    	ro->m_MarkForDelete = 0;
    	ro->m_Enabled = 1;
    	ro->m_Mask = mask;

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


    	world->m_RenderObjectInstanceList.Push(ro);
    	return ro;
    }

    void SetData(HRenderObject ro, void* data)
    {
        ro->m_Data = data;
    }

    void SetGameObject(HRenderObject ro, void* go)
    {
        ro->m_Go = go;
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

    void DeleteRenderObject(HRenderWorld world, HRenderObject ro)
    {
    	// mark for delete
    	ro->m_MarkForDelete = 1;
    }

    void SetPosition(HRenderObject ro, Point3 pos)
    {
        ro->m_Pos = pos;
    }
    void SetRotation(HRenderObject ro, Quat rot)
    {
        ro->m_Rot = rot;
    }

    void SetSize(HRenderObject ro, Vector3 size)
    {
        ro->m_Size = size;
    }




    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type)
    {
        ro->m_Colour[color_type] = color;
    }
    void* GetData(HRenderObject ro)
    {
        return ro->m_Data;
    }

    Point3 GetPosition(HRenderObject ro)
    {
        return ro->m_Pos;
    }

    Quat GetRotation(HRenderObject ro)
    {
        return ro->m_Rot;
    }
    Vector4 GetColor(HRenderObject ro, ColorType color_type)
    {
        return ro->m_Colour[color_type];
    }
    Vector3 GetSize(HRenderObject ro)
    {
        return ro->m_Size;
    }


    void SetViewMatrix(HRenderPass renderpass, Matrix4* viewmatrix)
    {
        renderpass->m_RenderContext.m_View = *viewmatrix;
    }

    void SetViewProjectionMatrix(HRenderPass renderpass, Matrix4* viewprojectionmatrix)
    {
        renderpass->m_RenderContext.m_ViewProj = *viewprojectionmatrix;
    }


    HRenderPass NewRenderPass(RenderPassDesc* desc)
    {
        RenderPass* rp = new RenderPass;

        memcpy((void*)&rp->m_Desc, (void*)desc, sizeof(RenderPassDesc) );
        rp->m_RenderBuffer = 1;
        rp->m_InUse = 1;

        rp->m_InstanceList[0].SetCapacity(desc->m_Capacity);
        rp->m_InstanceList[1].SetCapacity(desc->m_Capacity);

        // separate context per renderpass possibly?
        rp->m_RenderContext.m_GFXContext = dmGraphics::GetContext();
        return rp;
    }

    void DeleteRenderPass(HRenderPass renderpass)
    {
        delete renderpass;
    }

    void DisableRenderPass(HRenderPass renderpass)
    {

    }

    void EnableRenderPass(HRenderPass renderpass)
    {

    }

    void AddRenderObject(HRenderPass renderpass, HRenderObject renderobject)
    {
        renderpass->m_InstanceList[!renderpass->m_RenderBuffer].Push(renderobject);
    }

}
