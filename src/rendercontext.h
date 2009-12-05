// TODO: should probably go someplace else?
#ifndef __RENDERCONTEXT_H__
#define __RENDERCONTEXT_H__

#include <vectormath/cpp/vectormath_aos.h>
#include <graphics/graphics_device.h>


struct RenderContext
{
    Vectormath::Aos::Matrix4    m_View;
    Vectormath::Aos::Matrix4    m_Projection;
    Vectormath::Aos::Matrix4    m_ViewProj;

    dmGraphics::HContext        m_GFXContext;
};


#endif // __RENDERCONTEXT_H__
