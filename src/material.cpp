#include "material.h"

namespace dmGraphics
{

    struct Material
    {

        Material(): m_FragmentConstantMask(0), m_VertexConstantMask(0)
        {}

        dmGraphics::HFragmentProgram m_FragmentProgram;
        dmGraphics::HVertexProgram   m_VertexProgram;
        Vectormath::Aos::Vector4     m_FragmentConstant[MAX_MATERIAL_CONSTANTS];
        Vectormath::Aos::Vector4     m_VertexConstant[MAX_MATERIAL_CONSTANTS];

        uint8_t                      m_FragmentConstantMask;
        uint8_t                      m_VertexConstantMask;

    };


    HMaterial NewMaterial()
    {
        Material* m = new Material;
        return (HMaterial)m;
    }

    void DeleteMaterial(HMaterial material)
    {
        delete material;
    }

    void SetMaterialVertexProgram(HMaterial material, HVertexProgram vertexprogram)
    {
        material->m_VertexProgram = vertexprogram;
    }
    void SetMaterialFragmentProgram(HMaterial material, HFragmentProgram fragmentprogram)
    {
        material->m_FragmentProgram = fragmentprogram;
    }

    HVertexProgram GetMaterialVertexProgram(HMaterial material)
    {
        return material->m_VertexProgram;
    }

    HFragmentProgram GetMaterialFragmentProgram(HMaterial material)
    {
        return material->m_FragmentProgram;
    }

    void SetMaterialVertexProgramConstant(HMaterial material, uint32_t reg, Vector4 constant)
    {
        material->m_VertexConstant[reg] = constant;
    }

    void SetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg, Vector4 constant)
    {
        material->m_FragmentConstant[reg] = constant;
    }

    Vector4 GetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg)
    {
        return material->m_FragmentConstant[reg];
    }

    Vector4 GetMaterialVertexProgramConstant(HMaterial material, uint32_t reg)
    {
        return material->m_VertexConstant[reg];
    }

    void SetMaterialVertexConstantMask(HMaterial material, uint32_t mask)   { material->m_VertexConstantMask = mask; }
    void SetMaterialFragmentConstantMask(HMaterial material, uint32_t mask) { material->m_FragmentConstantMask = mask; }
    uint32_t GetMaterialVertexConstantMask(HMaterial material)              { return material->m_VertexConstantMask; }
    uint32_t GetMaterialFragmentConstantMask(HMaterial material)            { return material->m_FragmentConstantMask; }



}
