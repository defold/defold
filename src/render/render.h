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

    enum Result
    {
        RESULT_OK = 0,
        RESULT_INVALID_WORLD = -1,
    };


	enum RenderObjectType
	{
		RENDEROBJECT_TYPE_MODEL = 0,
		RENDEROBJECT_TYPE_PARTICLE = 1,
		RENDEROBJECT_TYPE_TEXT = 2,
		RENDEROBJECT_TYPE_PARTICLESETUP = 3,
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
    typedef void (*RenderTypeOnceFunc)(const RenderContext* rendercontext);



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
    Result DeleteRenderWorld(HRenderWorld world);
    Result ClearWorld(HRenderWorld world);
    Result AddRenderPass(HRenderWorld world, HRenderPass renderpass);

    Result AddToRender(HRenderWorld local_world, HRenderWorld world);
    Result RegisterRenderer(HRenderWorld world, uint32_t type, RenderTypeOnceFunc rendertype_setup, RenderTypeInstanceFunc rendertype_instance, RenderTypeOnceFunc rendertype_end);


    Result Update(HRenderWorld world, float dt);
    void UpdateContext(HRenderWorld world, RenderContext* rendercontext);
    HRenderObject NewRenderObject(HRenderWorld world, void* resource, void* go, uint64_t mask, uint32_t type);
    void DeleteRenderObject(HRenderWorld world, HRenderObject ro);
    void SetData(HRenderObject ro, void* data);
    void SetGameObject(HRenderObject ro, void* go);

    void Disable(HRenderObject ro);
    void Enable(HRenderObject ro);
    bool IsEnabled(HRenderObject ro);

    void SetPosition(HRenderObject ro, Point3 pos);
    void SetRotation(HRenderObject ro, Quat rot);
    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type);
    void SetSize(HRenderObject ro, Vector3 size);

    Point3 GetPosition(HRenderObject ro);
    Quat GetRotation(HRenderObject ro);
    Vector4 GetColor(HRenderObject ro, ColorType color_type);
    Vector3 GetSize(HRenderObject ro);
    void* GetData(HRenderObject ro);
    uint32_t GetUserData(HRenderObject ro, uint32_t index);
    void SetUserData(HRenderObject ro, uint32_t index, uint32_t value);


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
