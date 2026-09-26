// Copyright 2020-2026 The Defold Foundation
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

#include "test_gamesys_private.h"

#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
#include <sanitizer/allocator_interface.h>
#endif

using namespace dmVMath;

#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
struct MaterialAttributeAllocationPoison
{
    dmhash_t m_ElementId;
    uint32_t m_AttributeCount;
    bool     m_Armed;
    bool     m_MaterialAllocated;
    bool     m_Poisoned;
};

static MaterialAttributeAllocationPoison g_MaterialAttributeAllocationPoison;

static void MaterialAttributeMallocHook(const volatile void* ptr, size_t size)
{
    MaterialAttributeAllocationPoison& poison = g_MaterialAttributeAllocationPoison;
    if (!poison.m_Armed)
        return;

    if (!poison.m_MaterialAllocated)
    {
        poison.m_MaterialAllocated = size == sizeof(dmRender::Material);
        return;
    }

    if (size == sizeof(dmRender::MaterialAttribute) * poison.m_AttributeCount)
    {
        dmRender::MaterialAttribute* attributes = (dmRender::MaterialAttribute*) const_cast<void*>(ptr);
        attributes[0].m_ElementIds[0] = poison.m_ElementId;
        poison.m_Poisoned = true;
        poison.m_Armed = false;
    }
    else
    {
        poison.m_MaterialAllocated = size == sizeof(dmRender::Material);
    }
}

static void MaterialAttributeFreeHook(const volatile void*)
{
}

static bool PoisonNextMaterialAttributeAllocation(dmhash_t element_id, uint32_t attribute_count)
{
    static bool hook_installed = false;
    if (!hook_installed)
    {
        hook_installed = __sanitizer_install_malloc_and_free_hooks(MaterialAttributeMallocHook, MaterialAttributeFreeHook) != 0;
    }

    memset(&g_MaterialAttributeAllocationPoison, 0, sizeof(g_MaterialAttributeAllocationPoison));
    g_MaterialAttributeAllocationPoison.m_ElementId = element_id;
    g_MaterialAttributeAllocationPoison.m_AttributeCount = attribute_count;
    g_MaterialAttributeAllocationPoison.m_Armed = hook_installed;
    return hook_installed;
}
#endif

static void ComputeTextureTransformFromTexCoords(const float* tc, float* out_tt)
{
    const bool uv_rotated = (tc[0] != tc[2]) && (tc[3] != tc[5]);
    if (uv_rotated)
    {
        out_tt[0] = tc[4] - tc[6];
        out_tt[1] = tc[5] - tc[7];
        out_tt[2] = 0.0f;
        out_tt[3] = tc[0] - tc[6];
        out_tt[4] = tc[1] - tc[7];
        out_tt[5] = 0.0f;
        out_tt[6] = tc[6];
        out_tt[7] = tc[7];
        out_tt[8] = 1.0f;
    }
    else
    {
        out_tt[0] = tc[6] - tc[0];
        out_tt[1] = tc[7] - tc[1];
        out_tt[2] = 0.0f;
        out_tt[3] = tc[2] - tc[0];
        out_tt[4] = tc[3] - tc[1];
        out_tt[5] = 0.0f;
        out_tt[6] = tc[0];
        out_tt[7] = tc[1];
        out_tt[8] = 1.0f;
    }
}

static void ComputeTextureTransformFromTextureSet(dmResource::HFactory factory,
                                                  const char* textureset_path,
                                                  uint32_t frame_index,
                                                  float* out_tt)
{
    dmGameSystem::TextureSetResource* ts_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK,
              dmResource::Get(factory, textureset_path, (void**)&ts_res));
    ASSERT_NE((void*)0, ts_res);

    const dmGameSystemDDF::TextureSet* ts_ddf = ts_res->m_TextureSet;
    ASSERT_TRUE(ts_ddf->m_TexCoords.m_Count >= (frame_index + 1u) * 8u * sizeof(float));

    const float* tc = (const float*) ts_ddf->m_TexCoords.m_Data;
    tc += frame_index * 8u;

    ComputeTextureTransformFromTexCoords(tc, out_tt);

    dmResource::Release(factory, ts_res);
}

TEST_F(RenderConstantsTest, CreateDestroy)
{
    dmGameSystem::HComponentRenderConstants constants = dmGameSystem::CreateRenderConstants();
    dmGameSystem::DestroyRenderConstants(constants);
}

#if !defined(DM_PLATFORM_VENDOR) // we need to fix our test material/shader compiler to work with the constants

TEST_F(RenderConstantsTest, SetGetConstant)
{
    dmhash_t name_hash1 = dmHashString64("user_var1");
    dmhash_t name_hash2 = dmHashString64("user_var2");

    dmGameSystem::MaterialResource* material = 0;
    {
        const char path_material[] = "/material/valid.materialc";
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_material, (void**)&material));
        ASSERT_NE((void*)0, material);

        dmRender::HConstant rconstant;
        ASSERT_TRUE(dmRender::GetMaterialProgramConstant(material->m_Material, name_hash1, rconstant));
        ASSERT_TRUE(dmRender::GetMaterialProgramConstant(material->m_Material, name_hash2, rconstant));
    }

    dmGameSystem::HComponentRenderConstants constants = dmGameSystem::CreateRenderConstants();

    dmRender::HConstant constant = 0;
    bool result = dmGameSystem::GetRenderConstant(constants, name_hash1, &constant);
    ASSERT_FALSE(result);
    ASSERT_EQ(0, constant);

    // Setting property value
    dmGameObject::PropertyVar var1(dmVMath::Vector4(1,2,3,4));
    dmGameSystem::SetRenderConstant(constants, material->m_Material, name_hash1, 0, 0, var1); // stores the previous value

    result = dmGameSystem::GetRenderConstant(constants, name_hash1, &constant);
    ASSERT_TRUE(result);
    ASSERT_NE((void*)0, constant);
    ASSERT_EQ(name_hash1, constant->m_NameHash);

    // Issue in 1.2.183: We reallocated the array, thus invalidating the previous pointer
    dmGameObject::PropertyVar var2(dmVMath::Vector4(5,6,7,8));
    dmGameSystem::SetRenderConstant(constants, material->m_Material, name_hash2, 0, 0, var2);
    // Make sure it's still valid and doesn't trigger an ASAN issue
    ASSERT_EQ(name_hash1, constant->m_NameHash);

    dmRender::HConstant constant2 = 0;
    dmGameSystem::GetRenderConstant(constants, name_hash2, &constant2);

    ASSERT_NE(0, dmGameSystem::ClearRenderConstant(constants, name_hash1)); // removed
    ASSERT_EQ(0, dmGameSystem::ClearRenderConstant(constants, name_hash1)); // not removed
    ASSERT_NE(0, dmGameSystem::ClearRenderConstant(constants, name_hash2));
    ASSERT_EQ(0, dmGameSystem::ClearRenderConstant(constants, name_hash2));

    dmRender::DeleteConstant(constant);
    dmRender::DeleteConstant(constant2);

    // Setting raw value
    dmVMath::Vector4 value(1,2,3,4);
    dmGameSystem::SetRenderConstant(constants, name_hash1, &value, 1);

    result = dmGameSystem::GetRenderConstant(constants, name_hash1, &constant);
    ASSERT_TRUE(result);

    uint32_t num_values;
    dmVMath::Vector4* values = dmRender::GetConstantValues(constant, &num_values);
    ASSERT_EQ(1U, num_values);
    ASSERT_TRUE(values != 0);
    ASSERT_ARRAY_EQ_LEN(&value, values, num_values);

    dmGameSystem::DestroyRenderConstants(constants);

    dmResource::Release(m_Factory, material);
}
#endif

TEST_F(RenderConstantsTest, SetGetManyConstants)
{
    dmGameSystem::HComponentRenderConstants constants = dmGameSystem::CreateRenderConstants();

    for (int i = 0; i < 64; ++i)
    {
        char name[64];
        dmSnPrintf(name, sizeof(name), "var%03d", i);
        dmhash_t name_hash = dmHashString64(name);

        dmVMath::Vector4 v(i, i*3+1,0,0);
        dmGameSystem::SetRenderConstant(constants, name_hash, &v, 1);
    }

    for (int i = 0; i < 64; ++i)
    {
        char name[64];
        dmSnPrintf(name, sizeof(name), "var%03d", i);
        dmhash_t name_hash = dmHashString64(name);

        dmVMath::Vector4 v(i, i*3+1,0,0);

        dmRender::HConstant constant = 0;
        bool result = dmGameSystem::GetRenderConstant(constants, name_hash, &constant);
        ASSERT_TRUE(result);
        ASSERT_NE((dmRender::HConstant)0, constant);

        uint32_t num_values;
        dmVMath::Vector4* values = dmRender::GetConstantValues(constant, &num_values);
        ASSERT_EQ(1U, num_values);
        ASSERT_TRUE(values != 0);
        ASSERT_EQ((float)i, values[0].getX());
        ASSERT_EQ((float)(i*3+1), values[0].getY());
    }

    dmGameSystem::DestroyRenderConstants(constants);
}


TEST_F(RenderConstantsTest, HashRenderConstants)
{
    dmGameSystem::HComponentRenderConstants constants = dmGameSystem::CreateRenderConstants();
    bool result;

    result = dmGameSystem::AreRenderConstantsUpdated(constants);
    ASSERT_FALSE(result);

    dmhash_t name_hash1 = dmHashString64("user_var1");
    dmVMath::Vector4 value(1,2,3,4);
    dmGameSystem::SetRenderConstant(constants, name_hash1, &value, 1);

    result = dmGameSystem::AreRenderConstantsUpdated(constants);
    ASSERT_TRUE(result);

    ////////////////////////////////////////////////////////////////////////
    // Update frame
    HashState32 state;
    dmHashInit32(&state, false);
    dmGameSystem::HashRenderConstants(constants, &state);
    // No need to finalize, since we're not actually looking at the outcome

    result = dmGameSystem::AreRenderConstantsUpdated(constants);
    ASSERT_FALSE(result);

    ////////////////////////////////////////////////////////////////////////
    // Set the same value again, and the "updated" flag should still be set to false
    dmGameSystem::SetRenderConstant(constants, name_hash1, &value, 1);

    result = dmGameSystem::AreRenderConstantsUpdated(constants);
    ASSERT_FALSE(result);

    dmGameSystem::DestroyRenderConstants(constants);
}

#if !defined(DM_PLATFORM_VENDOR) // we need to fix our test material/shader compiler to work with the constants

TEST_F(MaterialTest, CustomInstanceAttributes)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/attributes_instancing_valid.materialc", (void**)&material_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    const dmGraphics::VertexAttributeInfo* attributes;
    uint32_t attribute_count;
    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);
    ASSERT_EQ(5, attribute_count);
    ASSERT_EQ(dmHashString64("position"),   attributes[0].m_NameHash);
    ASSERT_EQ(dmHashString64("normal"),     attributes[1].m_NameHash);
    ASSERT_EQ(dmHashString64("texcoord0"),  attributes[2].m_NameHash);
    ASSERT_EQ(dmHashString64("mtx_normal"), attributes[3].m_NameHash);
    ASSERT_EQ(dmHashString64("mtx_world"),  attributes[4].m_NameHash);

    ASSERT_EQ(2,  attributes[0].m_ElementCount); // Position has been overridden!
    ASSERT_EQ(3,  attributes[1].m_ElementCount);
    ASSERT_EQ(2,  attributes[2].m_ElementCount);
    ASSERT_EQ(9,  attributes[3].m_ElementCount);
    ASSERT_EQ(16, attributes[4].m_ElementCount);

    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, attributes[0].m_SemanticType);
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,     attributes[1].m_SemanticType); // No normal semantic type (yet)
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXCOORD, attributes[2].m_SemanticType);

    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX, attributes[3].m_SemanticType);
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX,  attributes[4].m_SemanticType);

    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[0].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_BYTE,  attributes[1].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_SHORT, attributes[2].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[3].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[4].m_DataType);

    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE, attributes[0].m_StepFunction);
    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_VERTEX,   attributes[1].m_StepFunction);
    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE, attributes[2].m_StepFunction);
    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE, attributes[3].m_StepFunction);
    ASSERT_EQ(dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE, attributes[4].m_StepFunction);

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, CustomVertexAttributes)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/attributes_valid.materialc", (void**)&material_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    const dmGraphics::VertexAttributeInfo* attributes;
    uint32_t attribute_count;

    // Attributes specified in the shader:
    //      attribute vec4 position;
    //      attribute vec3 normal;
    //      attribute vec2 texcoord0;
    //      attribute vec4 color;

    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);
    ASSERT_EQ(4, attribute_count);
    ASSERT_EQ(dmHashString64("position"),  attributes[0].m_NameHash);
    ASSERT_EQ(dmHashString64("normal"),    attributes[1].m_NameHash);
    ASSERT_EQ(dmHashString64("texcoord0"), attributes[2].m_NameHash);
    ASSERT_EQ(dmHashString64("color"),     attributes[3].m_NameHash);

    ASSERT_EQ(2, attributes[0].m_ElementCount); // Position has been overridden!
    ASSERT_EQ(3, attributes[1].m_ElementCount);
    ASSERT_EQ(2, attributes[2].m_ElementCount);
    ASSERT_EQ(3, attributes[3].m_ElementCount);

    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_POSITION, attributes[0].m_SemanticType);
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_NONE,     attributes[1].m_SemanticType); // No normal semantic type (yet)
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXCOORD, attributes[2].m_SemanticType);
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_COLOR,    attributes[3].m_SemanticType);

    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[0].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_BYTE,  attributes[1].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_SHORT, attributes[2].m_DataType);
    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[3].m_DataType);

    const uint8_t* value_ptr;
    uint32_t num_values;

    // Test position values
    {
        dmRender::GetMaterialProgramAttributeValues(material, 0, &value_ptr, &num_values);
        ASSERT_NE((void*) 0x0, value_ptr);
        ASSERT_EQ(2 * sizeof(float), num_values);

        // Note: The attribute has been declared as a vec2.
        float position_expected[] = { 0.0f, 0.0f };
        for (int i = 0; i < 2; ++i)
        {
            ASSERT_NEAR(position_expected[i], ReadUnalignedFloat(value_ptr + i * sizeof(float)), EPSILON);
        }
    }

    // Test normal values
    {
        dmRender::GetMaterialProgramAttributeValues(material, 1, &value_ptr, &num_values);
        ASSERT_NE((void*) 0x0, value_ptr);
        ASSERT_EQ(3, num_values);

        int8_t normal_expected[] = { 64, 32, 16 };
        for (int i = 0; i < 3; ++i)
        {
            ASSERT_EQ(normal_expected[i], value_ptr[i]);
        }
    }

    // Test texcoord values
    {
        dmRender::GetMaterialProgramAttributeValues(material, 2, &value_ptr, &num_values);
        ASSERT_NE((void*) 0x0, value_ptr);
        ASSERT_EQ(2 * sizeof(int16_t), num_values);

        int16_t texcoord0_expected[] = { -16000, 16000 };
        for (int i = 0; i < 2; ++i)
        {
            ASSERT_EQ(texcoord0_expected[i], ReadUnalignedInt16(value_ptr + i * sizeof(int16_t)));
        }
    }

    // Test color values
    {
        dmRender::GetMaterialProgramAttributeValues(material, 3, &value_ptr, &num_values);
        ASSERT_NE((void*) 0x0, value_ptr);
        ASSERT_EQ(3 * sizeof(float), num_values);

        // Note: The attribute specifies more values in the attribute, but in the engine we clamp the values to the element count
        float color_expected[] = { 1.0f, 2.0f, 3.0f };
        for (int i = 0; i < 3; ++i)
        {
            ASSERT_NEAR(color_expected[i], ReadUnalignedFloat(value_ptr + i * sizeof(float)), EPSILON);
        }
    }

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, TextureTransform2DAttribute)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/attributes_texture_transform_valid.materialc", (void**)&material_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    const dmGraphics::VertexAttributeInfo* attributes;
    uint32_t attribute_count;
    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);

    ASSERT_EQ(5u, attribute_count);

    const dmGraphics::VertexAttributeInfo* tt_attr = 0;
    for (uint32_t i = 0; i < attribute_count; ++i)
    {
        if (attributes[i].m_NameHash == dmHashString64("texture_transform_2d"))
        {
            tt_attr = &attributes[i];
            break;
        }
    }
    ASSERT_NE((void*)0, tt_attr);
    ASSERT_EQ(dmGraphics::VertexAttribute::SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D, tt_attr->m_SemanticType);
    ASSERT_EQ(9u, tt_attr->m_ElementCount);
    ASSERT_EQ(dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3, tt_attr->m_VectorType);

    // No value test here, the engine provides data for this semantic type.

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialComponentTest, TextureTransformVertexBuffer)
{
    // Shared material and vertex layout for texture_transform_2d
    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/attributes_texture_transform_valid.materialc", (void**)&material_res));
    ASSERT_NE((void*)0, material_res);

    dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material_res->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    ASSERT_NE((dmGraphics::HVertexDeclaration)0, vx_decl);

    uint32_t vertex_stride = dmGraphics::GetVertexDeclarationStride(vx_decl);
    uint32_t tt_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("texture_transform_2d"));
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, tt_offset);

    float expected_sprite_tt[9];
    ComputeTextureTransformFromTextureSet(m_Factory, "/tile/valid.t.texturesetc", 0u, expected_sprite_tt);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance sprite_go = Spawn(m_Factory, m_Collection, "/sprite/texture_transform_sprite.goc", dmHashString64("/sprite"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, sprite_go);

    dmGameObject::HInstance model_go = Spawn(m_Factory, m_Collection, "/model/texture_transform_model.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, model_go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    /////////////////////////////////////////////////////////////////////////////////////
    // Sprite: vertex buffer should contain transform derived from texture set tex coords
    /////////////////////////////////////////////////////////////////////////////////////
    {
        void* sprite_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
        ASSERT_NE((void*)0, sprite_world);

        dmRender::BufferedRenderBuffer* sprite_vx_buffer = 0;
        dmRender::BufferedRenderBuffer* ix_buffer = 0;
        dmGameSystem::GetSpriteWorldRenderBuffers(sprite_world, &sprite_vx_buffer, &ix_buffer);
        ASSERT_NE((void*)0, sprite_vx_buffer);
        ASSERT_TRUE(sprite_vx_buffer->m_Buffers.Size() > 0);

        const uint32_t sprite_vertex_count = 4;
        dmGraphics::HVertexBuffer sprite_vx_buffer_handle = sprite_vx_buffer->m_Buffers[0];
        ASSERT_EQ(sprite_vertex_count * vertex_stride, dmGraphics::GetVertexBufferSize(sprite_vx_buffer_handle));

        const char* sprite_vb_base = ((dmGraphics::VertexBuffer*)sprite_vx_buffer_handle)->m_Buffer;
        for (int i = 0; i < 9; ++i)
        {
            ASSERT_NEAR(expected_sprite_tt[i], ReadUnalignedFloat(sprite_vb_base + tt_offset + i * sizeof(float)), EPSILON);
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Model: no mesh data for texture_transform_2d, so runtime uses material default (identity mat3) per vertex.
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        uint32_t component_type;
        dmGameObject::HComponent model_component;
        dmGameObject::HComponentWorld model_world;
        dmGameObject::Result res = dmGameObject::GetComponent(model_go, dmHashString64("model"), &component_type, &model_component, &model_world);
        ASSERT_EQ(dmGameObject::RESULT_OK, res);

        uint32_t vx_buffers_count;
        dmRender::BufferedRenderBuffer** vx_buffers;
        dmGameSystem::GetModelWorldRenderBuffers(model_world, &vx_buffers, &vx_buffers_count);
        ASSERT_TRUE(vx_buffers_count > 0);

        dmGraphics::HVertexBuffer model_vx_buffer = vx_buffers[0]->m_Buffers[0];

        uint32_t model_vx_buffer_size = dmGraphics::GetVertexBufferSize(model_vx_buffer);
        uint32_t model_vertex_count = model_vx_buffer_size / vertex_stride;
        ASSERT_GT(model_vertex_count, 0u);

        const float identity_mat3[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };

        const char* model_vb_base = (const char*) dmGraphics::MapVertexBuffer(m_GraphicsContext, model_vx_buffer, dmGraphics::BUFFER_ACCESS_READ_ONLY);
        for (uint32_t v = 0; v < model_vertex_count; ++v)
        {
            for (int i = 0; i < 9; ++i)
            {
                ASSERT_NEAR(identity_mat3[i], ReadUnalignedFloat(model_vb_base + v * vertex_stride + tt_offset + i * sizeof(float)), EPSILON);
            }
        }
        dmGraphics::UnmapVertexBuffer(m_GraphicsContext, model_vx_buffer);
    }

    dmResource::Release(m_Factory, material_res);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MaterialComponentTest, SpriteTextureTransformMultiAtlasVertexBuffer)
{
    float expected_tt0[9];
    float expected_tt1[9];
    ComputeTextureTransformFromTextureSet(m_Factory, "/tile/valid.t.texturesetc", 0u, expected_tt0);
    ComputeTextureTransformFromTextureSet(m_Factory, "/tile/valid2.t.texturesetc", 0u, expected_tt1);

    // Load material to get vertex declaration (stride and offsets for both transforms).
    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK,
              dmResource::Get(m_Factory, "/material/attributes_texture_transform_multi.materialc", (void**)&material_res));
    ASSERT_NE((void*)0, material_res);

    dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material_res->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    ASSERT_NE((dmGraphics::HVertexDeclaration)0, vx_decl);

    uint32_t vertex_stride = dmGraphics::GetVertexDeclarationStride(vx_decl);
    uint32_t tt0_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("texture_transform_2d_0"));
    uint32_t tt1_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("texture_transform_2d_1"));
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, tt0_offset);
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, tt1_offset);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/texture_transform_multi.goc", dmHashString64("/sprite"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    void* sprite_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
    ASSERT_NE((void*)0, sprite_world);

    dmRender::BufferedRenderBuffer* vx_buffer = 0;
    dmRender::BufferedRenderBuffer* ix_buffer = 0;
    dmGameSystem::GetSpriteWorldRenderBuffers(sprite_world, &vx_buffer, &ix_buffer);
    ASSERT_NE((void*)0, vx_buffer);
    ASSERT_TRUE(vx_buffer->m_Buffers.Size() > 0);

    const uint32_t vertex_count = 4;
    dmGraphics::HVertexBuffer vx_buffer_handle = vx_buffer->m_Buffers[0];
    ASSERT_EQ(vertex_count * vertex_stride, dmGraphics::GetVertexBufferSize(vx_buffer_handle));

    const char* vb_base = ((dmGraphics::VertexBuffer*)vx_buffer_handle)->m_Buffer;
    for (int i = 0; i < 9; ++i)
    {
        ASSERT_NEAR(expected_tt0[i], ReadUnalignedFloat(vb_base + tt0_offset + i * sizeof(float)), EPSILON);
        ASSERT_NEAR(expected_tt1[i], ReadUnalignedFloat(vb_base + tt1_offset + i * sizeof(float)), EPSILON);
    }

    dmResource::Release(m_Factory, material_res);
    dmGameObject::Final(m_Collection);
}

TEST_F(MaterialTest, ManyVertexStreamsLoad)
{
    // Material with vertex shader that has more than 8 attribute streams (10 total)
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/attributes_many_streams_valid.materialc", (void**)&material_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    const dmGraphics::VertexAttributeInfo* attributes;
    uint32_t attribute_count;
    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);

    ASSERT_EQ(10u, attribute_count);
    ASSERT_EQ(dmHashString64("position"), attributes[0].m_NameHash);
    for (uint32_t i = 0; i < 9; ++i)
    {
        char name[16];
        dmSnPrintf(name, sizeof(name), "stream%u", i);
        ASSERT_EQ(dmHashString64(name), attributes[1 + i].m_NameHash);
    }

    ASSERT_EQ(2u, attributes[0].m_ElementCount);
    for (uint32_t i = 1; i < 10; ++i)
        ASSERT_EQ(1u, attributes[i].m_ElementCount);

    ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[0].m_DataType);
    for (uint32_t i = 1; i < 10; ++i)
        ASSERT_EQ(dmGraphics::VertexAttribute::TYPE_FLOAT, attributes[i].m_DataType);

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, ManyVertexStreamsAttributeValues)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/attributes_many_streams_valid.materialc", (void**)&material_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    const dmGraphics::VertexAttributeInfo* attributes;
    uint32_t attribute_count;
    dmRender::GetMaterialProgramAttributes(material, &attributes, &attribute_count);
    ASSERT_EQ(10u, attribute_count);

    const uint8_t* value_ptr;
    uint32_t num_values;

    for (uint32_t i = 0; i < 9; ++i)
    {
        dmRender::GetMaterialProgramAttributeValues(material, 1 + i, &value_ptr, &num_values);
        ASSERT_NE((void*)0x0, value_ptr);
        ASSERT_EQ(sizeof(float), num_values);
        float value = *((const float*)value_ptr);
        ASSERT_NEAR((float)i, value, EPSILON);
    }

    dmResource::Release(m_Factory, material_res);
}

struct DynamicVertexAttributesContext
{
    dmArray<dmGraphics::VertexAttribute> m_Attributes;
    bool m_Result;
};

bool Test_GetMaterialAttributeCallback(void* user_data, dmhash_t name_hash, const dmGraphics::VertexAttribute** attribute)
{
    DynamicVertexAttributesContext* ctx = (DynamicVertexAttributesContext*) user_data;

    bool found = false;

    for (int i = 0; i < ctx->m_Attributes.Size(); ++i)
    {
        if (ctx->m_Attributes[i].m_NameHash == name_hash)
        {
            *attribute = &ctx->m_Attributes[i];
            found = true;
            break;
        }
    }

    ctx->m_Result = found;
    return found;
}

template<typename T>
static uint32_t CountOccurences(dmArray<T>& lst, T entry)
{
    uint32_t count = 0;
    for (int i = 0; i < lst.Size(); ++i)
    {
        if (lst[i] == entry)
        {
            count++;
        }
    }
    return count;
}

template<typename T>
static void ValidateVertexAttributeTypeConversion(dmGameSystem::DynamicAttributeInfo& info, uint16_t info_index, dmGraphics::VertexAttribute::DataType data_type, T* expected_values, uint32_t num_values)
{
    uint8_t value_buffer[sizeof(float) * 4 + 1];
    uint8_t* values = value_buffer + 1;

    dmGraphics::VertexAttributeInfo attr = {};
    attr.m_ElementCount = num_values;
    attr.m_DataType = data_type;

    dmGameSystem::ConvertMaterialAttributeValuesToDataType(info, info_index, &attr, values);

    for (int i = 0; i < num_values; ++i)
    {
        T value;
        memcpy(&value, values + sizeof(T) * i, sizeof(T));
        ASSERT_EQ(expected_values[i], value);
    }
}

TEST_F(MaterialTest, DynamicVertexAttributes)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/attributes_valid.materialc", (void**)&material_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    const uint32_t INITIAL_SIZE = 4;

    DynamicVertexAttributesContext ctx;
    ctx.m_Attributes.SetCapacity(INITIAL_SIZE);

    dmGameSystem::DynamicAttributePool dynamic_attribute_pool;
    InitializeMaterialAttributeInfos(dynamic_attribute_pool, INITIAL_SIZE);

    // Attribute not found
    {
        uint16_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        dmGameObject::PropertyDesc desc = {};
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_NOT_FOUND, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("attribute_does_not_exist"), desc, Test_GetMaterialAttributeCallback, &ctx));
    }

    // Attribute(s) found
    {
        uint16_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        dmGameObject::PropertyDesc desc = {};
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("position"), desc, Test_GetMaterialAttributeCallback, &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("normal"), desc, Test_GetMaterialAttributeCallback, &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("texcoord0"), desc, Test_GetMaterialAttributeCallback, &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("color"), desc, Test_GetMaterialAttributeCallback, &ctx));

        // No slots has been taken
        ASSERT_EQ(0, dynamic_attribute_pool.Size());
    }

    // Callback
    {
        uint32_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        dmGameObject::PropertyDesc desc = {};

        float data_buffer[4] = { 13.0f, 14.0f, 15.0f, 16.0f };

        // Create override data
        dmGraphics::VertexAttribute attr;
        attr.m_NameHash                      = dmHashString64("position");
        attr.m_Values.m_BinaryValues.m_Count = sizeof(data_buffer);
        attr.m_Values.m_BinaryValues.m_Data  = (uint8_t*) data_buffer;

        ctx.m_Attributes.Push(attr);

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("position"), desc, Test_GetMaterialAttributeCallback, (void*) &ctx));
        ASSERT_TRUE(ctx.m_Result);

        // note: the material position attribute is only two elements, so we put in a vector3 instead since there are no v2 property values
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);

        ASSERT_NEAR(data_buffer[0], desc.m_Variant.m_V4[0], EPSILON);
        ASSERT_NEAR(data_buffer[1], desc.m_Variant.m_V4[1], EPSILON);
        ASSERT_NEAR(0.0f,           desc.m_Variant.m_V4[2], EPSILON);

        ctx.m_Attributes.SetSize(0);
    }

    // Set a dynamic attribute by vector
    {
        uint16_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        dmhash_t attr_name_hash = dmHashString64("position");

        dmGameObject::PropertyVar var = {};
        dmGameObject::PropertyDesc desc = {};

        var.m_Type  = dmGameObject::PROPERTY_TYPE_VECTOR4;
        var.m_V4[0] = 99.0f;
        var.m_V4[1] = 98.0f;
        var.m_V4[2] = 97.0f;
        var.m_V4[3] = 96.0f;

        ASSERT_EQ(0, dynamic_attribute_pool.Size());

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetMaterialAttribute(dynamic_attribute_pool, &index, material, attr_name_hash, var, Test_GetMaterialAttributeCallback, (void*) &ctx, 0));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, attr_name_hash, desc, Test_GetMaterialAttributeCallback, (void*) &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);

        ASSERT_EQ(1,           dynamic_attribute_pool.Get(0).m_NumInfos);
        ASSERT_NE((void*) 0x0, dynamic_attribute_pool.Get(0).m_Infos);

        // Again, "position" only has two elements, which in turn ends up as a vector three, so we know we will only care about these three values
        ASSERT_NEAR(var.m_V4[0], desc.m_Variant.m_V4[0], EPSILON);
        ASSERT_NEAR(var.m_V4[1], desc.m_Variant.m_V4[1], EPSILON);
        ASSERT_NEAR(0.0f,        desc.m_Variant.m_V4[2], EPSILON);

        // Clear the dynamic proeprty
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, ClearMaterialAttribute(dynamic_attribute_pool, index, attr_name_hash));
        ASSERT_EQ(0, dynamic_attribute_pool.Size());
    }

    // Set a dynamic attribute by value(s)
    {
        uint16_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        dmhash_t attr_name_hash_x    = dmHashString64("position.x");
        dmhash_t attr_name_hash_y    = dmHashString64("position.y");
        dmhash_t attr_name_hash_full = dmHashString64("position");

        dmGameObject::PropertyVar var_x = {};
        dmGameObject::PropertyVar var_y = {};
        dmGameObject::PropertyDesc desc = {};

        var_x.m_Type   = dmGameObject::PROPERTY_TYPE_NUMBER;
        var_x.m_Number = 1337.0f;

        var_y.m_Type   = dmGameObject::PROPERTY_TYPE_NUMBER;
        var_y.m_Number = 666.0f;

        ASSERT_EQ(0, dynamic_attribute_pool.Size());

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetMaterialAttribute(dynamic_attribute_pool, &index, material, attr_name_hash_y, var_y, Test_GetMaterialAttributeCallback, (void*) &ctx, 0));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, attr_name_hash_full, desc, Test_GetMaterialAttributeCallback, (void*) &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);

        ASSERT_EQ(1,           dynamic_attribute_pool.Get(0).m_NumInfos);
        ASSERT_NE((void*) 0x0, dynamic_attribute_pool.Get(0).m_Infos);

        // Should be 0.0f, 666.0f, 0.0f
        ASSERT_NEAR(0.0f,           desc.m_Variant.m_V4[0], EPSILON);
        ASSERT_NEAR(var_y.m_Number, desc.m_Variant.m_V4[1], EPSILON);
        ASSERT_NEAR(0.0f,           desc.m_Variant.m_V4[2], EPSILON);

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetMaterialAttribute(dynamic_attribute_pool, &index, material, attr_name_hash_x, var_x, Test_GetMaterialAttributeCallback, (void*) &ctx, 0));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, attr_name_hash_full, desc, Test_GetMaterialAttributeCallback, (void*) &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);

        // Should be 1337.0f, 666.0f, 0.0f
        ASSERT_NEAR(var_x.m_Number, desc.m_Variant.m_V4[0], EPSILON);
        ASSERT_NEAR(var_y.m_Number, desc.m_Variant.m_V4[1], EPSILON);
        ASSERT_NEAR(0.0f,           desc.m_Variant.m_V4[2], EPSILON);

        // Clear the dynamic proeprty
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, ClearMaterialAttribute(dynamic_attribute_pool, index, attr_name_hash_full));
        ASSERT_EQ(0, dynamic_attribute_pool.Size());
    }

    // Set multiple dynamic attributes (more than original capacity)
    {
        dmArray<uint16_t> allocated_indices;
        allocated_indices.SetCapacity( dmGameSystem::DYNAMIC_ATTRIBUTE_INCREASE_COUNT * 2 + INITIAL_SIZE + 1); // Should equate to three resizes

        dmhash_t attr_name_hash = dmHashString64("position");
        dmGameObject::PropertyVar var = {};
        dmGameObject::PropertyDesc desc = {};

        for (int i = 0; i < allocated_indices.Capacity(); ++i)
        {
            var.m_Number = (float) i;

            uint16_t new_index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetMaterialAttribute(dynamic_attribute_pool, &new_index, material, attr_name_hash, var, Test_GetMaterialAttributeCallback, (void*) &ctx, 0));
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, new_index, material, attr_name_hash, desc, Test_GetMaterialAttributeCallback, (void*) &ctx));

            ASSERT_NEAR(var.m_Number, desc.m_Variant.m_V4[0], EPSILON);
            ASSERT_NEAR(0.0f,         desc.m_Variant.m_V4[1], EPSILON);

            allocated_indices.Push(new_index);
        }

        ASSERT_EQ(INITIAL_SIZE + dmGameSystem::DYNAMIC_ATTRIBUTE_INCREASE_COUNT * 3, dynamic_attribute_pool.Capacity());
        ASSERT_EQ(allocated_indices.Size(), dynamic_attribute_pool.Size());

        for (int i = 0; i < allocated_indices.Capacity(); ++i)
        {
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, allocated_indices[i], material, attr_name_hash, desc, Test_GetMaterialAttributeCallback, (void*) &ctx));
            ASSERT_NEAR((float) i, desc.m_Variant.m_V4[0], EPSILON);

            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, ClearMaterialAttribute(dynamic_attribute_pool, allocated_indices[i], attr_name_hash));
        }

        ASSERT_EQ(0, dynamic_attribute_pool.Size());
    }

    {
        dmGameSystem::DynamicAttributePool tmp_pool;
        tmp_pool.SetCapacity(dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX - 1);

        for (int i = 0; i < tmp_pool.Capacity(); ++i)
        {
            tmp_pool.Alloc();
        }

        dmhash_t attr_name_hash = dmHashString64("position");
        dmGameObject::PropertyVar var = {};
        dmGameObject::PropertyDesc desc = {};

        uint16_t new_index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE, SetMaterialAttribute(tmp_pool, &new_index, material, attr_name_hash, var, Test_GetMaterialAttributeCallback, (void*) &ctx, 0));
    }

    // Data conversion for attribute values
    {
        dmGameSystem::DynamicAttributeInfo::Info info_members[3];

        info_members[0].m_NameHash  = dmHashString64("dynamic_attribute_pos");
        info_members[0].m_Values[0] = 128.0f;
        info_members[0].m_Values[1] = 256.0f;
        info_members[0].m_Values[2] = 32768.0f;
        info_members[0].m_Values[3] = 65536.0f;

        info_members[1].m_NameHash  = dmHashString64("dynamic_attribute_neg");
        info_members[1].m_Values[0] = -128.0f;
        info_members[1].m_Values[1] = -256.0f;
        info_members[1].m_Values[2] = -32768.0f;
        info_members[1].m_Values[3] = -65536.0f;

        dmGameSystem::DynamicAttributeInfo info;
        info.m_Infos    = info_members;
        info.m_NumInfos = DM_ARRAY_SIZE(info_members);

        // TYPE_BYTE
        {
            int8_t expected_values_pos[] = { 127, 127, 127, 127 };
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_BYTE, expected_values_pos, 4);

            int8_t expected_values_neg[] = { -128, -128, -128, -128 };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_BYTE, expected_values_neg, 4);
        }

        // TYPE_UNSIGNED_BYTE
        {
            uint8_t expected_values_pos[] = { 128, 255, 255, 255 };
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE, expected_values_pos, 4);

            uint8_t expected_values_neg[] = { 0, 0, 0, 0 };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE, expected_values_neg, 4);
        }

        // TYPE_SHORT
        {
            int16_t expected_values_pos[] = { 128, 256, 32767, 32767 };
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_SHORT, expected_values_pos, 4);

            int16_t expected_values_neg[] = { -128, -256, -32768, -32768 };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_SHORT, expected_values_neg, 4);
        }

        // TYPE_UNSIGNED_SHORT
        {
            uint16_t expected_values_pos[] = { 128, 256, 32768, 65535};
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT, expected_values_pos, 4);

            uint16_t expected_values_neg[] = { 0, 0, 0, 0 };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT, expected_values_neg, 4);
        }

        // TYPE_INT
        {
            int32_t expected_values_pos[] = { 128, 256, 32768, 65536 };
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_INT, expected_values_pos, 4);

            int32_t expected_values_neg[] = { -128, -256, -32768, -65536 };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_INT, expected_values_neg, 4);
        }

        // TYPE_UNSIGNED_INT
        {
            uint32_t expected_values_pos[] = { 128, 256, 32768, 65536 };
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT, expected_values_pos, 4);

            uint32_t expected_values_neg[] = { 0, 0, 0, 0 };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT, expected_values_neg, 4);
        }

        // TYPE_FLOAT
        {
            float expected_values_pos[] = { 128.0f, 256.0f, 32768.0f, 65536.0f };
            ValidateVertexAttributeTypeConversion(info, 0, dmGraphics::VertexAttribute::TYPE_FLOAT, expected_values_pos, 4);

            float expected_values_neg[] = { -128.0f, -256.0f, -32768.0f, -65536.0f };
            ValidateVertexAttributeTypeConversion(info, 1, dmGraphics::VertexAttribute::TYPE_FLOAT, expected_values_neg, 4);
        }
    }

    // Data conversion for dynamic attributes
    {
        uint16_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        dmhash_t attr_name_hash = dmHashString64("normal");

        dmGameObject::PropertyVar var = {};
        dmGameObject::PropertyDesc desc = {};

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, attr_name_hash, desc, Test_GetMaterialAttributeCallback, (void*) &ctx));
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_VECTOR3, desc.m_Variant.m_Type);

        // Values are from the material
        ASSERT_NEAR(64.0f, desc.m_Variant.m_V4[0], EPSILON);
        ASSERT_NEAR(32.0f, desc.m_Variant.m_V4[1], EPSILON);
        ASSERT_NEAR(16.0f, desc.m_Variant.m_V4[2], EPSILON);

        // Dynamic attribute not set, so can't clear it!
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_NOT_FOUND, ClearMaterialAttribute(dynamic_attribute_pool, index, attr_name_hash));
    }

    dmGameSystem::DestroyMaterialAttributeInfos(dynamic_attribute_pool);

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, DynamicMatrixVertexAttributes)
{
    dmGameSystem::MaterialResource* material_res;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/matrix_attributes.materialc", (void**)&material_res));
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    DynamicVertexAttributesContext ctx;
    ctx.m_Attributes.SetCapacity(1);

    dmGameSystem::DynamicAttributePool dynamic_attribute_pool;
    InitializeMaterialAttributeInfos(dynamic_attribute_pool, 1);
    uint16_t index = dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX;
    dmGameObject::PropertyDesc desc = {};

    const float expected_default_mat3[16] = {
        1.0f, 2.0f, 3.0f, 0.0f,
        4.0f, 5.0f, 6.0f, 0.0f,
        7.0f, 8.0f, 9.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("custom_mat3"), desc, Test_GetMaterialAttributeCallback, &ctx));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_MATRIX4, desc.m_Variant.m_Type);
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(expected_default_mat3[i], desc.m_Variant.m_M4[i], EPSILON);
    }

    const float expected_default_mat2[16] = {
        10.0f, 11.0f, 0.0f, 0.0f,
        12.0f, 13.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("custom_mat2"), desc, Test_GetMaterialAttributeCallback, &ctx));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_MATRIX4, desc.m_Variant.m_Type);
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(expected_default_mat2[i], desc.m_Variant.m_M4[i], EPSILON);
    }

    // Compiled component attributes derive their value count from the vector type
    // and leave the deprecated element count at zero. Also verify that a smaller
    // component matrix is expanded to the material's matrix type.
    float component_mat2[4] = { 21.0f, 22.0f, 23.0f, 24.0f };
    dmGraphics::VertexAttribute component_attribute = {};
    component_attribute.m_NameHash                      = dmHashString64("custom_mat3");
    component_attribute.m_DataType                      = dmGraphics::VertexAttribute::TYPE_FLOAT;
    component_attribute.m_VectorType                    = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT2;
    component_attribute.m_Values.m_BinaryValues.m_Data  = (uint8_t*) component_mat2;
    component_attribute.m_Values.m_BinaryValues.m_Count = sizeof(component_mat2);
    ASSERT_EQ(0u, component_attribute.m_ElementCount);
    ctx.m_Attributes.Push(component_attribute);

    const float expected_component_mat3[16] = {
        21.0f, 22.0f, 0.0f, 0.0f,
        23.0f, 24.0f, 0.0f, 0.0f,
        0.0f,  0.0f,  1.0f, 0.0f,
        0.0f,  0.0f,  0.0f, 1.0f
    };
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("custom_mat3"), desc, Test_GetMaterialAttributeCallback, &ctx));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_MATRIX4, desc.m_Variant.m_Type);
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(expected_component_mat3[i], desc.m_Variant.m_M4[i], EPSILON);
    }
    ctx.m_Attributes.SetSize(0);

    dmGameObject::PropertyVar vector_value(dmVMath::Vector4(1.0f, 2.0f, 3.0f, 4.0f));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_UNSUPPORTED_TYPE, SetMaterialAttribute(dynamic_attribute_pool, &index, material, dmHashString64("custom_mat2"), vector_value, Test_GetMaterialAttributeCallback, &ctx, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_UNSUPPORTED_TYPE, SetMaterialAttribute(dynamic_attribute_pool, &index, material, dmHashString64("custom_mat3"), vector_value, Test_GetMaterialAttributeCallback, &ctx, 0));
    ASSERT_EQ(dmGameSystem::INVALID_DYNAMIC_ATTRIBUTE_INDEX, index);

    dmVMath::Matrix4 matrix_value(
        dmVMath::Vector4(1.0f, 2.0f, 3.0f, 4.0f),
        dmVMath::Vector4(5.0f, 6.0f, 7.0f, 8.0f),
        dmVMath::Vector4(9.0f, 10.0f, 11.0f, 12.0f),
        dmVMath::Vector4(13.0f, 14.0f, 15.0f, 16.0f));
    dmGameObject::PropertyVar matrix_property(matrix_value);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetMaterialAttribute(dynamic_attribute_pool, &index, material, dmHashString64("custom_mat3"), matrix_property, Test_GetMaterialAttributeCallback, &ctx, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("custom_mat3"), desc, Test_GetMaterialAttributeCallback, &ctx));
    const float expected_mat3[16] = {
        1.0f, 2.0f, 3.0f, 0.0f,
        5.0f, 6.0f, 7.0f, 0.0f,
        9.0f, 10.0f, 11.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(expected_mat3[i], desc.m_Variant.m_M4[i], EPSILON);
    }

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetMaterialAttribute(dynamic_attribute_pool, &index, material, dmHashString64("custom_mat2"), matrix_property, Test_GetMaterialAttributeCallback, &ctx, 0));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, GetMaterialAttribute(dynamic_attribute_pool, index, material, dmHashString64("custom_mat2"), desc, Test_GetMaterialAttributeCallback, &ctx));
    const float expected_mat2[16] = {
        1.0f, 2.0f, 0.0f, 0.0f,
        5.0f, 6.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    for (uint32_t i = 0; i < 16; ++i)
    {
        ASSERT_NEAR(expected_mat2[i], desc.m_Variant.m_M4[i], EPSILON);
    }

    DestroyMaterialAttributeInfos(dynamic_attribute_pool);
    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, DynamicVertexAttributesWithGoAnimate)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/attributes_dynamic_go_animate.goc", dmHashString64("/attributes_go_animate"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    for (int i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

#if defined(DM_SANITIZE_ADDRESS) && !defined(_MSC_VER)
// Tests #13108 through the complete script-to-sprite property path: an omitted
// shader attribute must not use a stale element id to shadow a declared vector4.
// Poisoning the fresh MaterialAttribute allocation with the "tint" hash makes the
// go.set() followed by go.animate() failure deterministic under ASan.
TEST_F(MaterialTest, DynamicVertexAttributesWithUninitializedElementIds)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(PoisonNextMaterialAttributeAllocation(dmHashString64("tint"), 7));

    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/attributes_uninitialized_element_ids.materialc", (void**) &material_res));
    ASSERT_TRUE(g_MaterialAttributeAllocationPoison.m_Poisoned);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/attributes_uninitialized_element_ids.goc", dmHashString64("/attributes_uninitialized_element_ids"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));

    bool finalized = dmGameObject::Final(m_Collection);
    dmResource::Release(m_Factory, material_res);

    ASSERT_NE(0, go);
    ASSERT_TRUE(finalized);
}
#endif

TEST_F(MaterialTest, DynamicVertexAttributesGoSetGetSparse)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/attributes_dynamic_go_set_get_sparse.goc", dmHashString64("/attributes_dynamic_go_set_get_sparse"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MaterialTest, DynamicVertexAttributesCount)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    const uint32_t NUM_INSTANCES = 32;

    dmArray<dmGameObject::HInstance> instances;
    instances.SetCapacity(NUM_INSTANCES);
    instances.SetSize(NUM_INSTANCES);

    void* sprite_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
    ASSERT_NE((void*) 0, sprite_world);

    dmGameSystem::DynamicAttributePool* dynamic_attribute_pool = 0;
    GetSpriteWorldDynamicAttributePool(sprite_world, &dynamic_attribute_pool);

    char name_buffer[128] = {};
    for (int i = 0; i < NUM_INSTANCES; ++i)
    {
        dmSnPrintf(name_buffer, sizeof(name_buffer), "/dynamic_attribute_instance_%d", i);

        dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/attributes_dynamic_count.goc", dmHashString64(name_buffer), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
        ASSERT_NE(0, go);
        instances[i] = go;

        ASSERT_EQ((i+1), dynamic_attribute_pool->Size());
    }

    for (int i = 0; i < NUM_INSTANCES; ++i)
    {
        dmGameObject::Delete(m_Collection, instances[i], false);
        // PostUpdate deletes the instance, Delete just flags it for deletion
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        ASSERT_EQ(NUM_INSTANCES - i - 1, dynamic_attribute_pool->Size());
    }

    ASSERT_EQ(0, dynamic_attribute_pool->Size());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Test setting material constants via go.set and go.get
// for both single constants and array constants.
// The test also tests for setting nested structs.
TEST_F(MaterialTest, GoGetSetConstants)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/material.goc", dmHashString64("/material"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MiscTests, MaterialModule)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/misc/material_compute_modules/material_module.goc", dmHashString64("/material_module"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MiscTests, ComputeModule)
{
    dmGameSystem::ComputeResource* compute_program_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/misc/material_compute_modules/compute_module.computec", (void**) &compute_program_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((dmGameSystem::ComputeResource*) 0, compute_program_res);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/misc/material_compute_modules/compute_module.goc", dmHashString64("/compute_module"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmResource::Release(m_Factory, compute_program_res);
}
TEST_F(MaterialTest, TestUniformBuffersLayout)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/uniform_buffers.materialc", (void**)&material_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    dmGraphics::ShaderResourceMember color_intensity_members[2];

    // color : vec3
    color_intensity_members[0].m_Name                 = (char*)"color";
    color_intensity_members[0].m_NameHash             = dmHashString64("color");
    color_intensity_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC3;
    color_intensity_members[0].m_Type.m_UseTypeIndex  = 0;
    color_intensity_members[0].m_ElementCount         = 1;
    color_intensity_members[0].m_Offset               = 0;

    // intensity : float
    color_intensity_members[1].m_Name                 = (char*)"intensity";
    color_intensity_members[1].m_NameHash             = dmHashString64("intensity");
    color_intensity_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
    color_intensity_members[1].m_Type.m_UseTypeIndex  = 0;
    color_intensity_members[1].m_ElementCount         = 1;
    color_intensity_members[1].m_Offset               = 0;

    dmGraphics::ShaderResourceMember light_members[2];

    // position : vec3
    light_members[0].m_Name                 = (char*)"position";
    light_members[0].m_NameHash             = dmHashString64("position");
    light_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC3;
    light_members[0].m_Type.m_UseTypeIndex  = 0;
    light_members[0].m_ElementCount         = 1;
    light_members[0].m_Offset               = 0;

    // color_intensity : ColorIntensity (matches uniform_buffers.fp struct name)
    light_members[1].m_Name                 = (char*)"color_intensity";
    light_members[1].m_NameHash             = dmHashString64("color_intensity");
    light_members[1].m_Type.m_TypeIndex     = 2;   // index into ShaderResourceTypeInfo[]
    light_members[1].m_Type.m_UseTypeIndex  = 1;
    light_members[1].m_ElementCount         = 1;
    light_members[1].m_Offset               = 0;


    dmGraphics::ShaderResourceMember light_data_members[2];

    // lights : Light[4]
    light_data_members[0].m_Name                 = (char*)"lights";
    light_data_members[0].m_NameHash             = dmHashString64("lights");
    light_data_members[0].m_Type.m_TypeIndex     = 1;   // Light
    light_data_members[0].m_Type.m_UseTypeIndex  = 1;
    light_data_members[0].m_ElementCount         = 4;
    light_data_members[0].m_Offset               = 0;

    // light_count : float
    light_data_members[1].m_Name                 = (char*)"light_count";
    light_data_members[1].m_NameHash             = dmHashString64("light_count");
    light_data_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
    light_data_members[1].m_Type.m_UseTypeIndex  = 0;
    light_data_members[1].m_ElementCount         = 1;
    light_data_members[1].m_Offset               = 0;


    dmGraphics::ShaderResourceTypeInfo types[3];

    // LightData (index 0)
    types[0].m_Name        = (char*)"LightData";
    types[0].m_NameHash    = dmHashString64("LightData");
    types[0].m_Members     = light_data_members;
    types[0].m_MemberCount = 2;

    // Light (index 1)
    types[1].m_Name        = (char*)"Light";
    types[1].m_NameHash    = dmHashString64("Light");
    types[1].m_Members     = light_members;
    types[1].m_MemberCount = 2;

    // ColorIntensity (index 2) - must match struct name in uniform_buffers.fp
    types[2].m_Name        = (char*)"ColorIntensity";
    types[2].m_NameHash    = dmHashString64("ColorIntensity");
    types[2].m_Members     = color_intensity_members;
    types[2].m_MemberCount = 2;

    dmGraphics::UpdateShaderTypesOffsets(types, DM_ARRAY_SIZE(types));

    dmGraphics::UniformBufferLayout layout = dmGraphics::GetUniformBufferLayout(0, types, DM_ARRAY_SIZE(types));
    uint32_t layout_size = dmGraphics::GetUniformBufferTypeSize(0, types, DM_ARRAY_SIZE(types));

    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
    const dmGraphics::ShaderMeta* program_meta = dmGraphics::GetShaderMeta(program);

    dmGraphics::UniformBufferLayout built_layout = dmGraphics::GetUniformBufferLayout(0, program_meta->m_TypeInfos.Begin(), program_meta->m_TypeInfos.Size());
    uint32_t built_layout_size = dmGraphics::GetUniformBufferTypeSize(0, program_meta->m_TypeInfos.Begin(), program_meta->m_TypeInfos.Size());

    ASSERT_EQ(layout_size, built_layout_size);
    ASSERT_EQ(layout, built_layout);

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, TestLightBuffer)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/light_buffer.materialc", (void**)&material_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    ASSERT_TRUE(material->m_HasLightBuffer);
    // Set and binding are assigned from the shader's uniform block; ensure they are initialized
    ASSERT_LT(material->m_LightBufferSet, 8u);
    ASSERT_LT(material->m_LightBufferBinding, 32u);
    ASSERT_EQ(32u, material->m_LightBufferCapacity);

    // Verify the material's program declares a LightBuffer with the expected layout
    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
    ASSERT_NE((dmGraphics::HProgram)0, program);
    const dmGraphics::ShaderMeta* program_meta = dmGraphics::GetShaderMeta(program);
    ASSERT_NE((void*)0, program_meta);

    const dmhash_t light_buffer_type = dmHashString64("LightBuffer");
    const dmhash_t lights_member = dmHashString64("lights");
    bool found_light_buffer = false;
    for (uint32_t i = 0; i < program_meta->m_TypeInfos.Size(); ++i)
    {
        const dmGraphics::ShaderResourceTypeInfo& type_info = program_meta->m_TypeInfos[i];
        if (type_info.m_NameHash == light_buffer_type)
        {
            found_light_buffer = true;
            for (uint32_t m = 0; m < type_info.m_MemberCount; ++m)
            {
                if (type_info.m_Members[m].m_NameHash == lights_member)
                {
                    ASSERT_EQ(32, type_info.m_Members[m].m_ElementCount);
                    break;
                }
            }
            break;
        }
    }
    ASSERT_TRUE(found_light_buffer);

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialTest, TestLightBufferSmallerThanProjectMax)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/light_buffer_small.materialc", (void**)&material_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    ASSERT_TRUE(material->m_HasLightBuffer);
    ASSERT_LT(material->m_LightBufferSet, 8u);
    ASSERT_LT(material->m_LightBufferBinding, 32u);
    ASSERT_EQ(4u, material->m_LightBufferCapacity);

    dmResource::Release(m_Factory, material_res);
}

TEST_F(MaterialResourceTest, TestLightBufferWriteIntoUbo)
{
    // Spawns ambient and point-light components, updates their persistent instance data, renders the
    // collection to submit the visible instances, then applies light_buffer.material. The ambient
    // instance is folded into light_info.xyz while only point lights are uploaded to lights[].
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance ambient_go = Spawn(m_Factory, m_Collection, "/light/valid_ambient_light.goc", dmHashString64("/ubo_ambient"), 0, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
    ASSERT_NE((dmGameObject::HInstance)0, ambient_go);

    Point3 positions[10];
    for (uint32_t i = 0; i < 10; ++i)
    {
        char id_buf[32];
        dmSnPrintf(id_buf, sizeof(id_buf), "/lpl%u", i);

        positions[i] = Point3(i * 0.1f, (float) i * 1.5f, (float) i * 2.0f);
        dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64(id_buf), 0, positions[i], Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
        ASSERT_NE((dmGameObject::HInstance)0, go);
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Light data is commited into a scratch buffer before pushing it to the GPU
    ASSERT_EQ(11u, render_ctx->m_LightBufferScratch.Size());
    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_VEC3(positions[i], render_ctx->m_LightBufferScratch[i + 1].m_Position);
    }

    dmRender::BeginFrame(m_RenderContext, 1.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);

    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/light_buffer.materialc", (void**) &material_res));
    ASSERT_NE((void*)0, material_res);
    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);
    ASSERT_TRUE(material->m_HasLightBuffer);

    // Writes the light count into the light uniform buffer
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material);

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    dmGraphics::NullUniformBuffer* ubo = null_context->m_UniformBuffers[material->m_LightBufferSet][material->m_LightBufferBinding];
    ASSERT_NE((void*)0, ubo);
    ASSERT_NE((void*)0, ubo->m_Buffer);
    ASSERT_EQ(dmRender::LIGHT_BUFFER_HEADER_SIZE + render_ctx->m_MaxLightCount * dmRender::LIGHT_BUFFER_LIGHT_STRIDE, ubo->m_BufferSize);

    Vector4 light_info_written;
    memcpy(&light_info_written, ubo->m_Buffer + render_ctx->m_LightBufferInfoWriteStart, sizeof(light_info_written));
    ASSERT_VEC4(Vector4(0.5f, 1.0f, 1.5f, 10.0f), light_info_written);

    const uint32_t light_data_offset = render_ctx->m_LightBufferDataWriteStart;
    const uint32_t light_data_bytes  = 10u * (uint32_t) sizeof(dmRender::LightSTD140);
    ASSERT_LE(light_data_offset + light_data_bytes, ubo->m_BufferSize);
    ASSERT_EQ(0, memcmp(ubo->m_Buffer + light_data_offset, render_ctx->m_LightBufferUploadScratch.Begin(), light_data_bytes));

    dmGameSystem::MaterialResource* small_material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/light_buffer_small.materialc", (void**) &small_material_res));
    ASSERT_NE((void*)0, small_material_res);
    dmRender::HMaterial small_material = small_material_res->m_Material;
    ASSERT_NE((void*)0, small_material);
    ASSERT_TRUE(small_material->m_HasLightBuffer);
    ASSERT_EQ(4u, small_material->m_LightBufferCapacity);

    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, small_material);
    dmGraphics::NullUniformBuffer* small_ubo = null_context->m_UniformBuffers[small_material->m_LightBufferSet][small_material->m_LightBufferBinding];
    ASSERT_NE((void*)0, small_ubo);
    ASSERT_EQ(ubo, small_ubo);
    ASSERT_EQ(dmRender::LIGHT_BUFFER_HEADER_SIZE + render_ctx->m_MaxLightCount * dmRender::LIGHT_BUFFER_LIGHT_STRIDE, small_ubo->m_BufferSize);
    memcpy(&light_info_written, small_ubo->m_Buffer + render_ctx->m_LightBufferInfoWriteStart, sizeof(light_info_written));
    ASSERT_VEC4(Vector4(0.5f, 1.0f, 1.5f, 10.0f), light_info_written);

    // Programs with a smaller declaration share the project-sized buffer and
    // clamp light_info.w to their own MAX_LIGHTS before indexing lights[].
    ASSERT_EQ(0, memcmp(small_ubo->m_Buffer + light_data_offset, render_ctx->m_LightBufferUploadScratch.Begin(), light_data_bytes));

    // Exercise adapter validation: a project-sized buffer is compatible with a
    // shader whose trailing lights[] declaration is smaller.
    dmGraphics::EnableProgram(m_GraphicsContext, small_material->m_Program);
    dmGraphics::Draw(m_GraphicsContext, dmGraphics::PRIMITIVE_TRIANGLES, 0, 0, 0);
    ASSERT_TRUE(small_ubo->m_UsedInDraw);
    ASSERT_EQ(small_ubo, null_context->m_UniformBuffers[small_material->m_LightBufferSet][small_material->m_LightBufferBinding]);
    dmGraphics::DisableProgram(m_GraphicsContext);

    dmGameSystem::MaterialResource* unlit_material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/valid.materialc", (void**) &unlit_material_res));
    ASSERT_NE((void*)0, unlit_material_res);
    dmRender::HMaterial unlit_material = unlit_material_res->m_Material;
    ASSERT_NE((void*)0, unlit_material);
    ASSERT_FALSE(unlit_material->m_HasLightBuffer);

    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, unlit_material);
    ASSERT_EQ((dmGraphics::NullUniformBuffer*) 0, null_context->m_UniformBuffers[material->m_LightBufferSet][material->m_LightBufferBinding]);
    ASSERT_EQ((dmGraphics::NullUniformBuffer*) 0, null_context->m_UniformBuffers[small_material->m_LightBufferSet][small_material->m_LightBufferBinding]);

    dmResource::Release(m_Factory, (void*) unlit_material_res);
    dmResource::Release(m_Factory, (void*) small_material_res);
    dmResource::Release(m_Factory, (void*) material_res);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MaterialResourceTest, TestLightBufferWriteIntoUboAfterDelete)
{
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    Point3 positions[3] =
    {
        Point3(0.0f, 0.0f, 0.0f),
        Point3(1.0f, 2.0f, 3.0f),
        Point3(4.0f, 5.0f, 6.0f)
    };

    dmGameObject::HInstance gos[3];
    for (uint32_t i = 0; i < 3; ++i)
    {
        char id_buf[32];
        dmSnPrintf(id_buf, sizeof(id_buf), "/lpl_delete%u", i);
        gos[i] = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64(id_buf), 0, positions[i], Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
        ASSERT_NE((dmGameObject::HInstance)0, gos[i]);
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::BeginFrame(m_RenderContext, 1.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);

    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/light_buffer.materialc", (void**) &material_res));
    ASSERT_NE((void*)0, material_res);
    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);
    ASSERT_TRUE(material->m_HasLightBuffer);

    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material);

    DeleteInstance(m_Collection, gos[1]);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::BeginFrame(m_RenderContext, 2.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);

    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material);

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    dmGraphics::NullUniformBuffer* ubo = null_context->m_UniformBuffers[material->m_LightBufferSet][material->m_LightBufferBinding];
    ASSERT_NE((void*)0, ubo);
    ASSERT_NE((void*)0, ubo->m_Buffer);

    Vector4 light_info_written;
    memcpy(&light_info_written, ubo->m_Buffer + render_ctx->m_LightBufferInfoWriteStart, sizeof(light_info_written));
    ASSERT_VEC4(Vector4(0.0f, 0.0f, 0.0f, 2.0f), light_info_written);

    const uint32_t light_data_offset = render_ctx->m_LightBufferDataWriteStart;
    dmRender::LightSTD140 uploaded_lights[2];
    memcpy(uploaded_lights, ubo->m_Buffer + light_data_offset, sizeof(uploaded_lights));
    ASSERT_VEC3(positions[0], uploaded_lights[0].m_Position);
    ASSERT_VEC3(positions[2], uploaded_lights[1].m_Position);

    dmResource::Release(m_Factory, (void*) material_res);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MaterialTest, TestLightBufferAbsent)
{
    dmGameSystem::MaterialResource* material_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/material/valid.materialc", (void**)&material_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, material_res);

    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);

    ASSERT_FALSE(material->m_HasLightBuffer);

    dmResource::Release(m_Factory, material_res);
}

#if defined(DM_HAVE_PLATFORM_COMPUTE_SUPPORT)
TEST_F(MaterialResourceTest, TestLightBufferWriteIntoUboCompute)
{
    // Same as TestLightBufferWriteIntoUbo, but dispatches a compute program
    // that declares LightBuffer to exercise the automatic binding path.
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    Point3 positions[10];
    for (uint32_t i = 0; i < 10; ++i)
    {
        char id_buf[32];
        dmSnPrintf(id_buf, sizeof(id_buf), "/lcpl%u", i);

        positions[i] = Point3(i * 0.1f, (float) i * 1.5f, (float) i * 2.0f);
        dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64(id_buf), 0, positions[i], Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
        ASSERT_NE((dmGameObject::HInstance)0, go);
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(10u, render_ctx->m_LightBufferScratch.Size());
    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_VEC3(positions[i], render_ctx->m_LightBufferScratch[i].m_Position);
    }

    dmRender::BeginFrame(m_RenderContext, 1.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);

    dmGameSystem::ComputeResource* compute_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/shader/light_buffer.computec", (void**) &compute_res));
    ASSERT_NE((void*)0, compute_res);
    dmRender::HComputeProgram compute_program = compute_res->m_Program;
    ASSERT_NE((void*)0, compute_program);
    ASSERT_TRUE(compute_program->m_HasLightBuffer);

    render_ctx->m_ComputeProgram = compute_program;
    dmRender::DispatchCompute(m_RenderContext, 1, 1, 1, 0);
    render_ctx->m_ComputeProgram = 0;

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    dmGraphics::NullUniformBuffer* ubo = null_context->m_UniformBuffers[compute_program->m_LightBufferSet][compute_program->m_LightBufferBinding];
    ASSERT_NE((void*)0, ubo);
    ASSERT_NE((void*)0, ubo->m_Buffer);
    ASSERT_EQ(ubo, null_context->m_UniformBuffers[compute_program->m_LightBufferSet][compute_program->m_LightBufferBinding]);
    ASSERT_EQ(dmRender::LIGHT_BUFFER_HEADER_SIZE + render_ctx->m_MaxLightCount * dmRender::LIGHT_BUFFER_LIGHT_STRIDE, ubo->m_BufferSize);

    Vector4 light_info_written;
    memcpy(&light_info_written, ubo->m_Buffer + render_ctx->m_LightBufferInfoWriteStart, sizeof(light_info_written));
    ASSERT_VEC4(Vector4(0.0f, 0.0f, 0.0f, 10.0f), light_info_written);

    const uint32_t light_data_offset = render_ctx->m_LightBufferDataWriteStart;
    const uint32_t light_data_bytes  = 10u * (uint32_t) sizeof(dmRender::LightSTD140);
    ASSERT_LE(light_data_offset + light_data_bytes, ubo->m_BufferSize);
    ASSERT_EQ(0, memcmp(ubo->m_Buffer + light_data_offset, render_ctx->m_LightBufferUploadScratch.Begin(), light_data_bytes));

    dmResource::Release(m_Factory, (void*) compute_res);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}
#endif // DM_HAVE_PLATFORM_COMPUTE_SUPPORT

#endif // !defined(DM_PLATFORM_VENDOR)

#ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT

TEST_F(ShaderTest, Compute)
{
    dmGraphics::ShaderDesc* ddf;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile("build/src/gamesys/test/shader/shader/valid.spc", dmGraphics::ShaderDesc::m_DDFDescriptor, (void**) &ddf));
    ASSERT_EQ(dmGraphics::ShaderDesc::SHADER_TYPE_COMPUTE, ddf->m_Shaders[0].m_ShaderType);
    ASSERT_NE(0, ddf->m_Shaders.m_Count);

    dmGraphics::ShaderDesc::Shader* compute_shader = 0;

    for (int i = 0; i < ddf->m_Shaders.m_Count; ++i)
    {
        if (ddf->m_Shaders[i].m_Language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV ||
            ddf->m_Shaders[i].m_Language == dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430)
        {
            compute_shader = &ddf->m_Shaders[i];
        }
    }
    ASSERT_NE((void*)0, compute_shader);

    // Note: We cannot get this informtion from our shader pipeline for other languages than SPIR-V at the momemnt.
    //       When we can create actual dmGraphics::HProgram from compute we can verify this via the GFX context.
    if (compute_shader->m_Language == dmGraphics::ShaderDesc::LANGUAGE_SPIRV)
    {
        dmGraphics::ShaderDesc::ResourceTypeInfo color_type_info = ddf->m_Reflection.m_Types[ddf->m_Reflection.m_UniformBuffers[0].m_Type.m_Type.m_TypeIndex];

        // Slot 1
        ASSERT_EQ(1,                                        ddf->m_Reflection.m_UniformBuffers.m_Count);
        ASSERT_EQ(dmHashString64("color"),                  color_type_info.m_Members[0].m_NameHash);
        ASSERT_EQ(dmGraphics::ShaderDesc::SHADER_TYPE_VEC4, color_type_info.m_Members[0].m_Type.m_Type.m_ShaderType);
        // Slot 2,
        ASSERT_EQ(1,                                           ddf->m_Reflection.m_Textures.m_Count);
        ASSERT_EQ(dmHashString64("texture_out"),               ddf->m_Reflection.m_Textures[0].m_NameHash);
        ASSERT_EQ(dmGraphics::ShaderDesc::SHADER_TYPE_IMAGE2D, ddf->m_Reflection.m_Textures[0].m_Type.m_Type.m_ShaderType);
    }

    dmDDF::FreeMessage(ddf);
}

TEST_F(ShaderTest, ComputeResource)
{
    dmGameSystem::ComputeResource* compute_program_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/shader/inputs.computec", (void**) &compute_program_res);

    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((dmGameSystem::ComputeResource*) 0, compute_program_res);

    dmRender::HComputeProgram compute_program = compute_program_res->m_Program;

    dmGraphics::HProgram graphics_compute_program = dmRender::GetComputeProgram(compute_program);
    ASSERT_EQ(7, dmGraphics::GetUniformCount(graphics_compute_program));

    dmRender::HConstant ca, cb, cc, cd;
    ASSERT_TRUE(dmRender::GetComputeProgramConstant(compute_program, dmHashString64("buffer_a"), ca));
    ASSERT_TRUE(dmRender::GetComputeProgramConstant(compute_program, dmHashString64("buffer_b"), cb));
    ASSERT_TRUE(dmRender::GetComputeProgramConstant(compute_program, dmHashString64("buffer_c"), cc));
    ASSERT_TRUE(dmRender::GetComputeProgramConstant(compute_program, dmHashString64("buffer_d"), cd));

    uint32_t vca,vcb,vcc,vcd;
    dmVMath::Vector4* va = dmRender::GetConstantValues(ca, &vca);

    dmVMath::Vector4 exp_a(1,2,3,4);
    ASSERT_VEC4(exp_a, (*va));

    dmVMath::Vector4* vb = dmRender::GetConstantValues(cb, &vcb);
    dmVMath::Vector4 exp_b(11,21,31,41);
    ASSERT_VEC4(exp_b, (*vb));

    dmVMath::Vector4* vc = dmRender::GetConstantValues(cc, &vcc);
    dmVMath::Vector4 exp_m_c[] = {
        dmVMath::Vector4(1, 2,  3, 4),
        dmVMath::Vector4(5, 6,  7, 8),
        dmVMath::Vector4(9, 10,11,12),
        dmVMath::Vector4(13,14,15,16),
    };
    ASSERT_VEC4(exp_m_c[0], vc[0]);
    ASSERT_VEC4(exp_m_c[1], vc[1]);
    ASSERT_VEC4(exp_m_c[2], vc[2]);
    ASSERT_VEC4(exp_m_c[3], vc[3]);

    dmVMath::Vector4* vd = dmRender::GetConstantValues(cd, &vcd);
    dmVMath::Vector4 exp_m_d[] = {
        dmVMath::Vector4(11,  12,  13, 14),
        dmVMath::Vector4(15,  16,  17, 18),
        dmVMath::Vector4(19, 110, 111,112),
        dmVMath::Vector4(113,114, 115,116),
    };
    ASSERT_VEC4(exp_m_d[0], vd[0]);
    ASSERT_VEC4(exp_m_d[1], vd[1]);
    ASSERT_VEC4(exp_m_d[2], vd[2]);
    ASSERT_VEC4(exp_m_d[3], vd[3]);

    dmGraphics::HUniformLocation la = dmRender::GetComputeProgramSamplerUnit(compute_program, dmHashString64("texture_a"));
    dmGraphics::HUniformLocation lb = dmRender::GetComputeProgramSamplerUnit(compute_program, dmHashString64("texture_b"));
    dmGraphics::HUniformLocation lc = dmRender::GetComputeProgramSamplerUnit(compute_program, dmHashString64("texture_c"));

    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, la);
    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, lb);
    ASSERT_NE(dmGraphics::INVALID_UNIFORM_LOCATION, lc);

    // Note: texture_a is a storage texture, so we only have two actual samplers here:
    ASSERT_EQ(2, compute_program_res->m_NumTextures);

    dmRender::Sampler* sampler_tex_b = dmRender::GetComputeProgramSampler(compute_program, 0);
    dmRender::Sampler* sampler_tex_c = dmRender::GetComputeProgramSampler(compute_program, 1);

    ASSERT_NE((dmRender::Sampler*) 0, sampler_tex_b);
    ASSERT_EQ(dmHashString64("texture_b"),            sampler_tex_b->m_NameHash);
    ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D,            sampler_tex_b->m_Type);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST,     sampler_tex_b->m_MinFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST,     sampler_tex_b->m_MagFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_REPEAT,        sampler_tex_b->m_UWrap);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_REPEAT,        sampler_tex_b->m_VWrap);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_REPEAT,        sampler_tex_b->m_WWrap);
    ASSERT_NEAR(0.0f, sampler_tex_b->m_MaxAnisotropy, EPSILON);

    ASSERT_NE((dmRender::Sampler*) 0, sampler_tex_c);
    ASSERT_EQ(dmHashString64("texture_c"),             sampler_tex_c->m_NameHash);
    ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D,             sampler_tex_c->m_Type);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR,       sampler_tex_c->m_MinFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR,       sampler_tex_c->m_MagFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE,  sampler_tex_c->m_UWrap);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE,  sampler_tex_c->m_VWrap);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_MIRRORED_REPEAT, sampler_tex_c->m_WWrap);
    ASSERT_NEAR(14.0f, sampler_tex_c->m_MaxAnisotropy, EPSILON);

    dmResource::Release(m_Factory, (void*) compute_program_res);
}

TEST_F(ShaderTest, ComputeLightBuffer)
{
    dmGameSystem::ComputeResource* compute_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/shader/light_buffer.computec", (void**) &compute_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, compute_res);

    dmRender::HComputeProgram compute_program = compute_res->m_Program;
    ASSERT_NE((void*)0, compute_program);

    ASSERT_TRUE(compute_program->m_HasLightBuffer);
    ASSERT_LT(compute_program->m_LightBufferSet, 8u);
    ASSERT_LT(compute_program->m_LightBufferBinding, 32u);
    ASSERT_EQ(32u, compute_program->m_LightBufferCapacity);

    dmGraphics::HProgram program = dmRender::GetComputeProgram(compute_program);
    ASSERT_NE((dmGraphics::HProgram)0, program);
    const dmGraphics::ShaderMeta* program_meta = dmGraphics::GetShaderMeta(program);
    ASSERT_NE((void*)0, program_meta);

    const dmhash_t light_buffer_type = dmHashString64("LightBuffer");
    const dmhash_t lights_member = dmHashString64("lights");
    bool found_light_buffer = false;
    for (uint32_t i = 0; i < program_meta->m_TypeInfos.Size(); ++i)
    {
        const dmGraphics::ShaderResourceTypeInfo& type_info = program_meta->m_TypeInfos[i];
        if (type_info.m_NameHash == light_buffer_type)
        {
            found_light_buffer = true;
            for (uint32_t m = 0; m < type_info.m_MemberCount; ++m)
            {
                if (type_info.m_Members[m].m_NameHash == lights_member)
                {
                    ASSERT_EQ(32, type_info.m_Members[m].m_ElementCount);
                    break;
                }
            }
            break;
        }
    }
    ASSERT_TRUE(found_light_buffer);

    dmResource::Release(m_Factory, (void*) compute_res);
}

TEST_F(ShaderTest, ComputeLightBufferAbsent)
{
    dmGameSystem::ComputeResource* compute_res;
    dmResource::Result res = dmResource::Get(m_Factory, "/shader/valid.computec", (void**) &compute_res);
    ASSERT_EQ(dmResource::RESULT_OK, res);
    ASSERT_NE((void*)0, compute_res);

    dmRender::HComputeProgram compute_program = compute_res->m_Program;
    ASSERT_NE((void*)0, compute_program);
    ASSERT_FALSE(compute_program->m_HasLightBuffer);

    dmResource::Release(m_Factory, (void*) compute_res);
}

#endif

TEST_F(ModelTest, GetAABB)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PlayAnim)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_anim.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "play_anim_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PlayAnimMessage)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_anim_message.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "play_anim_message_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PlayAnimMissingAnimation)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_anim_missing.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "play_anim_missing_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, BlendWeightsScript)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_blend_weights.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "blend_weights_script_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// A single mesh with multiple materials that have different coordinate spaces
// should generate the corresponding batch types. In this case the .gltf file
// has two sub-meshes (RenderItems) with one world space material and one
// local space material. Hence there should be one of each batch rendered.
TEST_F(ModelTest, MultiMaterialVertexSpaceRenderBatching)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/one_mesh_two_materials.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t model_type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc"));
    void*    model_world = dmGameObject::GetWorld(m_Collection, model_type);
    ASSERT_NE((void*)0, model_world);

    uint8_t world_batch_count;
    uint8_t local_batch_count;
    uint8_t local_instanced_batch_count;
    dmGameSystem::GetModelWorldRenderBatchStats(model_world, &world_batch_count, &local_batch_count, &local_instanced_batch_count);
    ASSERT_EQ(1, world_batch_count);
    ASSERT_EQ(1, local_batch_count);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, MorphTargetInstancedWeightsBatch)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go_a = Spawn(m_Factory, m_Collection, "/model/morph_instanced_attr.goc", dmHashString64("/morph_a"), 0, Point3(-1, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go_b = Spawn(m_Factory, m_Collection, "/model/morph_instanced_attr.goc", dmHashString64("/morph_b"), 0, Point3(1, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_a);
    ASSERT_NE(0, go_b);

    uint32_t component_type;
    dmGameObject::HComponent component_a;
    dmGameObject::HComponent component_b;
    dmGameObject::HComponentWorld world;

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go_a, dmHashString64("model"), &component_type, &component_a, &world));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go_b, dmHashString64("model"), &component_type, &component_b, &world));

    const float weights_a[] = { 0.25f, 0.75f };
    const float weights_b[] = { 0.50f, 0.125f };
    dmGameSystem::CompModelSetBlendWeights((dmGameSystem::ModelComponent*) component_a, weights_a, DM_ARRAY_SIZE(weights_a));
    dmGameSystem::CompModelSetBlendWeights((dmGameSystem::ModelComponent*) component_b, weights_b, DM_ARRAY_SIZE(weights_b));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t model_type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc"));
    void*    model_world = dmGameObject::GetWorld(m_Collection, model_type);
    ASSERT_NE((void*)0, model_world);

    uint8_t world_batch_count;
    uint8_t local_batch_count;
    uint8_t local_instanced_batch_count;
    dmGameSystem::GetModelWorldRenderBatchStats(model_world, &world_batch_count, &local_batch_count, &local_instanced_batch_count);
    ASSERT_EQ(0, world_batch_count);
    ASSERT_EQ(0, local_batch_count);
    ASSERT_EQ(1, local_instanced_batch_count);

    struct MorphInstanceData
    {
        float mtx_world[16];
        float mtx_normal[16];
        float morph_targets_weights[16];
    };

    dmRender::BufferedRenderBuffer* instance_buffer = 0;
    dmGameSystem::GetModelWorldInstanceRenderBuffer(model_world, &instance_buffer);
    ASSERT_NE((dmRender::BufferedRenderBuffer*)0, instance_buffer);
    ASSERT_EQ(1u, instance_buffer->m_Buffers.Size());

    dmGraphics::HVertexBuffer vx_buffer_handle = instance_buffer->m_Buffers[0];
    dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer_handle;
    ASSERT_EQ(2u * sizeof(MorphInstanceData), dmGraphics::GetVertexBufferSize(vx_buffer_handle));

    const MorphInstanceData* instances = (const MorphInstanceData*) gfx_vx_buffer->m_Buffer;
    bool found_a = false;
    bool found_b = false;
    for (uint32_t i = 0; i < 2; ++i)
    {
        const float* weights = instances[i].morph_targets_weights;
        found_a |= dmMath::Abs(weights[0] - weights_a[0]) < EPSILON && dmMath::Abs(weights[1] - weights_a[1]) < EPSILON;
        found_b |= dmMath::Abs(weights[0] - weights_b[0]) < EPSILON && dmMath::Abs(weights[1] - weights_b[1]) < EPSILON;
        ASSERT_NEAR(0.0f, weights[2], EPSILON);
        ASSERT_NEAR(0.0f, weights[3], EPSILON);
    }
    ASSERT_TRUE(found_a);
    ASSERT_TRUE(found_b);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, InstancedRenderBufferUsesOneBackingPerDispatch)
{
    dmRender::RenderContext* render_context = (dmRender::RenderContext*) m_RenderContext;
    const bool multi_buffering_required = render_context->m_MultiBufferingRequired;
    render_context->m_MultiBufferingRequired = true;

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/morph_instanced_attr.goc", dmHashString64("/model"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);

    const uint32_t dispatch_count = 3;
    for (uint32_t i = 0; i < dispatch_count; ++i)
    {
        dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    }

    uint32_t model_type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc"));
    void* model_world = dmGameObject::GetWorld(m_Collection, model_type);
    ASSERT_NE((void*)0, model_world);

    dmRender::BufferedRenderBuffer* instance_buffer = 0;
    dmGameSystem::GetModelWorldInstanceRenderBuffer(model_world, &instance_buffer);
    ASSERT_NE((dmRender::BufferedRenderBuffer*)0, instance_buffer);
    ASSERT_EQ(dispatch_count, instance_buffer->m_Buffers.Size());

    render_context->m_MultiBufferingRequired = multi_buffering_required;
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, SelectedMeshUsesInstantiatedMorphModelId)
{
    dmGameSystem::ModelResource* model_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/model/morph_selected.modelc", (void**)&model_resource));
    ASSERT_NE((dmGameSystem::ModelResource*)0, model_resource);
    ASSERT_EQ(1u, model_resource->m_Meshes.Size());

    const dmGameSystem::MeshInfo& mesh_info = model_resource->m_Meshes[0];
    ASSERT_EQ(dmHashString64("MorphSource"), mesh_info.m_Model->m_Id);
    ASSERT_EQ(dmHashString64("MorphedMesh"), mesh_info.m_MorphModelId);

    dmResource::Release(m_Factory, model_resource);
}

TEST_F(ModelTest, MorphTargetUniformWeightsSplitInstancedBatches)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go_a = Spawn(m_Factory, m_Collection, "/model/morph_instanced_legacy.goc", dmHashString64("/morph_a"), 0, Point3(-1, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go_b = Spawn(m_Factory, m_Collection, "/model/morph_instanced_legacy.goc", dmHashString64("/morph_b"), 0, Point3(1, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_a);
    ASSERT_NE(0, go_b);

    uint32_t component_type;
    dmGameObject::HComponent component_a;
    dmGameObject::HComponent component_b;
    dmGameObject::HComponentWorld world;

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go_a, dmHashString64("model"), &component_type, &component_a, &world));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go_b, dmHashString64("model"), &component_type, &component_b, &world));

    const float weights_a[] = { 0.25f, 0.75f };
    const float weights_b[] = { 0.50f, 0.125f };
    dmGameSystem::CompModelSetBlendWeights((dmGameSystem::ModelComponent*) component_a, weights_a, DM_ARRAY_SIZE(weights_a));
    dmGameSystem::CompModelSetBlendWeights((dmGameSystem::ModelComponent*) component_b, weights_b, DM_ARRAY_SIZE(weights_b));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t model_type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc"));
    void*    model_world = dmGameObject::GetWorld(m_Collection, model_type);
    ASSERT_NE((void*)0, model_world);

    uint8_t world_batch_count;
    uint8_t local_batch_count;
    uint8_t local_instanced_batch_count;
    dmGameSystem::GetModelWorldRenderBatchStats(model_world, &world_batch_count, &local_batch_count, &local_instanced_batch_count);
    ASSERT_EQ(0, world_batch_count);
    ASSERT_EQ(0, local_batch_count);
    ASSERT_EQ(2, local_instanced_batch_count);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// One model component can hold meshes with different morph target counts, but the blend weights are
// stored once per component. Each mesh must only apply as many of them as it has morph targets, the
// rest are ignored (model.set_blend_weights). Without the per mesh clamp the mesh with fewer targets
// keeps the extra weights and samples morph texture layers outside its own range, which corrupts it.
// morph_mixed_targets.gltf has one mesh with two targets and one with a single target.
TEST_F(ModelTest, MorphTargetInstancedWeightsClampedPerMesh)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/morph_mixed_targets_attr.goc", dmHashString64("/morph"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go, dmHashString64("model"), &component_type, &component, &world));

    const float weights[] = { 0.25f, 0.75f };
    dmGameSystem::CompModelSetBlendWeights((dmGameSystem::ModelComponent*) component, weights, DM_ARRAY_SIZE(weights));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t model_type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc"));
    void*    model_world = dmGameObject::GetWorld(m_Collection, model_type);
    ASSERT_NE((void*)0, model_world);

    struct MorphInstanceData
    {
        float mtx_world[16];
        float mtx_normal[16];
        float morph_targets_weights[16];
    };

    dmRender::BufferedRenderBuffer* instance_buffer = 0;
    dmGameSystem::GetModelWorldInstanceRenderBuffer(model_world, &instance_buffer);
    ASSERT_NE((dmRender::BufferedRenderBuffer*)0, instance_buffer);
    ASSERT_EQ(1u, instance_buffer->m_Buffers.Size());

    // The meshes are rendered in separate batches, but both write into the same instance buffer.
    dmGraphics::HVertexBuffer vx_buffer_handle = instance_buffer->m_Buffers[0];
    dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer_handle;
    ASSERT_EQ(2u * sizeof(MorphInstanceData), dmGraphics::GetVertexBufferSize(vx_buffer_handle));

    const MorphInstanceData* instances = (const MorphInstanceData*) gfx_vx_buffer->m_Buffer;
    bool found_two_targets = false;
    bool found_one_target = false;
    for (uint32_t i = 0; i < 2; ++i)
    {
        const float* w = instances[i].morph_targets_weights;
        found_two_targets |= dmMath::Abs(w[0] - 0.25f) < EPSILON && dmMath::Abs(w[1] - 0.75f) < EPSILON;
        found_one_target  |= dmMath::Abs(w[0] - 0.25f) < EPSILON && dmMath::Abs(w[1]) < EPSILON;
        ASSERT_NEAR(0.0f, w[2], EPSILON);
        ASSERT_NEAR(0.0f, w[3], EPSILON);
    }
    ASSERT_TRUE(found_two_targets);
    ASSERT_TRUE(found_one_target);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Same per mesh clamp as above, but for materials that take the weights as a uniform instead of an
// instance attribute. The weights are written to a scratch constant buffer per render object.
TEST_F(ModelTest, MorphTargetUniformWeightsClampedPerMesh)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/morph_mixed_targets_legacy.goc", dmHashString64("/morph"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(go, dmHashString64("model"), &component_type, &component, &world));

    const float weights[] = { 0.25f, 0.75f };
    dmGameSystem::CompModelSetBlendWeights((dmGameSystem::ModelComponent*) component, weights, DM_ARRAY_SIZE(weights));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t model_type = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc"));
    void*    model_world = dmGameObject::GetWorld(m_Collection, model_type);
    ASSERT_NE((void*)0, model_world);

    dmGameSystem::HComponentRenderConstants* constant_buffers = 0;
    uint32_t constant_buffer_count = 0;
    dmGameSystem::GetModelWorldScratchConstantBuffers(model_world, &constant_buffers, &constant_buffer_count);
    ASSERT_EQ(2u, constant_buffer_count);

    bool found_two_targets = false;
    bool found_one_target = false;
    for (uint32_t i = 0; i < constant_buffer_count; ++i)
    {
        dmRender::HNamedConstantBuffer buffer = dmGameSystem::GetRenderConstantsNamedBuffer(constant_buffers[i]);

        dmVMath::Vector4* values = 0;
        uint32_t num_values = 0;
        ASSERT_TRUE(dmRender::GetNamedConstant(buffer, dmRender::CONSTANT_MORPH_TARGETS_WEIGHTS, &values, &num_values));
        ASSERT_EQ(1u, num_values);

        found_two_targets |= dmMath::Abs(values[0].getX() - 0.25f) < EPSILON && dmMath::Abs(values[0].getY() - 0.75f) < EPSILON;
        found_one_target  |= dmMath::Abs(values[0].getX() - 0.25f) < EPSILON && dmMath::Abs(values[0].getY()) < EPSILON;
        ASSERT_NEAR(0.0f, values[0].getZ(), EPSILON);
        ASSERT_NEAR(0.0f, values[0].getW(), EPSILON);
    }
    ASSERT_TRUE(found_two_targets);
    ASSERT_TRUE(found_one_target);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, DynamicVertexAttributes)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/dynamic_vertex_attributes.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;

    dmGameObject::Result res = dmGameObject::GetComponent(go, dmHashString64("model"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    // Inspect the render data after render
    dmGraphics::HVertexBuffer vx_buffer;
    dmGraphics::HVertexDeclaration vx_decl;
    dmGraphics::HVertexDeclaration inst_decl;
    dmGameSystem::GetModelComponentAttributeRenderData(component, 0, &vx_buffer, &vx_decl, &inst_decl);

    ASSERT_EQ(4, vx_decl->m_StreamCount);
    ASSERT_EQ(dmHashString64("custom_color"), vx_decl->m_Streams[0].m_NameHash);
    ASSERT_EQ(dmHashString64("custom_transform"), vx_decl->m_Streams[1].m_NameHash);
    ASSERT_EQ(dmHashString64("custom_mat3"), vx_decl->m_Streams[2].m_NameHash);
    ASSERT_EQ(dmHashString64("custom_mat2"), vx_decl->m_Streams[3].m_NameHash);

    // Should be a cube with 24 vertices
    uint32_t exp_num_vertices = 24;
    uint32_t vertex_stride = dmGraphics::GetVertexDeclarationStride(vx_decl);
    uint32_t vx_buffer_size = dmGraphics::GetVertexBufferSize(vx_buffer);
    ASSERT_EQ(exp_num_vertices, vx_buffer_size / vertex_stride);

    uint32_t color_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("custom_color"));
    uint32_t transform_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("custom_transform"));
    uint32_t mat3_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("custom_mat3"));
    uint32_t mat2_offset = dmGraphics::GetVertexStreamOffset(vx_decl, dmHashString64("custom_mat2"));
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, color_offset);
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, transform_offset);
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, mat3_offset);
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, mat2_offset);

    const char* vx_data = (const char*) dmGraphics::MapVertexBuffer(m_GraphicsContext, vx_buffer, dmGraphics::BUFFER_ACCESS_READ_ONLY);

    // This should be the last value that the script "dynamic_vertex_attributes.script" sets
    const float exp_color[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    const float exp_transform[16] = {
        1.0f, 2.0f, 3.0f, 4.0f,
        5.0f, 6.0f, 7.0f, 8.0f,
        9.0f, 10.0f, 11.0f, 12.0f,
        13.0f, 14.0f, 15.0f, 16.0f
    };
    const float exp_mat3[9] = { 1.0f, 2.0f, 3.0f, 5.0f, 6.0f, 7.0f, 9.0f, 10.0f, 11.0f };
    const float exp_mat2[4] = { 1.0f, 2.0f, 5.0f, 6.0f };

    for (int i = 0; i < exp_num_vertices; ++i)
    {
        const char* vertex = vx_data + i * vertex_stride;
        for (uint32_t element = 0; element < 4; ++element)
        {
            ASSERT_NEAR(exp_color[element], ReadUnalignedFloat(vertex + color_offset + element * sizeof(float)), EPSILON);
        }
        for (uint32_t element = 0; element < 16; ++element)
        {
            ASSERT_NEAR(exp_transform[element], ReadUnalignedFloat(vertex + transform_offset + element * sizeof(float)), EPSILON);
        }
        for (uint32_t element = 0; element < 9; ++element)
        {
            ASSERT_NEAR(exp_mat3[element], ReadUnalignedFloat(vertex + mat3_offset + element * sizeof(float)), EPSILON);
        }
        for (uint32_t element = 0; element < 4; ++element)
        {
            ASSERT_NEAR(exp_mat2[element], ReadUnalignedFloat(vertex + mat2_offset + element * sizeof(float)), EPSILON);
        }
    }

    dmGraphics::UnmapVertexBuffer(m_GraphicsContext, vx_buffer);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, MeshAttributeRenderDataPurge)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/dynamic_vertex_attributes.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    // First frame: update, post-update and render once to create attribute render data
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;

    dmGameObject::Result res = dmGameObject::GetComponent(go, dmHashString64("model"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    dmGraphics::HVertexBuffer vx_buffer;
    dmGraphics::HVertexDeclaration vx_decl;
    dmGraphics::HVertexDeclaration inst_decl;

    // After first render, attribute render data must exist
    dmGameSystem::GetModelComponentAttributeRenderData(component, 0, &vx_buffer, &vx_decl, &inst_decl);
    ASSERT_NE((dmGraphics::HVertexBuffer)0, vx_buffer);
    ASSERT_NE((dmGraphics::HVertexDeclaration)0, vx_decl);

    // Advance enough frames without rendering to trigger purge (~30 frames)
    const int kFrames = 40;
    for (int i = 0; i < kFrames; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    }

    // After purge, the cached attribute data should have been released
    dmGameSystem::GetModelComponentAttributeRenderData(component, 0, &vx_buffer, &vx_decl, &inst_decl);
    ASSERT_EQ((dmGraphics::HVertexBuffer)0, vx_buffer);
    ASSERT_EQ((dmGraphics::HVertexDeclaration)0, vx_decl);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PbrProperties)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/pbr_properties.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;
    dmGameSystem::HComponentRenderConstants render_constants;

    ///////////////////////////////
    // Test 1: Test data properties
    ///////////////////////////////
    dmGameObject::Result res = dmGameObject::GetComponent(go, dmHashString64("model"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    GetModelComponentRenderConstants(component, 0, &render_constants);
    ASSERT_NE((dmGameSystem::HComponentRenderConstants)0, render_constants);

    dmRender::HConstant constant = 0;
    dmVMath::Vector4* values     = 0;
    uint32_t num_values          = 0;
    dmVMath::Vector4 exp;

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_BASE_COLOR_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.10f, 0.0f, 0.0f, 0.19f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_METALLIC_AND_ROUGHNESS_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.12f, 0.135f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    // NOTE! Blender uses a "metallic-roguhess" workflow, which means that the "specular" values here will be all 1.0
    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_GLOSSINESS_DIFFUSE_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_GLOSSINESS_SPECULAR_AND_SPECULAR_GLOSSINESS_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_CLEAR_COAT_CLEAR_COAT_AND_CLEAR_COAT_ROUGHNESS_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.16f, 0.165f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_TRANSMISSION_TRANSMISSION_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.175f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_IOR_IOR_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.17f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_SPECULAR_COLOR_AND_SPECULAR_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);

    // JG: I don't know where these values are coming from in blender, so I'm ignoring them for now. It's something like this:
    // exp = dmVMath::Vector4(0.25f, 0.166f, 0.166f, 1.0f);
    // ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_VOLUME_THICKNESS_FACTOR_AND_ATTENUATION_COLOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.2f, 0.205f, 0.210f, 0.215f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_VOLUME_ATTENUATION_DISTANCE, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.22f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SHEEN_SHEEN_COLOR_AND_SHEEN_ROUGHNESS_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.225f, 0.230f, 0.235f, 0.240f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_EMISSIVE_STRENGTH_EMISSIVE_STRENGTH, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.245f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_IRIDESCENCE_IRIDESCENCE_FACTOR_AND_IOR_AND_THICKNESS_MIN_MAX, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.25f, 1.255f, 0.260f, 0.265f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_ALPHA_CUTOFF_AND_DOUBLE_SIDED_AND_IS_UNLIT, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.25f, 1.0f, 1.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    // No textures in this material
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_GLOSSINESS_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_CLEAR_COAT_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_TRANSMISSION_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_VOLUME_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SHEEN_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_IRIDESCENCE_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_COMMON_TEXTURES, &constant));

    ///////////////////////////////////
    // Test 2: Test texture properties
    ///////////////////////////////////
    res = dmGameObject::GetComponent(go, dmHashString64("model_textured"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    GetModelComponentRenderConstants(component, 0, &render_constants);
    ASSERT_NE((dmGameSystem::HComponentRenderConstants) 0, render_constants);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_GLOSSINESS_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_CLEAR_COAT_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_TRANSMISSION_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SPECULAR_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_VOLUME_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_SHEEN_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_IRIDESCENCE_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_COMMON_TEXTURES, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ///////////////////////////////////////////////////////
    // Test 3: Test fallback defaults for meshes with no material entries
    ///////////////////////////////////////////////////////
    res = dmGameObject::GetComponent(go, dmHashString64("model_no_materials"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    GetModelComponentRenderConstants(component, 0, &render_constants);
    ASSERT_NE((dmGameSystem::HComponentRenderConstants) 0, render_constants);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_BASE_COLOR_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_METALLIC_AND_ROUGHNESS_FACTOR, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_TRUE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_ALPHA_CUTOFF_AND_DOUBLE_SIDED_AND_IS_UNLIT, &constant));
    values = dmRender::GetConstantValues(constant, &num_values);
    exp = dmVMath::Vector4(0.5f, 0.0f, 0.0f, 0.0f);
    ASSERT_VEC4(exp, values[0]);

    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_COMMON_TEXTURES, &constant));

    ///////////////////////////////////////////////////////
    // Test 4: Test model texture bindings for meshes with no material entries
    ///////////////////////////////////////////////////////
    res = dmGameObject::GetComponent(go, dmHashString64("model_no_materials_textured"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    GetModelComponentRenderConstants(component, 0, &render_constants);
    ASSERT_NE((dmGameSystem::HComponentRenderConstants) 0, render_constants);

    // The no-material fallback uses default PBR material properties. Model-level texture
    // bindings do not create PBR texture presence constants without glTF material entries.
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_METALLIC_ROUGHNESS_TEXTURES, &constant));
    ASSERT_FALSE(dmGameSystem::GetRenderConstant(render_constants, dmGameSystem::PBR_COMMON_TEXTURES, &constant));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}
