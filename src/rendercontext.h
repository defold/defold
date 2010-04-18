#ifndef RENDERCONTEXT_H
#define RENDERCONTEXT_H

#include <vectormath/cpp/vectormath_aos.h>
#include <graphics/graphics_device.h>

using namespace Vectormath::Aos;

struct RenderContext
{
    Matrix4                     m_View;
    Matrix4                     m_Projection;
    Matrix4                     m_ViewProj;
    Point3                      m_CameraPosition;

    dmGraphics::HContext        m_GFXContext;
};


#endif // __RENDERCONTEXT_H__
