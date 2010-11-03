#ifndef RENDER_H
#define RENDER_H

#include <string.h>
#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/container.h>
#include <graphics/graphics_device.h>
#include <graphics/material.h>

namespace dmRender
{
	using namespace Vectormath::Aos;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_INVALID_CONTEXT = -1,
    };

    enum ColorType
    {
        DIFFUSE_COLOR = 0,
        EMISSIVE_COLOR = 1,
        SPECULAR_COLOR = 2,
        GENERIC = 3,
        MAX_COLOR
    };

    struct Predicate
    {
        uint32_t m_Tags[16];
        uint32_t m_TagCount;
    };

    typedef struct RenderContext* HRenderContext;
    typedef struct RenderObject* HRenderObject;
    typedef void (*SetObjectModel)(void* visual_object, Quat* rotation, Point3* position);

    typedef void (*RenderTypeBeginCallback)(HRenderContext render_context);
    typedef void (*RenderTypeDrawCallback)(HRenderContext render_context, HRenderObject ro, uint32_t count);
    typedef void (*RenderTypeEndCallback)(HRenderContext render_context);

    struct RenderType
    {
        RenderTypeBeginCallback m_BeginCallback;
        RenderTypeDrawCallback  m_DrawCallback;
        RenderTypeEndCallback   m_EndCallback;
    };
    typedef uint32_t HRenderType;

    static const HRenderObject INVALID_RENDER_OBJECT_HANDLE = 0;
    static const HRenderType INVALID_RENDER_TYPE_HANDLE = ~0;

    HRenderContext NewRenderContext(uint32_t max_render_types, uint32_t max_instances, SetObjectModel set_object_model);
    Result DeleteRenderContext(HRenderContext render_context);

    Result RegisterRenderType(HRenderContext render_context, RenderType render_type, HRenderType* out_render_type);

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context);

    Matrix4* GetViewProjectionMatrix(HRenderContext render_context);
    void SetViewMatrix(HRenderContext render_context, const Matrix4& view);
    void SetProjectionMatrix(HRenderContext render_context, const Matrix4& projection);

    Result AddToRender(HRenderContext context, HRenderObject ro);
    Result ClearRenderObjects(HRenderContext context);

    Result Draw(HRenderContext context, const Predicate* predicate);

    HRenderObject NewRenderObject(HRenderType render_type, dmGraphics::HMaterial material, void* visual_object);
    void DeleteRenderObject(HRenderObject ro);

    void* GetUserData(HRenderObject ro);
    void SetUserData(HRenderObject ro, void* user_data);

    Point3 GetPosition(HRenderObject ro);
    void SetPosition(HRenderObject ro, Point3 pos);
    Quat GetRotation(HRenderObject ro);
    void SetRotation(HRenderObject ro, Quat rot);
    Vector4 GetColor(HRenderObject ro, ColorType color_type);
    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type);
    Vector3 GetSize(HRenderObject ro);
    void SetSize(HRenderObject ro, Vector3 size);
}

#endif /* RENDER_H */
