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
        static const uint32_t MAX_TAG_COUNT = 32;
        uint32_t m_Tags[MAX_TAG_COUNT];
        uint32_t m_TagCount;
    };

    typedef struct RenderContext* HRenderContext;
    typedef struct RenderObject* HRenderObject;

    typedef void (*RenderTypeBeginCallback)(HRenderContext render_context, void* user_context);
    typedef void (*RenderTypeDrawCallback)(HRenderContext render_context, void* user_context, HRenderObject ro, uint32_t count);
    typedef void (*RenderTypeEndCallback)(HRenderContext render_context, void* user_context);

    struct RenderType
    {
        RenderType();

        RenderTypeBeginCallback m_BeginCallback;
        RenderTypeDrawCallback  m_DrawCallback;
        RenderTypeEndCallback   m_EndCallback;
        void*                   m_UserContext;
    };

    typedef void (*SetObjectModel)(void* visual_object, Quat* rotation, Point3* position);

    struct RenderContextParams
    {
        RenderContextParams();

        uint32_t        m_MaxRenderTypes;
        uint32_t        m_MaxInstances;
        SetObjectModel  m_SetObjectModel;
        void*           m_VertexProgramData;
        uint32_t        m_VertexProgramDataSize;
        void*           m_FragmentProgramData;
        uint32_t        m_FragmentProgramDataSize;
        uint32_t        m_DisplayWidth;
        uint32_t        m_DisplayHeight;
        uint32_t        m_MaxCharacters;
    };

    typedef uint32_t HRenderType;

    static const HRenderObject INVALID_RENDER_OBJECT_HANDLE = 0;
    static const HRenderType INVALID_RENDER_TYPE_HANDLE = ~0;

    HRenderContext NewRenderContext(const RenderContextParams& params);
    Result DeleteRenderContext(HRenderContext render_context);

    Result RegisterRenderType(HRenderContext render_context, RenderType render_type, HRenderType* out_render_type);

    dmGraphics::HContext GetGraphicsContext(HRenderContext render_context);

    Matrix4* GetViewProjectionMatrix(HRenderContext render_context);
    void SetViewMatrix(HRenderContext render_context, const Matrix4& view);
    void SetProjectionMatrix(HRenderContext render_context, const Matrix4& projection);

    uint32_t GetDisplayWidth(HRenderContext render_context);
    uint32_t GetDisplayHeight(HRenderContext render_context);

    Result AddToRender(HRenderContext context, HRenderObject ro);
    Result ClearRenderObjects(HRenderContext context);

    Result Draw(HRenderContext context, Predicate* predicate);
    Result DrawDebug3d(HRenderContext context);
    Result DrawDebug2d(HRenderContext context);

    HRenderObject NewRenderObject(HRenderType render_type, dmGraphics::HMaterial material, void* visual_object);
    void DeleteRenderObject(HRenderObject ro);

    void* GetUserData(HRenderObject ro);
    void SetUserData(HRenderObject ro, void* user_data);

    dmGraphics::HMaterial GetMaterial(HRenderObject ro);
    void SetMaterial(HRenderObject ro, dmGraphics::HMaterial material);

    Point3 GetPosition(HRenderObject ro);
    void SetPosition(HRenderObject ro, Point3 pos);
    Quat GetRotation(HRenderObject ro);
    void SetRotation(HRenderObject ro, Quat rot);
    Vector4 GetColor(HRenderObject ro, ColorType color_type);
    void SetColor(HRenderObject ro, Vector4 color, ColorType color_type);
    Vector3 GetSize(HRenderObject ro);
    void SetSize(HRenderObject ro, Vector3 size);

    /**
     * Render debug square. The upper left corner of the screen is (-1,-1) and the bottom right is (1,1).
     * @param context Render context handle
     * @param x0 x coordinate of the left edge of the square
     * @param y0 y coordinate of the upper edge of the square
     * @param x1 x coordinate of the right edge of the square
     * @param y1 y coordinate of the bottom edge of the square
     * @param color Color
     */
    void Square2d(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color);

    /**
     * Render debug line. The upper left corner of the screen is (-1,-1) and the bottom right is (1,1).
     * @param context Render context handle
     * @param x0 x coordinate of the start of the line
     * @param y0 y coordinate of the start of the line
     * @param x1 x coordinate of the end of the line
     * @param y1 y coordinate of the end of the line
     * @param color0 Color of the start of the line
     * @param color1 Color of the end of the line
     */
    void Line2D(HRenderContext context, float x0, float y0, float x1, float y1, Vector4 color0, Vector4 color1);

    /**
     * Line3D Render debug line
     * @param context Render context handle
     * @param start Start point
     * @param end End point
     * @param color Color
     */
    void Line3D(HRenderContext context, Point3 start, Point3 end, Vector4 start_color, Vector4 end_color);
}

#endif /* RENDER_H */
