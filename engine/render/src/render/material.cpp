// Copyright 2020-2024 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <algorithm>
#include <string.h>

#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hashtable.h>
#include <dlib/log.h>
#include <dmsdk/dlib/math.h> // min
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/graphics/graphics.h>
#include "render.h"
#include "render_private.h"

namespace dmRender
{
    using namespace dmVMath;

    static dmGraphics::VertexAttribute::SemanticType GetAttributeSemanticType(dmhash_t from_hash)
    {
        if      (from_hash == VERTEX_STREAM_POSITION)      return dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION;
        else if (from_hash == VERTEX_STREAM_TEXCOORD0)     return dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXCOORD;
        else if (from_hash == VERTEX_STREAM_TEXCOORD1)     return dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXCOORD;
        else if (from_hash == VERTEX_STREAM_COLOR)         return dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR;
        else if (from_hash == VERTEX_STREAM_PAGE_INDEX)    return dmGraphics::VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX;
        else if (from_hash == VERTEX_STREAM_NORMAL)        return dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL;
        else if (from_hash == VERTEX_STREAM_TANGENT)       return dmGraphics::VertexAttribute::SEMANTIC_TYPE_TANGENT;
        else if (from_hash == VERTEX_STREAM_WORLD_MATRIX)  return dmGraphics::VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX;
        else if (from_hash == VERTEX_STREAM_NORMAL_MATRIX) return dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX;
        return dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE;
    }

    // This is just a default setting, you can still specify a world matrix as a non-instanced attribute
    static inline dmGraphics::VertexStepFunction GetAttributeVertexStepFunction(dmGraphics::VertexAttribute::SemanticType semantic_type)
    {
        if (semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX ||
            semantic_type == dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX)
        {
            return dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
        }
        return dmGraphics::VERTEX_STEP_FUNCTION_VERTEX;
    }

    static inline dmGraphics::VertexAttribute::DataType GetAttributeDataType(dmGraphics::Type from_type)
    {
        switch(from_type)
        {
            case dmGraphics::TYPE_FLOAT:
            case dmGraphics::TYPE_FLOAT_VEC2:
            case dmGraphics::TYPE_FLOAT_VEC3:
            case dmGraphics::TYPE_FLOAT_VEC4:
            case dmGraphics::TYPE_FLOAT_MAT2:
            case dmGraphics::TYPE_FLOAT_MAT3:
            case dmGraphics::TYPE_FLOAT_MAT4:     return dmGraphics::VertexAttribute::TYPE_FLOAT;
            case dmGraphics::TYPE_BYTE:           return dmGraphics::VertexAttribute::TYPE_BYTE;
            case dmGraphics::TYPE_UNSIGNED_BYTE:  return dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE;
            case dmGraphics::TYPE_SHORT:          return dmGraphics::VertexAttribute::TYPE_SHORT;
            case dmGraphics::TYPE_UNSIGNED_SHORT: return dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT;
            case dmGraphics::TYPE_INT:            return dmGraphics::VertexAttribute::TYPE_INT;
            case dmGraphics::TYPE_UNSIGNED_INT:   return dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT;
            default: assert(0 && "Type not supported");
        }

        return (dmGraphics::VertexAttribute::DataType) -1;
    }

    static inline dmGraphics::VertexAttribute::ShaderType GetAttributeShaderType(dmGraphics::Type from_type)
    {
        switch(from_type)
        {
            case dmGraphics::TYPE_FLOAT:
            case dmGraphics::TYPE_BYTE:
            case dmGraphics::TYPE_UNSIGNED_BYTE:
            case dmGraphics::TYPE_SHORT:
            case dmGraphics::TYPE_UNSIGNED_SHORT:
            case dmGraphics::TYPE_INT:
            case dmGraphics::TYPE_UNSIGNED_INT: return dmGraphics::VertexAttribute::SHADER_TYPE_NUMBER;
            case dmGraphics::TYPE_FLOAT_VEC2:   return dmGraphics::VertexAttribute::SHADER_TYPE_VEC2;
            case dmGraphics::TYPE_FLOAT_VEC3:   return dmGraphics::VertexAttribute::SHADER_TYPE_VEC3;
            case dmGraphics::TYPE_FLOAT_VEC4:   return dmGraphics::VertexAttribute::SHADER_TYPE_VEC4;
            case dmGraphics::TYPE_FLOAT_MAT2:   return dmGraphics::VertexAttribute::SHADER_TYPE_MAT2;
            case dmGraphics::TYPE_FLOAT_MAT3:   return dmGraphics::VertexAttribute::SHADER_TYPE_MAT3;
            case dmGraphics::TYPE_FLOAT_MAT4:   return dmGraphics::VertexAttribute::SHADER_TYPE_MAT4;
            default: assert(0 && "Type not supported");
        }

        return (dmGraphics::VertexAttribute::ShaderType) -1;
    }

    static inline uint32_t GetAttributeElementCount(dmGraphics::VertexAttribute::ShaderType from_type)
    {
        switch(from_type)
        {
            case dmGraphics::VertexAttribute::SHADER_TYPE_NUMBER: return 1;
            case dmGraphics::VertexAttribute::SHADER_TYPE_VEC2: return 2;
            case dmGraphics::VertexAttribute::SHADER_TYPE_VEC3: return 3;
            case dmGraphics::VertexAttribute::SHADER_TYPE_VEC4: return 4;
            case dmGraphics::VertexAttribute::SHADER_TYPE_MAT2: return 4;
            case dmGraphics::VertexAttribute::SHADER_TYPE_MAT3: return 9;
            case dmGraphics::VertexAttribute::SHADER_TYPE_MAT4: return 16;
            default:assert(0 && "ShaderType not supported");
        }
        return -1;
    }

    static void CreateVertexDeclarations(dmGraphics::HContext graphics_context, Material* m)
    {
        if (m->m_VertexDeclarationShared != 0)
        {
            dmGraphics::DeleteVertexDeclaration(m->m_VertexDeclarationShared);
            m->m_VertexDeclarationShared = 0;
        }
        if (m->m_VertexDeclarationPerVertex != 0)
        {
            dmGraphics::DeleteVertexDeclaration(m->m_VertexDeclarationPerVertex);
            m->m_VertexDeclarationPerVertex = 0;
        }
        if (m->m_VertexDeclarationPerInstance != 0)
        {
            dmGraphics::DeleteVertexDeclaration(m->m_VertexDeclarationPerInstance);
            m->m_VertexDeclarationPerInstance = 0;
        }

        uint32_t num_material_attributes = m->m_MaterialAttributes.Size();
        bool use_secondary_vertex_declarations = false;

        // 1. Find out if we need to use secondary vertex and instance declarations
        for (int i = 0; i < num_material_attributes; ++i)
        {
            const dmGraphics::VertexAttribute& graphics_attribute = m->m_VertexAttributes[i];
            if (graphics_attribute.m_StepFunction == dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE)
            {
                use_secondary_vertex_declarations = true;
                break;
            }
        }

        dmGraphics::HVertexStreamDeclaration sd_shared   = dmGraphics::NewVertexStreamDeclaration(graphics_context);
        dmGraphics::HVertexStreamDeclaration sd_vertex   = 0;
        dmGraphics::HVertexStreamDeclaration sd_instance = 0;

        if (use_secondary_vertex_declarations)
        {
            sd_vertex = dmGraphics::NewVertexStreamDeclaration(graphics_context, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
            sd_instance = dmGraphics::NewVertexStreamDeclaration(graphics_context, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
        }

        // 2. Construct all vertex declarations
        for (int i = 0; i < num_material_attributes; ++i)
        {
            const dmGraphics::VertexAttribute& graphics_attribute = m->m_VertexAttributes[i];

            #define ADD_VERTEX_STREAM(sd, graphics_attribute) \
                dmGraphics::AddVertexStream(sd, \
                    graphics_attribute.m_NameHash, \
                    graphics_attribute.m_ElementCount, \
                    dmGraphics::GetGraphicsType(graphics_attribute.m_DataType), \
                    graphics_attribute.m_Normalize);

            ADD_VERTEX_STREAM(sd_shared, graphics_attribute);
            if (use_secondary_vertex_declarations)
            {
                ADD_VERTEX_STREAM(graphics_attribute.m_StepFunction == dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE ?
                        sd_instance : sd_vertex, graphics_attribute);
            }

            #undef ADD_VERTEX_STREAM
        }

        m->m_VertexDeclarationShared = dmGraphics::NewVertexDeclaration(graphics_context, sd_shared);
        dmGraphics::DeleteVertexStreamDeclaration(sd_shared);

        if (use_secondary_vertex_declarations)
        {
            m->m_VertexDeclarationPerVertex = dmGraphics::NewVertexDeclaration(graphics_context, sd_vertex);
            dmGraphics::DeleteVertexStreamDeclaration(sd_vertex);
            m->m_VertexDeclarationPerInstance = dmGraphics::NewVertexDeclaration(graphics_context, sd_instance);
            dmGraphics::DeleteVertexStreamDeclaration(sd_instance);
        }
    }

    static void CreateAttributes(dmGraphics::HContext graphics_context, Material* m)
    {
        uint32_t num_program_attributes  = dmGraphics::GetAttributeCount(m->m_Program);
        uint32_t num_attribute_byte_size = 0;

        m->m_MaterialAttributes.SetCapacity(num_program_attributes);
        m->m_MaterialAttributes.SetSize(num_program_attributes);
        m->m_VertexAttributes.SetCapacity(num_program_attributes);
        m->m_VertexAttributes.SetSize(num_program_attributes);

        bool instancing_supported = m->m_InstancingSupported;

        for (int i = 0; i < num_program_attributes; ++i)
        {
            dmhash_t name_hash     = 0;
            dmGraphics::Type type  = (dmGraphics::Type) -1;
            uint32_t num_values    = 0;
            uint32_t element_count = 0;
            int32_t location       = -1;

            dmGraphics::GetAttribute(m->m_Program, i, &name_hash, &type, &element_count, &num_values, &location);

            dmGraphics::VertexAttribute& vertex_attribute = m->m_VertexAttributes[i];
            vertex_attribute.m_NameHash        = name_hash;
            vertex_attribute.m_SemanticType    = GetAttributeSemanticType(name_hash);
            vertex_attribute.m_DataType        = GetAttributeDataType(type); // Convert from mat/vec to float if necessary
            vertex_attribute.m_ElementCount    = element_count;
            vertex_attribute.m_Normalize       = false;
            vertex_attribute.m_CoordinateSpace = dmGraphics::COORDINATE_SPACE_WORLD;
            vertex_attribute.m_ShaderType      = GetAttributeShaderType(type);
            vertex_attribute.m_StepFunction    = instancing_supported ? GetAttributeVertexStepFunction(vertex_attribute.m_SemanticType) : dmGraphics::VERTEX_STEP_FUNCTION_VERTEX;

            MaterialAttribute& material_attribute = m->m_MaterialAttributes[i];
            material_attribute.m_Location         = location;
            material_attribute.m_ValueIndex       = num_attribute_byte_size;
            material_attribute.m_ValueCount       = num_values;

            dmGraphics::Type base_type = dmGraphics::GetGraphicsType(vertex_attribute.m_DataType);

            num_attribute_byte_size += dmGraphics::GetTypeSize(base_type) * element_count;

        #if 0 // Debugging
            dmLogInfo("Vertex Attribute: %s", dmHashReverseSafe64(name_hash));
            dmLogInfo("type: %d, ele_count: %d, num_vals: %d, loc: %d, valueIndex: %d",
                (int) type, element_count, num_values, location, material_attribute.m_ValueIndex);
        #endif
        }

        m->m_MaterialAttributeValues.SetCapacity(num_attribute_byte_size);
        m->m_MaterialAttributeValues.SetSize(num_attribute_byte_size);
        memset(m->m_MaterialAttributeValues.Begin(), 0, num_attribute_byte_size);
    }

    void CreateConstants(dmGraphics::HContext graphics_context, HMaterial material)
    {
        uint32_t total_constants_count = dmGraphics::GetUniformCount(material->m_Program);

        uint32_t constants_count = 0;
        uint32_t samplers_count   = 0;
        GetProgramUniformCount(material->m_Program, total_constants_count, &constants_count, &samplers_count);

        if ((constants_count + samplers_count) > 0)
        {
            material->m_NameHashToLocation.SetCapacity((constants_count + samplers_count), (constants_count + samplers_count) * 2);
            material->m_Constants.SetCapacity(constants_count);
        }

        if (samplers_count > 0)
        {
            material->m_Samplers.SetCapacity(samplers_count);
            for (uint32_t i = 0; i < samplers_count; ++i)
            {
                material->m_Samplers.Push(Sampler());
            }
        }

        SetMaterialConstantValues(graphics_context, material->m_Program, total_constants_count, material->m_NameHashToLocation, material->m_Constants, material->m_Samplers);
    }

    HMaterial NewMaterial(dmRender::HRenderContext render_context, dmGraphics::HVertexProgram vertex_program, dmGraphics::HFragmentProgram fragment_program)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::HProgram program          = dmGraphics::NewProgram(graphics_context, vertex_program, fragment_program);
        if (!program)
        {
            return 0;
        }

        Material* m                       = new Material;
        m->m_RenderContext                = render_context;
        m->m_VertexProgram                = vertex_program;
        m->m_FragmentProgram              = fragment_program;
        m->m_Program                      = program;
        m->m_VertexDeclarationShared      = 0;
        m->m_VertexDeclarationPerVertex   = 0;
        m->m_VertexDeclarationPerInstance = 0;
        m->m_InstancingSupported          = dmGraphics::IsContextFeatureSupported(graphics_context, dmGraphics::CONTEXT_FEATURE_INSTANCING);

        CreateAttributes(graphics_context, m);
        CreateVertexDeclarations(graphics_context, m);
        CreateConstants(graphics_context, m);

        return (HMaterial)m;
    }

    void DeleteMaterial(dmRender::HRenderContext render_context, HMaterial material)
    {
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        dmGraphics::DeleteProgram(graphics_context, material->m_Program);
        dmGraphics::DeleteVertexDeclaration(material->m_VertexDeclarationPerVertex);

        if (material->m_VertexDeclarationPerInstance)
            dmGraphics::DeleteVertexDeclaration(material->m_VertexDeclarationPerInstance);

        for (uint32_t i = 0; i < material->m_Constants.Size(); ++i)
        {
            dmRender::DeleteConstant(material->m_Constants[i].m_Constant);
        }
        delete material;
    }

    void ApplyMaterialConstants(dmRender::HRenderContext render_context, HMaterial material, const RenderObject* ro)
    {
        const dmArray<RenderConstant>& constants = material->m_Constants;
        dmGraphics::HContext graphics_context    = dmRender::GetGraphicsContext(render_context);

        uint32_t n = constants.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            const RenderConstant& material_constant = constants[i];
            const HConstant constant = material_constant.m_Constant;
            dmGraphics::HUniformLocation location = GetConstantLocation(constant);
            dmRenderDDF::MaterialDesc::ConstantType type = GetConstantType(constant);

            switch (type)
            {
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                {
                    uint32_t num_values;
                    dmVMath::Vector4* values = GetConstantValues(constant, &num_values);
                    dmGraphics::SetConstantV4(graphics_context, values, num_values, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4:
                {
                    uint32_t num_values;
                    dmVMath::Vector4* values = GetConstantValues(constant, &num_values);
                    dmGraphics::SetConstantM4(graphics_context, values, num_values / 4, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ:
                {
                    if (dmGraphics::GetProgramLanguage(dmRender::GetMaterialProgram(material)) == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
                    {
                        Matrix4 ndc_matrix = Matrix4::identity();
                        ndc_matrix.setElem(2, 2, 0.5f );
                        ndc_matrix.setElem(3, 2, 0.5f );
                        const Matrix4 view_projection = ndc_matrix * render_context->m_ViewProj;
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&view_projection, 1, location);
                    }
                    else
                    {
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_ViewProj, 1, location);
                    }
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD:
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&ro->m_WorldTransform, 1, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_TEXTURE:
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&ro->m_TextureTransform, 1, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEW:
                {
                    dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_View, 1, location);
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_PROJECTION:
                {
                    // Vulkan NDC is [0..1] for z, so we must transform
                    // the projection before setting the constant.
                    if (dmGraphics::GetProgramLanguage(dmRender::GetMaterialProgram(material)) == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
                    {
                        Matrix4 ndc_matrix = Matrix4::identity();
                        ndc_matrix.setElem(2, 2, 0.5f );
                        ndc_matrix.setElem(3, 2, 0.5f );
                        const Matrix4 proj = ndc_matrix * render_context->m_Projection;
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&proj, 1, location);
                    }
                    else
                    {
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&render_context->m_Projection, 1, location);
                    }
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_NORMAL:
                {
                    {
                        Matrix4 normalT = GetNormalMatrix(render_context, ro->m_WorldTransform);
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&normalT, 1, location);
                    }
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLDVIEW:
                {
                    {
                        Matrix4 world_view = render_context->m_View * ro->m_WorldTransform;
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_view, 1, location);
                    }
                    break;
                }
                case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLDVIEWPROJ:
                {
                    if (dmGraphics::GetProgramLanguage(dmRender::GetMaterialProgram(material)) == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
                    {
                        Matrix4 ndc_matrix = Matrix4::identity();
                        ndc_matrix.setElem(2, 2, 0.5f );
                        ndc_matrix.setElem(3, 2, 0.5f );
                        const Matrix4 world_view_projection = ndc_matrix * render_context->m_ViewProj * ro->m_WorldTransform;
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_view_projection, 1, location);
                    }
                    else
                    {
                        const Matrix4 world_view_projection = render_context->m_ViewProj * ro->m_WorldTransform;
                        dmGraphics::SetConstantM4(graphics_context, (Vector4*)&world_view_projection, 1, location);
                    }
                    break;
                }
            }
        }
    }

    HSampler GetMaterialSampler(HMaterial material, uint32_t unit)
    {
        if (unit < material->m_Samplers.Size())
        {
            return (HSampler) &material->m_Samplers[unit];
        }
        return 0x0;
    }

    dmhash_t GetMaterialSamplerNameHash(HMaterial material, uint32_t unit)
    {
        if (unit < material->m_Samplers.Size())
        {
            return material->m_Samplers[unit].m_NameHash;
        }
        return 0;
    }

    uint32_t GetMaterialSamplerUnit(HMaterial material, dmhash_t name_hash)
    {
        for (uint32_t i = 0; i < material->m_Samplers.Size(); ++i)
        {
            const Sampler& sampler = material->m_Samplers[i];
            if (sampler.m_NameHash == name_hash)
                return i;
        }
        return 0xFFFFFFFF;
    }

    int32_t GetMaterialSamplerIndex(HMaterial material, dmhash_t name_hash)
    {
        for (int i = 0; i < material->m_Samplers.Size(); ++i)
        {
            if (material->m_Samplers[i].m_NameHash == name_hash)
            {
                return i;
            }
        }
        return -1;
    }

    void ApplyMaterialSampler(dmRender::HRenderContext render_context, HMaterial material, HSampler sampler, uint8_t unit, dmGraphics::HTexture texture)
    {
        if (!sampler)
        {
            return;
        }

        Sampler* s = (Sampler*) sampler;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        if (s->m_Location != -1)
        {
            dmGraphics::SetSampler(graphics_context, s->m_Location, unit);
            dmGraphics::SetTextureParams(texture, s->m_MinFilter, s->m_MagFilter, s->m_UWrap, s->m_VWrap, s->m_MaxAnisotropy);
        }
    }

    dmGraphics::HProgram GetMaterialProgram(HMaterial material)
    {
        return material->m_Program;
    }

    dmGraphics::HVertexProgram GetMaterialVertexProgram(HMaterial material)
    {
        return material->m_VertexProgram;
    }

    dmGraphics::HFragmentProgram GetMaterialFragmentProgram(HMaterial material)
    {
        return material->m_FragmentProgram;
    }

    static int32_t FindMaterialAttributeIndex(HMaterial material, dmhash_t name_hash)
    {
        dmArray<dmGraphics::VertexAttribute>& attributes = material->m_VertexAttributes;
        for (int i = 0; i < attributes.Size(); ++i)
        {
            if (attributes[i].m_NameHash == name_hash)
            {
                return i;
            }
        }
        return -1;
    }

    static inline int32_t FindMaterialConstantIndex(HMaterial material, dmhash_t name_hash)
    {
        dmArray<RenderConstant>& constants = material->m_Constants;
        int32_t n = (int32_t)constants.Size();
        for (int32_t i = 0; i < n; ++i)
        {
            dmhash_t constant_name_hash = GetConstantName(constants[i].m_Constant);
            if (constant_name_hash == name_hash)
            {
                return i;
            }
        }
        return -1;
    }

    void SetMaterialProgramConstantType(HMaterial material, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type)
    {
        int32_t index = FindMaterialConstantIndex(material, name_hash);
        if (index < 0)
            return;

        RenderConstant& mc = material->m_Constants[index];
        SetConstantType(mc.m_Constant, type);
    }

    bool GetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, HConstant& out_value)
    {
        int32_t index = FindMaterialConstantIndex(material, name_hash);
        if (index < 0)
            return false;

        RenderConstant& mc = material->m_Constants[index];
        out_value = mc.m_Constant;
        return true;
    }

    bool GetMaterialProgramConstantInfo(HMaterial material, dmhash_t name_hash, dmhash_t* out_constant_id, dmhash_t* out_element_ids[4], uint32_t* out_element_index, uint16_t* out_array_size)
    {
        if (name_hash == 0)
            return false;

        dmArray<RenderConstant>& constants = material->m_Constants;
        uint32_t n = constants.Size();
        *out_element_index = ~0u;
        for (uint32_t i = 0; i < n; ++i)
        {
            RenderConstant& c = constants[i];
            dmhash_t constant_name_hash = GetConstantName(c.m_Constant);
            uint32_t num_values;
            dmVMath::Vector4* values = GetConstantValues(c.m_Constant, &num_values);
            (void)values;
            if (constant_name_hash == name_hash)
            {
                *out_element_ids = c.m_ElementIds;
                *out_constant_id = constant_name_hash;
                *out_array_size  = num_values;
                return true;
            }
            for (uint32_t elem_i = 0; elem_i < 4; ++elem_i)
            {
                if (c.m_ElementIds[elem_i] == name_hash)
                {
                    *out_element_index = elem_i;
                    *out_constant_id   = constant_name_hash;
                    *out_array_size    = num_values;
                    return true;
                }
            }
        }
        return false;
    }

    bool GetMaterialProgramAttributeInfo(HMaterial material, dmhash_t name_hash, MaterialProgramAttributeInfo& info)
    {
        dmArray<dmGraphics::VertexAttribute>& attributes = material->m_VertexAttributes;
        for (int i = 0; i < attributes.Size(); ++i)
        {
            MaterialAttribute& material_attribute = material->m_MaterialAttributes[i];

            bool found = false;
            uint32_t element_index = 0;

            if (attributes[i].m_NameHash == name_hash)
            {
                found = true;
            }
            else
            {
                for (uint32_t elem_i = 0; elem_i < 4; ++elem_i)
                {
                    if (material_attribute.m_ElementIds[elem_i] == name_hash)
                    {
                        element_index = elem_i;
                        found         = true;
                        break;
                    }
                }
            }

            if (found)
            {
                info.m_AttributeNameHash = attributes[i].m_NameHash;
                info.m_Attribute         = &material->m_VertexAttributes[i];
                info.m_ValuePtr          = &material->m_MaterialAttributeValues[material_attribute.m_ValueIndex];
                info.m_ElementIndex      = element_index;
                memcpy(info.m_ElementIds, material_attribute.m_ElementIds, sizeof(material_attribute.m_ElementIds));
                return true;
            }
        }

        return false;
    }

    void GetMaterialProgramAttributes(HMaterial material, const dmGraphics::VertexAttribute** attributes, uint32_t* attribute_count)
    {
        *attributes      = material->m_VertexAttributes.Begin();
        *attribute_count = material->m_VertexAttributes.Size();
    }

    void GetMaterialProgramAttributeValues(HMaterial material, uint32_t index, const uint8_t** value_ptr, uint32_t* value_byte_size)
    {
        assert(index < material->m_MaterialAttributes.Size());
        MaterialAttribute& material_attribute           = material->m_MaterialAttributes[index];
        dmGraphics::VertexAttribute& graphics_attribute = material->m_VertexAttributes[index];

        dmGraphics::Type base_type = dmGraphics::GetGraphicsType(graphics_attribute.m_DataType);
        *value_byte_size           = dmGraphics::GetTypeSize(base_type) * graphics_attribute.m_ElementCount;
        *value_ptr                 = &material->m_MaterialAttributeValues[material_attribute.m_ValueIndex];
    }

    void SetMaterialProgramAttributes(HMaterial material, const dmGraphics::VertexAttribute* attributes, uint32_t attributes_count)
    {
        // Don't need to do all this work if we don't have any custom attributes coming in
        if (attributes == 0 || attributes_count == 0)
        {
            return;
        }

        bool update_attributes = false;

        for (int i = 0; i < attributes_count; ++i)
        {
            const dmGraphics::VertexAttribute& graphics_attribute_in = attributes[i];
            int32_t index = FindMaterialAttributeIndex(material, graphics_attribute_in.m_NameHash);
            if (index < 0)
            {
                continue;
            }

            dmGraphics::VertexAttribute& graphics_attribute = material->m_VertexAttributes[index];
            graphics_attribute.m_DataType                   = graphics_attribute_in.m_DataType;
            graphics_attribute.m_Normalize                  = graphics_attribute_in.m_Normalize;
            graphics_attribute.m_ElementCount               = GetAttributeElementCount(graphics_attribute_in.m_ShaderType);
            graphics_attribute.m_ShaderType                 = graphics_attribute_in.m_ShaderType;
            graphics_attribute.m_SemanticType               = graphics_attribute_in.m_SemanticType;
            graphics_attribute.m_CoordinateSpace            = graphics_attribute_in.m_CoordinateSpace;
            graphics_attribute.m_StepFunction               = material->m_InstancingSupported ? graphics_attribute_in.m_StepFunction : dmGraphics::VERTEX_STEP_FUNCTION_VERTEX;

            update_attributes = true;
        }

        // If the incoming attributes don't match any of the attributes from the shader, we don't need to do anything more
        if (!update_attributes)
        {
            return;
        }

        // Need to readjust value indices since the layout could have changed
        uint32_t value_byte_size = 0;
        for (int i = 0; i < material->m_VertexAttributes.Size(); ++i)
        {
            dmRender::MaterialAttribute& material_attribute = material->m_MaterialAttributes[i];
            material_attribute.m_ValueIndex                 = value_byte_size;

            dmGraphics::Type graphics_type = dmGraphics::GetGraphicsType(material->m_VertexAttributes[i].m_DataType);
            value_byte_size += dmGraphics::GetTypeSize(graphics_type) * material->m_VertexAttributes[i].m_ElementCount;
        }

        material->m_MaterialAttributeValues.SetCapacity(value_byte_size);
        material->m_MaterialAttributeValues.SetSize(value_byte_size);

        const uint32_t name_buffer_size = 128;
        char name_buffer[name_buffer_size];

        // And one more pass to set the new values from the incoming vertex declaration
        for (int i = 0; i < attributes_count; ++i)
        {
            const dmGraphics::VertexAttribute& graphics_attribute_in = attributes[i];
            int32_t index = FindMaterialAttributeIndex(material, graphics_attribute_in.m_NameHash);
            if (index < 0)
            {
                continue;
            }

            MaterialAttribute& material_attribute = material->m_MaterialAttributes[index];

            const uint8_t* bytes;
            uint32_t byte_size;
            dmGraphics::GetAttributeValues(graphics_attribute_in, &bytes, &byte_size);

            dmGraphics::Type graphics_type = dmGraphics::GetGraphicsType(graphics_attribute_in.m_DataType);
            uint32_t attribute_byte_size   = dmGraphics::GetTypeSize(graphics_type) * GetAttributeElementCount(graphics_attribute_in.m_ShaderType) * material_attribute.m_ValueCount;
            attribute_byte_size            = dmMath::Min(attribute_byte_size, byte_size);
            memcpy(&material->m_MaterialAttributeValues[material_attribute.m_ValueIndex], bytes, attribute_byte_size);

            if (graphics_attribute_in.m_Name != 0x0)
            {
                dmStrlCpy(name_buffer, graphics_attribute_in.m_Name, name_buffer_size);
                FillElementIds(name_buffer, name_buffer_size, material_attribute.m_ElementIds);
            }
        }

        CreateVertexDeclarations(GetGraphicsContext(material->m_RenderContext), material);
    }

    void SetMaterialProgramConstant(HMaterial material, dmhash_t name_hash, Vector4* values, uint32_t count)
    {
        int32_t index = FindMaterialConstantIndex(material, name_hash);
        if (index < 0)
        {
            return;
        }

        RenderConstant& mc = material->m_Constants[index];

        uint32_t num_default_values;
        dmVMath::Vector4* constant_values = dmRender::GetConstantValues(mc.m_Constant, &num_default_values);

        // we cannot set more values than are already registered with the program
        if (num_default_values < count)
            count = num_default_values;

        // we musn't set less values than are already registered with the program
        // so we write to the previous buffer
        memcpy(constant_values, values, count * sizeof(dmVMath::Vector4));
    }

    dmGraphics::HUniformLocation GetMaterialConstantLocation(HMaterial material, dmhash_t name_hash)
    {
        dmGraphics::HUniformLocation* location = material->m_NameHashToLocation.Get(name_hash);
        if (location)
        {
            return *location;
        }
        else
        {
            return dmGraphics::INVALID_UNIFORM_LOCATION;
        }
    }

    bool SetMaterialSampler(HMaterial material, dmhash_t name_hash, uint32_t unit, dmGraphics::TextureWrap u_wrap, dmGraphics::TextureWrap v_wrap, dmGraphics::TextureFilter min_filter, dmGraphics::TextureFilter mag_filter, float max_anisotropy)
    {
        dmArray<Sampler>& samplers = material->m_Samplers;

        if (unit < samplers.Size() && name_hash != 0)
        {
            dmGraphics::HUniformLocation* location = material->m_NameHashToLocation.Get(name_hash);
            if (location)
            {
                Sampler& s        = samplers[unit];
                s.m_NameHash      = name_hash;
                s.m_Location      = *location;
                s.m_UWrap         = u_wrap;
                s.m_VWrap         = v_wrap;
                s.m_MinFilter     = min_filter;
                s.m_MagFilter     = mag_filter;
                s.m_MaxAnisotropy = max_anisotropy;
                return true;
            }
        }
        return false;
    }

    dmGraphics::HVertexDeclaration GetVertexDeclaration(HMaterial material)
    {
        return material->m_VertexDeclarationShared;
    }

    dmGraphics::HVertexDeclaration GetVertexDeclaration(HMaterial material, dmGraphics::VertexStepFunction step_function)
    {
        if (step_function == dmGraphics::VERTEX_STEP_FUNCTION_VERTEX)
        {
            return material->m_InstancingSupported ? material->m_VertexDeclarationPerVertex : material->m_VertexDeclarationShared;
        }
        else
        {
            return material->m_VertexDeclarationPerInstance;
        }
    }

    HRenderContext GetMaterialRenderContext(HMaterial material)
    {
        return material->m_RenderContext;
    }

    uint64_t GetMaterialUserData1(HMaterial material)
    {
        return material->m_UserData1;
    }

    void SetMaterialUserData1(HMaterial material, uint64_t user_data)
    {
        material->m_UserData1 = user_data;
    }

    uint64_t GetMaterialUserData2(HMaterial material)
    {
        return material->m_UserData2;
    }

    void SetMaterialUserData2(HMaterial material, uint64_t user_data)
    {
        material->m_UserData2 = user_data;
    }

    void SetMaterialVertexSpace(HMaterial material, dmRenderDDF::MaterialDesc::VertexSpace vertex_space)
    {
        material->m_VertexSpace = vertex_space;
    }

    dmRenderDDF::MaterialDesc::VertexSpace GetMaterialVertexSpace(HMaterial material)
    {
        return material->m_VertexSpace;
    }

    uint32_t GetMaterialTagListKey(HMaterial material)
    {
        return material->m_TagListKey;
    }

    uint32_t RegisterMaterialTagList(HRenderContext context, uint32_t tag_count, const dmhash_t* tags)
    {
        uint32_t list_key = dmHashBuffer32(tags, sizeof(dmhash_t)*tag_count);
        MaterialTagList* value = context->m_MaterialTagLists.Get(list_key);
        if (value != 0)
            return list_key;

        assert(tag_count <= dmRender::MAX_MATERIAL_TAG_COUNT);
        MaterialTagList taglist;
        for (uint32_t i = 0; i < tag_count; ++i) {
            taglist.m_Tags[i] = tags[i];
        }
        taglist.m_Count = tag_count;

        if (context->m_MaterialTagLists.Full())
        {
            uint32_t capacity = context->m_MaterialTagLists.Capacity();
            capacity += 8;
            context->m_MaterialTagLists.SetCapacity(capacity * 2, capacity);
        }

        context->m_MaterialTagLists.Put(list_key, taglist);
        return list_key;
    }

    void GetMaterialTagList(HRenderContext context, uint32_t list_key, MaterialTagList* list)
    {
        MaterialTagList* value = context->m_MaterialTagLists.Get(list_key);
        if (!value) {
            dmLogError("Failed to get material tag list with hash 0x%08x", list_key)
            list->m_Count = 0;
            return;
        }
        *list = *value;
    }

    void SetMaterialTags(HMaterial material, uint32_t tag_count, const dmhash_t* tags)
    {
        material->m_TagListKey = RegisterMaterialTagList(material->m_RenderContext, tag_count, tags);
    }

    void ClearMaterialTags(HMaterial material)
    {
        material->m_TagListKey = 0;
    }

    bool MatchMaterialTags(uint32_t material_tag_count, const dmhash_t* material_tags, uint32_t tag_count, const dmhash_t* tags)
    {
        uint32_t last_hit = 0;
        for (uint32_t t = 0; t < tag_count; ++t)
        {
            // Both lists must be sorted in ascending order!
            bool hit = false;
            for (uint32_t mt = last_hit; mt < material_tag_count; ++mt)
            {
                if (tags[t] == material_tags[mt])
                {
                    hit = true;
                    last_hit = mt + 1; // since the list is sorted, we can start at this index next loop
                    break;
                }
            }
            if (!hit)
                return false;
        }
        return tag_count > 0; // don't render anything with no matches at all
    }

    bool GetCanBindTexture(dmGraphics::HTexture texture, HSampler sampler, uint32_t unit)
    {
        dmGraphics::TextureType texture_type = dmGraphics::GetTextureType(texture);
        Sampler* s = (Sampler*) sampler;

        if (s == 0x0)
        {
            dmLogError("Unable to bind texture with type %s to a null sampler (texture unit %d).",
                dmGraphics::GetTextureTypeLiteral(texture_type), unit);
            return false;
        }

        if (texture_type != s->m_Type)
        {
            dmLogError("Unable to bind texture with type %s to a sampler with type %s (texture unit %d).",
                dmGraphics::GetTextureTypeLiteral(texture_type),
                dmGraphics::GetTextureTypeLiteral(s->m_Type),
                unit);
            return false;
        }

        uint8_t num_sub_handles = dmGraphics::GetNumTextureHandles(texture);
        if (num_sub_handles > s->m_UnitValueCount)
        {
            dmLogError("Unable to bind array texture with %d handles to a sampler with %d bind slots",
                num_sub_handles, s->m_UnitValueCount);
            return false;
        }
        return true;
    }
}
