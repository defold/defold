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



    struct ModeltypeDesc
    {
        ModeltypeDesc(const char* name, void* userdata, uint32_t renderpass, void (*beginfunc)(const void* userdata), void (*endfunc)(const void* userdata), void (*updatefunc)(const void* userdata))
        {
            strncpy(m_Name, name, sizeof(m_Name));
            m_UserData = userdata;
            m_Renderpass = renderpass;
            m_BeginFunc = beginfunc;
            m_EndFunc = endfunc;
            m_UpdateFunc = updatefunc;
        }

        char    m_Name[128];
        void*   m_UserData;
        uint32_t m_Renderpass;
        void    (*m_BeginFunc)(const void* userdata);
        void    (*m_EndFunc)(const void* userdata);
        void    (*m_UpdateFunc)(const void* userdata);
    };

    struct RenderPassDesc
    {
        RenderPassDesc(){}
        RenderPassDesc(const char* name, void* userdata, uint32_t index, uint32_t capacity, void (*beginfunc)(const dmRender::RenderContext* rendercontext, const void* userdata), void (*endfunc)(const dmRender::RenderContext* rendercontext, const void* userdata))
        {
            strncpy(m_Name, name, sizeof(m_Name));
            m_UserData = userdata;
            m_Index = index;
            m_Capacity = capacity;
            m_BeginFunc = beginfunc;
            m_EndFunc = endfunc;
        }

        char        m_Name[128];
        void*       m_UserData;
        uint32_t    m_Index;
        uint32_t    m_Capacity;
        void        (*m_BeginFunc)(const dmRender::RenderContext* rendercontext, const void* userdata);
        void        (*m_EndFunc)(const dmRender::RenderContext* rendercontext, const void* userdata);
    };


    HRenderWorld NewRenderWorld(uint32_t max_instances, uint32_t max_renderpasses, SetObjectModel set_object_model);
    void DeleteRenderWorld(HRenderWorld world);
    void AddRenderPass(HRenderWorld world, HRenderPass renderpass);


    void Update(HRenderWorld world, float dt);
    void UpdateContext(HRenderWorld world, RenderContext* rendercontext);
    HRenderObject NewRenderObjectInstance(HRenderWorld world, void* resource, void* go, RenderObjectType type);
    void DeleteRenderObject(HRenderWorld world, HRenderObject ro);
    void Disable(HRenderObject ro);
    void Enable(HRenderObject ro);
    bool IsEnabled(HRenderObject ro);

    void SetPosition(HRenderObject ro, Vector4 pos);
    void SetRotation(HRenderObject ro, Quat rot);
    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type);


    void RenderPassBegin(RenderContext* rendercontext, RenderPass* rp);
    void RenderPassEnd(RenderContext* rendercontext, RenderPass* rp);
    HRenderPass NewRenderPass(RenderPassDesc* desc);
    void DeleteRenderPass(HRenderPass renderpass);
    void AddRenderObject(HRenderPass renderpass, HRenderObject renderobject);

    void DisableRenderPass(HRenderPass renderpass);
    void EnableRenderPass(HRenderPass renderpass);

}

#endif /* RENDER_H */
