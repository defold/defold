#ifndef RENDER_MATERIAL_H
#define RENDER_MATERIAL_H

#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/array.h>

#include <ddf/ddf.h>

#include <graphics/graphics_device.h>

#include "render/material_ddf.h"

namespace dmRender
{
    static const uint32_t           MAX_CONSTANT_COUNT = 32;
    typedef struct Material*        HMaterial;

    HMaterial                       NewMaterial();
    void                            DeleteMaterial(HMaterial material);

    void                            SetMaterialVertexProgram(HMaterial material, dmGraphics::HVertexProgram vertexprogram);
    dmGraphics::HVertexProgram      GetMaterialVertexProgram(HMaterial material);
    dmRenderDDF::MaterialDesc::ConstantType GetMaterialVertexProgramConstantType(HMaterial material, uint32_t reg);
    void                            SetMaterialVertexProgramConstantType(HMaterial material, uint32_t reg, dmRenderDDF::MaterialDesc::ConstantType type);
    void                            SetMaterialVertexProgramConstant(HMaterial material, uint32_t reg, Vector4 constant);
    Vector4                         GetMaterialVertexProgramConstant(HMaterial material, uint32_t reg);

    void                            SetMaterialFragmentProgram(HMaterial material, dmGraphics::HFragmentProgram fragmentprogram);
    dmGraphics::HFragmentProgram    GetMaterialFragmentProgram(HMaterial material);
    dmRenderDDF::MaterialDesc::ConstantType GetMaterialFragmentProgramConstantType(HMaterial material, uint32_t reg);
    void                            SetMaterialFragmentProgramConstantType(HMaterial material, uint32_t reg, dmRenderDDF::MaterialDesc::ConstantType type);
    void                            SetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg, Vector4 constant);
    Vector4                         GetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg);

    void                            SetMaterialVertexConstantMask(HMaterial material, uint32_t mask);
    void                            SetMaterialFragmentConstantMask(HMaterial material, uint32_t mask);
    uint32_t                        GetMaterialVertexConstantMask(HMaterial material);
    uint32_t                        GetMaterialFragmentConstantMask(HMaterial material);

    uint32_t                        GetMaterialTagMask(HMaterial material);
    void                            AddMaterialTag(HMaterial material, uint32_t tag);
    uint32_t                        ConvertMaterialTagsToMask(uint32_t* tags, uint32_t tag_count);
}

#endif /* RENDER_MATERIAL_H */
