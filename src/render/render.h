#ifndef RENDER_H
#define RENDER_H

#include <string.h>
#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/container.h>
#include <graphics/graphics_device.h>

namespace dmRender
{

	using namespace Vectormath::Aos;

	enum RenderObjectType
	{
		RENDEROBJECT_TYPE_MODEL = 0,
		RENDEROBJECT_TYPE_PARTICLE = 1,
		RENDEROBJECT_TYPE_TEXT = 2,
		RENDEROBJECT_TYPE_MAX
	};

    enum ColorType
    {
        DIFFUSE_COLOR = 0,
        EMISSIVE_COLOR = 1,
        SPECULAR_COLOR = 2,
        GENERIC = 3,
        MAX_COLOR
    };

    struct RenderContext
    {
        Matrix4                     m_View;
        Matrix4                     m_Projection;
        Matrix4                     m_ViewProj;
        Point3                      m_CameraPosition;

        dmGraphics::HContext        m_GFXContext;
    };

    typedef struct RenderObject* HRenderObject;
    typedef struct RenderWorld* HRenderWorld;
    typedef struct RenderPass* HRenderPass;
    typedef void (*SetObjectModel)(void* context, void* gameobject, Quat* rotation, Point3* position);

    typedef void (*RenderTypeInstanceFunc)(const RenderContext* rendercontext, const HRenderObject* ro, uint32_t count);
    typedef void (*RenderTypeSetupFunc)(const RenderContext* rendercontext);



    struct RenderPassDesc
    {
        RenderPassDesc(){}
        RenderPassDesc(const char* name, void* userdata, uint32_t index, uint32_t capacity, uint64_t predicate, void (*beginfunc)(const dmRender::RenderContext* rendercontext, const void* userdata), void (*endfunc)(const dmRender::RenderContext* rendercontext, const void* userdata))
        {
            strncpy(m_Name, name, sizeof(m_Name));
            m_Predicate = predicate;
            m_UserData = userdata;
            m_Index = index;
            m_Capacity = capacity;
            m_BeginFunc = beginfunc;
            m_EndFunc = endfunc;
        }

        char        m_Name[128];
        uint64_t    m_Predicate;
        void*       m_UserData;
        uint32_t    m_Index;
        uint32_t    m_Capacity;
        void        (*m_BeginFunc)(const dmRender::RenderContext* rendercontext, const void* userdata);
        void        (*m_EndFunc)(const dmRender::RenderContext* rendercontext, const void* userdata);
    };


    HRenderWorld NewRenderWorld(uint32_t max_instances, uint32_t max_renderpasses, SetObjectModel set_object_model);
    void DeleteRenderWorld(HRenderWorld world);
    void AddRenderPass(HRenderWorld world, HRenderPass renderpass);

    void AddToRender(HRenderWorld local_world, HRenderWorld world);
    void RegisterRenderer(HRenderWorld world, uint32_t type, RenderTypeSetupFunc rendertype_setup, RenderTypeInstanceFunc rendertype_instance);


    void Update(HRenderWorld world, float dt);
    void UpdateContext(HRenderWorld world, RenderContext* rendercontext);
    void UpdateDeletedInstances(HRenderWorld world);
    HRenderObject NewRenderObject(HRenderWorld world, void* resource, void* go, uint64_t mask, uint32_t type);
    void DeleteRenderObject(HRenderWorld world, HRenderObject ro);
    void SetData(HRenderObject ro, void* data);
    void SetGameObject(HRenderObject ro, void* go);

    void Disable(HRenderObject ro);
    void Enable(HRenderObject ro);
    bool IsEnabled(HRenderObject ro);

    void SetPosition(HRenderObject ro, Vector4 pos);
    void SetRotation(HRenderObject ro, Quat rot);
    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type);
    void* GetData(HRenderObject ro);


//    void RenderPassBegin(RenderContext* rendercontext, RenderPass* rp);
//    void RenderPassEnd(RenderContext* rendercontext, RenderPass* rp);

    void SetViewProjectionMatrix(HRenderPass renderpass, Matrix4* viewprojectionmatrix);

    HRenderPass NewRenderPass(RenderPassDesc* desc);
    void DeleteRenderPass(HRenderPass renderpass);
    void AddRenderObject(HRenderPass renderpass, HRenderObject renderobject);
    void SetViewMatrix(HRenderPass renderpass, Matrix4* viewmatrix);
    void SetProjectionMatrix(HRenderPass renderpass, Matrix4* projectionmatrix);

    void DisableRenderPass(HRenderPass renderpass);
    void EnableRenderPass(HRenderPass renderpass);

}

#endif /* RENDER_H */
