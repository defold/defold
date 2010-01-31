#ifndef MATERIAL_H
#define MATERIAL_H

#include <vectormath/cpp/vectormath_aos.h>
#include <graphics/graphics_device.h>

struct SMaterial
{
    SMaterial(): m_FragmentConstantMask(0), m_VertexConstantMask(0)
    {}

    static const uint32_t MAX_CONSTANTS = 8;

    dmGraphics::HFragmentProgram    m_FragmentProgram;
    dmGraphics::HVertexProgram      m_VertexProgram;
    Vectormath::Aos::Vector4        m_FragmentConstant[MAX_CONSTANTS];
    Vectormath::Aos::Vector4        m_VertexConstant[MAX_CONSTANTS];

    uint8_t                         m_FragmentConstantMask;
    uint8_t                         m_VertexConstantMask;
};

#endif // MATERIAL_H
