#ifndef MATERIAL_H_
#define MATERIAL_H_

#include <vectormath/cpp/vectormath_aos.h>
#include "graphics_device.h"


namespace dmGraphics
{
    static const uint32_t       MAX_MATERIAL_CONSTANTS = 8;
    typedef struct Material*    HMaterial;

    HMaterial           NewMaterial();
    void                DeleteMaterial(HMaterial material);

    void                SetMaterialVertexProgram(HMaterial material, HVertexProgram vertexprogram);
    HVertexProgram      GetMaterialVertexProgram(HMaterial material);
    void                SetMaterialVertexProgramConstant(HMaterial material, uint32_t reg, Vector4 constant);
    Vector4             GetMaterialVertexProgramConstant(HMaterial material, uint32_t reg);

    void                SetMaterialFragmentProgram(HMaterial material, HFragmentProgram fragmentprogram);
    HFragmentProgram    GetMaterialFragmentProgram(HMaterial material);
    void                SetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg, Vector4 constant);
    Vector4             GetMaterialFragmentProgramConstant(HMaterial material, uint32_t reg);

    void                SetMaterialVertexConstantMask(HMaterial material, uint32_t mask);
    void                SetMaterialFragmentConstantMask(HMaterial material, uint32_t mask);
    uint32_t            GetMaterialVertexConstantMask(HMaterial material);
    uint32_t            GetMaterialFragmentConstantMask(HMaterial material);

}


#endif /* MATERIAL_H_ */
