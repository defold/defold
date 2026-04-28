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

#include "test_gamesys.h"
#include "../gamesys_private.h"

#include "../../../../graphics/src/graphics_private.h"
#include "../../../../graphics/src/null/graphics_null_private.h"
#include "../../../../particle/src/particle_private.h"
#include "../../../../render/src/render/render_private.h"
#include "../../../../resource/src/resource_private.h"
#include "../../../../gui/src/gui_private.h"

#include <render/font/fontmap.h>
#include <platform/window.hpp>

#include "gamesys/resources/res_compute.h"
#include "gamesys/resources/res_font.h"
#include "gamesys/resources/res_font_private.h"
#include "gamesys/resources/res_material.h"
#include "gamesys/resources/res_render_target.h"
#include "gamesys/resources/res_ttf.h"
#include "gamesys/resources/res_textureset.h"

#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/memory.h>
#include <dlib/time.h>
#include <dlib/path.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>
#include <dlib/utf8.h>
#include <testmain/testmain.h>

#include <font/fontcollection.h>

#include <ddf/ddf.h>
#include <gameobject/gameobject.h>
#include <gameobject/gameobject_ddf.h>
#include <gameobject/lua_ddf.h>
#include <gameobject/script.h>
#include <gameobject/gameobject_props.h>

#include <gamesys/gamesys_ddf.h>
#include <gamesys/label_ddf.h>
#include <gamesys/sprite_ddf.h>
#include "../components/comp_label.h"
#include "../components/comp_collection_proxy.h"
#include "../scripts/script_sys_gamesys.h"
#include "../scripts/script_resource.h"

#include "resource/layer_guitar_a.ogg.embed.h" // LAYER_GUITAR_A_OGG / LAYER_GUITAR_A_OGG_SIZE
#include "resource/booster_on_sfx.wav.embed.h" // BOOSTER_ON_SFX_WAV / BOOSTER_ON_SFX_WAV_SIZE

#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/gamesys/components/comp_gui.h>
#include <dmsdk/gamesys/resources/res_data.h>
#include <dmsdk/gamesys/resources/res_light.h>

#include <sound/sound.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmVMath;

namespace dmGameObject
{
    HCollection GetCollectionByHash(HRegister regist, dmhash_t socket_name);
}

namespace dmGameSystem
{
    dmGameObject::Result CompCollectionProxyUnloadAsync(HCollectionProxyWorld world, HCollectionProxyComponent proxy, ProxyLoadCallback cbk, void* cbk_ctx);
}

#if !defined(DM_TEST_EXTERN_INIT_FUNCTIONS)
    bool GameSystemTest_PlatformInit()
    {
        return true;
    }
    void GameSystemTest_PlatformExit()
    {
    }
#endif

namespace dmGameSystem
{
    void DumpResourceRefs(dmGameObject::HCollection collection);
    extern void GetSpriteWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer* vx_buffer, dmRender::HBufferedRenderBuffer* ix_buffer);
    extern void GetSpriteWorldDynamicAttributePool(void* sprite_world, DynamicAttributePool** pool_out);
    extern void GetSpriteComponentScale(void* sprite_component, dmVMath::Vector3* scale_out);
    extern uint16_t GetSpriteComponentAnimationIndex(void* sprite_component);
    extern void GetModelWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer** vx_buffers, uint32_t* vx_buffers_count);
    extern void GetModelWorldRenderBatchStats(void* model_world, uint8_t* world_batch_count, uint8_t* local_batch_count, uint8_t* local_instanced_batch_count);
    extern void GetModelComponentRenderConstants(void* model_component, int render_item_ix, dmGameSystem::HComponentRenderConstants* render_constants);
    extern void GetModelComponentAttributeRenderData(void* model_component, int render_item_ix, dmGraphics::HVertexBuffer* vx_buffer, dmGraphics::HVertexDeclaration* vx_decl, dmGraphics::HVertexDeclaration* inst_decl);
    extern void GetParticleFXWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer* vx_buffer);
    extern void GetTileGridWorldRenderBuffers(void* world, dmRender::HBufferedRenderBuffer* vx_buffer);
}

#define EPSILON 0.0001f
#define ASSERT_VEC4(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), EPSILON);\
    ASSERT_NEAR(exp.getW(), act.getW(), EPSILON);

#define ASSERT_VEC3(exp, act)\
    ASSERT_NEAR(exp.getX(), act.getX(), EPSILON);\
    ASSERT_NEAR(exp.getY(), act.getY(), EPSILON);\
    ASSERT_NEAR(exp.getZ(), act.getZ(), EPSILON);

// Reloading these resources needs an update to clear any dirty data and get to a good state.
static const char* update_after_reload[] = {"/tile/valid.tilemapc", "/tile/valid_tilegrid_collisionobject.goc"};

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

static bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

bool CopyResource(const char* src, const char* dst)
{
    char src_path[128];
    dmTestUtil::MakeHostPathf(src_path, sizeof(src_path), "build/src/gamesys/test/%s", src);
    FILE* src_f = fopen(src_path, "rb");
    if (src_f == 0x0)
        return false;
    char dst_path[128];
    dmTestUtil::MakeHostPathf(dst_path, sizeof(dst_path), "build/src/gamesys/test/%s", dst);
    FILE* dst_f = fopen(dst_path, "wb");
    if (dst_f == 0x0)
    {
        fclose(src_f);
        return false;
    }
    char buffer[1024];
    int c = fread(buffer, 1, sizeof(buffer), src_f);
    while (c > 0)
    {
        fwrite(buffer, 1, c, dst_f);
        c = fread(buffer, 1, sizeof(buffer), src_f);
    }

    fclose(src_f);
    fclose(dst_f);

    return true;
}

bool UnlinkResource(const char* name)
{
    char path[128];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s", name);
    return dmSys::Unlink(path) == 0;
}

static FontResult GetGlyph(dmRender::HFontMap font_map, HFont font, uint32_t codepoint, FontGlyph** glyph)
{
    if (!font)
    {
        return FONT_RESULT_ERROR;
    }

    FontResult r = dmRender::GetOrCreateGlyph(font_map, font, codepoint, glyph);
    if ((*glyph))
        (*glyph)->m_Codepoint = codepoint;
    return r;
}



static void DeleteInstance(dmGameObject::HCollection collection, dmGameObject::HInstance instance) {
    dmGameObject::UpdateContext ctx;
    dmGameObject::Update(collection, &ctx);
    dmGameObject::Delete(collection, instance, false);
    dmGameObject::PostUpdate(collection);
}

struct CollectionProxyComponentRef
{
    dmGameSystem::HCollectionProxyWorld m_World;
    dmGameSystem::HCollectionProxyComponent m_Component;
};

static CollectionProxyComponentRef GetCollectionProxyComponentRef(dmGameObject::HInstance instance, dmhash_t component_id)
{
    uint32_t component_type = 0;
    dmGameObject::HComponent component = 0;
    dmGameObject::HComponentWorld world = 0;
    EXPECT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(instance, component_id, &component_type, &component, &world));
    EXPECT_NE((void*)0, component);
    EXPECT_NE((void*)0, world);

    CollectionProxyComponentRef proxy_ref;
    proxy_ref.m_World = (dmGameSystem::HCollectionProxyWorld) world;
    proxy_ref.m_Component = (dmGameSystem::HCollectionProxyComponent) component;
    return proxy_ref;
}

static dmGameObject::HCollection GetCollectionByName(dmGameObject::HRegister regist, const char* name)
{
    dmGameObject::HCollection collection = dmGameObject::GetCollectionByHash(regist, dmHashString64(name));
    EXPECT_NE((void*)0, collection);
    return collection;
}

static void ConfigureCollectionProxy(CollectionProxyComponentRef proxy, const char* collection_path, dmGameObject::Result expected_load_result = dmGameObject::RESULT_OK)
{
    ASSERT_EQ(dmGameSystem::SET_COLLECTION_PATH_RESULT_OK, dmGameSystem::CollectionProxySetCollectionPath(proxy.m_World, proxy.m_Component, collection_path));
    ASSERT_EQ(expected_load_result, dmGameSystem::CompCollectionProxyLoad(proxy.m_World, proxy.m_Component, 0, 0));
    if (expected_load_result != dmGameObject::RESULT_OK)
        return;

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyInitialize(proxy.m_World, proxy.m_Component));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyEnable(proxy.m_World, proxy.m_Component));
}

static void UpdateAndPostUpdateCollection(dmGameObject::HCollection collection, dmGameObject::UpdateContext* update_context, dmGameObject::HRegister regist)
{
    ASSERT_TRUE(dmGameObject::Update(collection, update_context));
    ASSERT_TRUE(dmGameObject::PostUpdate(collection));
    dmGameObject::PostUpdate(regist);
}

TEST_P(ResourceTest, Test)
{
    const char* resource_name = GetParam();
    void* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, resource_name, &resource));
    ASSERT_NE((void*)0, resource);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, resource_name, 0));
    dmResource::Release(m_Factory, resource);
}

TEST_P(ResourceTest, TestPreload)
{
    const char* resource_name = GetParam();
    void* resource;
    dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, resource_name);
    dmResource::Result r;

    uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
    while (dmTime::GetMonotonicTime() < stop_time)
    {
        // Simulate running at 30fps
        r = dmResource::UpdatePreloader(pr, 0, 0, 33*1000);
        if (r != dmResource::RESULT_PENDING)
            break;
        dmTime::Sleep(33*1000);
    }

    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, resource_name, &resource));

    dmResource::DeletePreloader(pr);
    dmResource::Release(m_Factory, resource);
}

TEST_P(ResourceTest, TestPreloadAsync)
{
    const char* resource_name = GetParam();
    void* resource;
    dmResource::HPreloader pr = dmResource::NewPreloader(m_Factory, resource_name);
    dmResource::Result r;

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    null_context->m_UseAsyncTextureLoad   = 1;

    uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
    while (dmTime::GetMonotonicTime() < stop_time)
    {
        // Simulate running at 30fps
        r = dmResource::UpdatePreloader(pr, 0, 0, 33*1000);
        if (r != dmResource::RESULT_PENDING)
            break;
        dmTime::Sleep(33*1000);
    }

    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, resource_name, &resource));

    dmResource::DeletePreloader(pr);
    dmResource::Release(m_Factory, resource);
}

TEST_F(ResourceTest, TestReloadTextureSet)
{
    const char* texture_set_path_a   = "/textureset/valid_a.t.texturesetc";
    const char* texture_set_path_b   = "/textureset/valid_b.t.texturesetc";
    const char* texture_set_path_tmp = "/textureset/tmp.t.texturesetc";

    dmGameSystem::TextureSetResource* resource = NULL;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, texture_set_path_a, (void**) &resource));
    ASSERT_NE((void*)0, resource);

    uint32_t original_width  = dmGraphics::GetOriginalTextureWidth(m_GraphicsContext, resource->m_Texture->m_Texture);
    uint32_t original_height = dmGraphics::GetOriginalTextureHeight(m_GraphicsContext, resource->m_Texture->m_Texture);

    // Swap compiled resources to simulate an atlas update
    ASSERT_TRUE(CopyResource(texture_set_path_a, texture_set_path_tmp));
    ASSERT_TRUE(CopyResource(texture_set_path_b, texture_set_path_a));
    ASSERT_TRUE(CopyResource(texture_set_path_tmp, texture_set_path_b));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, texture_set_path_a, 0));

    // If the load truly was successful, we should have a new width/height for the internal image
    ASSERT_NE(original_width,dmGraphics::GetOriginalTextureWidth(m_GraphicsContext, resource->m_Texture->m_Texture));
    ASSERT_NE(original_height,dmGraphics::GetOriginalTextureHeight(m_GraphicsContext, resource->m_Texture->m_Texture));

    dmResource::Release(m_Factory, (void**) resource);
}

TEST_F(ResourceTest, TestRenderPrototypeResources)
{
    dmGameSystem::RenderScriptPrototype* render_prototype = NULL;
    const char* render_path = "/render/resources.renderc";

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, render_path, (void**) &render_prototype));
    ASSERT_NE((void*)0, render_prototype);
    ASSERT_EQ(3, render_prototype->m_RenderResources.Size());

    HResourceType res_type_render_target;
    HResourceType res_type_material;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetTypeFromExtension(m_Factory, "materialc", &res_type_material));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetTypeFromExtension(m_Factory, "render_targetc", &res_type_render_target));

    HResourceDescriptor rd_mat = dmResource::FindByHash(m_Factory, dmHashString64("/material/valid.materialc"));
    ASSERT_NE((void*)0, rd_mat);

    HResourceDescriptor rd_rt = dmResource::FindByHash(m_Factory, dmHashString64("/render_target/valid.render_targetc"));
    ASSERT_NE((void*)0, rd_rt);

    HResourceType types[] = { res_type_material, res_type_render_target, res_type_material };
    void* resources[] = { dmResource::GetResource(rd_mat), dmResource::GetResource(rd_rt), dmResource::GetResource(rd_mat) };

    for (int i = 0; i < render_prototype->m_RenderResources.Size(); ++i)
    {
        ASSERT_NE((void*)0, render_prototype->m_RenderResources[i]);
        HResourceType res_type;
        dmResource::GetType(m_Factory, render_prototype->m_RenderResources[i], &res_type);
        ASSERT_EQ(types[i], res_type);
        ASSERT_EQ(resources[i], render_prototype->m_RenderResources[i]);
    }

    dmGameSystem::RenderTargetResource* rt = (dmGameSystem::RenderTargetResource*) dmResource::GetResource(rd_rt);
    ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, rt->m_RenderTarget));
    ASSERT_EQ(dmGraphics::ASSET_TYPE_RENDER_TARGET, dmGraphics::GetAssetType(rt->m_RenderTarget));

    dmGraphics::HTexture attachment_0 = dmGraphics::GetRenderTargetTexture(m_GraphicsContext, rt->m_RenderTarget, dmGraphics::BUFFER_TYPE_COLOR0_BIT);
    dmGraphics::HTexture attachment_1 = dmGraphics::GetRenderTargetTexture(m_GraphicsContext, rt->m_RenderTarget, dmGraphics::BUFFER_TYPE_COLOR1_BIT);

    ASSERT_EQ(128, dmGraphics::GetTextureWidth(m_GraphicsContext, attachment_0));
    ASSERT_EQ(128, dmGraphics::GetTextureHeight(m_GraphicsContext, attachment_0));
    ASSERT_EQ(128, dmGraphics::GetTextureWidth(m_GraphicsContext, attachment_1));
    ASSERT_EQ(128, dmGraphics::GetTextureHeight(m_GraphicsContext, attachment_1));
    ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D, dmGraphics::GetTextureType(m_GraphicsContext, attachment_0));
    ASSERT_EQ(dmGraphics::TEXTURE_TYPE_2D, dmGraphics::GetTextureType(m_GraphicsContext, attachment_1));

    dmResource::Release(m_Factory, (void**) render_prototype);
}

TEST_F(ResourceTest, DataResourceContents)
{
    dmGameSystem::DataResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/data/valid.datac", (void**)&resource));
    ASSERT_NE((void*)0, resource);

    const dmGameSystemDDF::Data* ddf = dmGameSystem::GetDDFData(resource);
    ASSERT_NE((void*)0, ddf);

    ASSERT_EQ(2u, ddf->m_Tags.m_Count);
    EXPECT_STREQ("tag-one", ddf->m_Tags[0]);
    EXPECT_STREQ("tag-two", ddf->m_Tags[1]);

    EXPECT_STREQ("hello", ddf->m_Data.m_Kind.m_String);

    dmResource::Release(m_Factory, (void*)resource);
}

TEST_F(ResourceTest, LightResourcePrototype)
{
    /////////////////////////////////
    // Test point light
    /////////////////////////////////
    dmGameSystem::LightResource* res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_point.lightc", (void**)&res));
    ASSERT_NE((void*)0, res); 

    dmRender::HLightPrototype light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);

    const dmRender::LightPrototype* proto = (const dmRender::LightPrototype*) light_prototype;
    ASSERT_EQ(dmRender::LIGHT_TYPE_POINT, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.5f, 0.25f, 1.0f), proto->m_Color);
    ASSERT_NEAR(2.0f, proto->m_Intensity, EPSILON);
    ASSERT_NEAR(10.0f, proto->m_Range, EPSILON);

    dmResource::Release(m_Factory, (void*)res);

    /////////////////////////////////
    // Test directional light
    /////////////////////////////////
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_directional_light.lightc", (void**)&res));
    ASSERT_NE((void*)0, res);

    light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);
    proto = (const dmRender::LightPrototype*)light_prototype;
    ASSERT_EQ(dmRender::LIGHT_TYPE_DIRECTIONAL, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.0f, 0.0f, 1.0f), proto->m_Color);
    ASSERT_NEAR(3.0f, proto->m_Intensity, EPSILON);
    ASSERT_VEC3(dmVMath::Vector3(1.0f, 2.0f, 3.0f), proto->m_Direction);

    dmResource::Release(m_Factory, (void*)res);

    /////////////////////////////////
    // Test spot light
    /////////////////////////////////
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_spot_light.lightc", (void**)&res));
    ASSERT_NE((void*)0, res);

    light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);
    proto = (const dmRender::LightPrototype*)light_prototype;
    ASSERT_EQ(dmRender::LIGHT_TYPE_SPOT, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(0.2f, 0.8f, 0.1f, 1.0f), proto->m_Color);
    ASSERT_NEAR(4.0f, proto->m_Intensity, EPSILON);
    ASSERT_NEAR(20.0f, proto->m_Range, EPSILON);
    ASSERT_NEAR(15.0f, proto->m_InnerConeAngle, EPSILON);
    ASSERT_NEAR(30.0f, proto->m_OuterConeAngle, EPSILON);

    dmResource::Release(m_Factory, (void*)res);
}

TEST_F(ResourceTest, LightComponentUpdatesLightBuffer)
{
    // CompLightLateUpdate calls dmRender::SetLightInstance, which commits into m_LightBufferScratch
    // (same data ApplyMaterialProgramLightBuffers uploads to the GPU light uniform buffer).
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);
    ASSERT_GE(render_ctx->m_MaxLightCount, 3u);

    const Quat rot_id(0.0f, 0.0f, 0.0f, 1.0f);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    const Point3 pos_point(4.0f, 5.0f, 6.0f);
    const Point3 pos_dir(10.0f, 11.0f, 12.0f);
    const Point3 pos_spot(-1.0f, 2.0f, -3.0f);

    dmGameObject::HInstance go_point = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64("/light_point"), 0, pos_point, rot_id, Vector3(1, 1, 1));
    dmGameObject::HInstance go_dir = Spawn(m_Factory, m_Collection, "/light/valid_directional_light.goc", dmHashString64("/light_dir"), 0, pos_dir, rot_id, Vector3(1, 1, 1));
    dmGameObject::HInstance go_spot = Spawn(m_Factory, m_Collection, "/light/valid_spot_light.goc", dmHashString64("/light_spot"), 0, pos_spot, rot_id, Vector3(1, 1, 1));
    ASSERT_NE((dmGameObject::HInstance)0, go_point);
    ASSERT_NE((dmGameObject::HInstance)0, go_dir);
    ASSERT_NE((dmGameObject::HInstance)0, go_spot);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(3u, render_ctx->m_LightBufferScratch.Size());

    // Creation order (point, directional, spot) matches CompLightWorld component order and light buffer indices 0..2
    const dmRender::LightSTD140& L_point = render_ctx->m_LightBufferScratch[0];
    ASSERT_VEC3(pos_point, L_point.m_Position);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.5f, 0.25f, 1.0f), L_point.m_Color);
    ASSERT_VEC4(dmVMath::Vector4(0.0f, 0.0f, 0.0f, 10.0f), L_point.m_DirectionRange);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_POINT, 2.0f, 0.0f, 0.0f), L_point.m_Params);

    const dmRender::LightSTD140& L_dir = render_ctx->m_LightBufferScratch[1];
    ASSERT_VEC3(pos_dir, L_dir.m_Position);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.0f, 0.0f, 1.0f), L_dir.m_Color);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 2.0f, 3.0f, 0.0f), L_dir.m_DirectionRange);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_DIRECTIONAL, 3.0f, 0.0f, 0.0f), L_dir.m_Params);

    const dmRender::LightSTD140& L_spot = render_ctx->m_LightBufferScratch[2];
    ASSERT_VEC3(pos_spot, L_spot.m_Position);
    ASSERT_VEC4(dmVMath::Vector4(0.2f, 0.8f, 0.1f, 1.0f), L_spot.m_Color);
    ASSERT_VEC4(dmVMath::Vector4(0.0f, 0.0f, -1.0f, 20.0f), L_spot.m_DirectionRange);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_SPOT, 4.0f, 15.0f, 30.0f), L_spot.m_Params);

    const Point3 pos_point_moved(7.0f, 8.0f, 9.0f);
    dmGameObject::SetPosition(go_point, pos_point_moved);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_VEC3(pos_point_moved, render_ctx->m_LightBufferScratch[0].m_Position);
    ASSERT_VEC3(pos_dir, render_ctx->m_LightBufferScratch[1].m_Position);
    ASSERT_VEC3(pos_spot, render_ctx->m_LightBufferScratch[2].m_Position);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ResourceTest, ReloadLightResourceTest)
{
    const char* valid_light_a = "/light/valid_point.lightc";
    const char* valid_light_b = "/light/valid_directional_light.lightc";
    const char* tmp_path      = "/light/tmp.lightc";

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64("/light_point"), 0, Point3(0,0,0), Quat(0,0,0,1), Vector3(1, 1, 1));

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;
    dmGameObject::Result res = dmGameObject::GetComponent(go, dmHashString64("light"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    dmGameSystem::LightResource* resource = NULL;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, valid_light_a, (void**) &resource));
    ASSERT_NE((void*)0, resource);

    dmRender::LightPrototype* valid_light_prototype_a = dmGameSystem::GetLightPrototype(resource);
    ASSERT_VEC4(dmVMath::Vector4(1.0, 0.5, 0.25, 1.0), valid_light_prototype_a->m_Color);
    ASSERT_NEAR(2.0, valid_light_prototype_a->m_Intensity, EPSILON);
    ASSERT_NEAR(10.0, valid_light_prototype_a->m_Range, EPSILON);

    ASSERT_TRUE(CopyResource(valid_light_a, tmp_path));
    ASSERT_TRUE(CopyResource(valid_light_b, valid_light_a));
    ASSERT_TRUE(CopyResource(tmp_path, valid_light_b));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, valid_light_a, 0));

    // A reload will not create new internal pointers
    dmRender::LightPrototype* valid_light_prototype_b = dmGameSystem::GetLightPrototype(resource);
    ASSERT_EQ(valid_light_prototype_a, valid_light_prototype_b);

    ASSERT_VEC4(dmVMath::Vector4(1.0, 0.0, 0.0, 1.0), valid_light_prototype_b->m_Color);
    ASSERT_NEAR(3.0, valid_light_prototype_b->m_Intensity, EPSILON);
    ASSERT_VEC3(Vector3(1.0, 2.0, 3.0), valid_light_prototype_b->m_Direction);

    dmResource::Release(m_Factory, (void**) resource);

    ASSERT_TRUE(CopyResource(valid_light_a, tmp_path));
    ASSERT_TRUE(CopyResource(valid_light_b, valid_light_a));
    ASSERT_TRUE(CopyResource(tmp_path, valid_light_b));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

static bool UpdateAndWaitUntilDone(
    dmGameSystem::ScriptLibContext&    scriptlibcontext,
    dmGameObject::HCollection          collection,
    dmGameObject::UpdateContext*       update_context,
    bool                               ignore_script_update_fail,
    const char*                        tests_done_key,
    uint32_t                           timeout_seconds = 1)
{
    uint64_t timeout = timeout_seconds * 1000000; // microseconds
    uint64_t stop_time = dmTime::GetMonotonicTime() + timeout;
    uint64_t frame_time = dmTime::GetMonotonicTime();
    bool tests_done = false;
    while (!tests_done)
    {
        uint64_t now = dmTime::GetMonotonicTime();
        if (now >= stop_time)
        {
            dmLogError("Test timed out after %f seconds", timeout / 1000000.0f);
            break;
        }

        // calculate dt and pass that via the update context
        float dt = (float)((now - frame_time) / 1000000.0);
        update_context->m_DT = dt;
        frame_time = now;

        JobSystemUpdate(scriptlibcontext.m_JobContext, 0);
        dmGameSystem::ScriptSysGameSysUpdate(scriptlibcontext);
        if (!dmGameSystem::GetScriptSysGameSysLastUpdateResult() && !ignore_script_update_fail)
        {
            dmLogError("Test failed on dmGameSystem::GetScriptSysGameSysLastUpdateResult()");
            return false;
        }
        if (!dmGameObject::Update(collection, update_context))
        {
            dmLogError("Test failed on dmGameObject::Update()");
            return false;
        }

        // check if tests are done
        lua_getglobal(scriptlibcontext.m_LuaState, tests_done_key);
        tests_done = lua_toboolean(scriptlibcontext.m_LuaState, -1);
        lua_pop(scriptlibcontext.m_LuaState, 1);

        dmTime::Sleep(30*1000);
    }

    return tests_done;
}

TEST_F(ResourceTest, TestCreateTextureFromScript)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    scriptlibcontext.m_JobContext      = m_JobContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/create_texture.goc", dmHashString64("/create_texture"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 1: create a 128x128 empty texture at "/test.texturec"
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    void* resource = 0x0;
    dmhash_t res_hash = dmHashString64("/test_simple.texturec");
    HResourceDescriptor rd = dmResource::FindByHash(m_Factory, res_hash);
    ASSERT_NE((HResourceDescriptor)0, rd);
    ASSERT_EQ(2, rd->m_ReferenceCount);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test_simple.texturec", &resource));
    ASSERT_TRUE(resource != 0x0);

    // 1 for create 1 for get
    ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, res_hash));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 2: remove resource twice (#model has 1 ref still)
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, res_hash));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 3: fail by using use wrong extension
    //         note that we use a pcall with an inverse assert in create_texture.script
    //         so that we don't need to care about recovering after this point.
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 4: fail by trying to create a resource that already exists
    //         (same as (3) - hence the assert_true)
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 5: fail by trying to release a resource that doesn't exist
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 6: test creating with compressed basis data
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // If the transcode function failed, the texture will be 1x1

    dmGameSystem::TextureResource* texture_res;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test_compressed.texturec", (void**) &texture_res));
    ASSERT_EQ(32, dmGraphics::GetTextureWidth(m_GraphicsContext,texture_res->m_Texture));
    ASSERT_EQ(32, dmGraphics::GetTextureHeight(m_GraphicsContext,texture_res->m_Texture));

    // Release the dmResource::Get call above
    dmResource::Release(m_Factory, texture_res);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 7: fail by using an empty buffer
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // res_texture will make an empty texture here if the test "worked", i.e coulnd't create a valid transcoded texture
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/test_compressed_fail.texturec", (void**) &texture_res));
    ASSERT_EQ(1, dmGraphics::GetTextureWidth(m_GraphicsContext, texture_res->m_Texture));
    ASSERT_EQ(1, dmGraphics::GetTextureHeight(m_GraphicsContext, texture_res->m_Texture));

    // Release the dmResource::Get call again
    dmResource::Release(m_Factory, texture_res);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 8: test getting texture info
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 9: create 0x0 texture
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 10: create 3d texture
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 11: create 2d array texture
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 12: create texture async
    ///////////////////////////////////////////////////////////////////////////////////////////
    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    null_context->m_UseAsyncTextureLoad   = 1;

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Wait until all async operations are done
    ASSERT_TRUE(UpdateAndWaitUntilDone(scriptlibcontext, m_Collection, &m_UpdateContext, false, "async_test_done"));
    ASSERT_TRUE(UpdateAndWaitUntilDone(scriptlibcontext, m_Collection, &m_UpdateContext, false, "async_basis_test_done"));

    // cleanup
    DeleteInstance(m_Collection, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, res_hash));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceTest, TestCreateSoundDataFromScript)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    lua_pushlstring(L, (const char*)LAYER_GUITAR_A_OGG, LAYER_GUITAR_A_OGG_SIZE);
    lua_setglobal(L, "sound_ogg");

    lua_pushlstring(L, (const char*)BOOSTER_ON_SFX_WAV, BOOSTER_ON_SFX_WAV_SIZE);
    lua_setglobal(L, "sound_wav");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/create_sound_data.goc", dmHashString64("/create_sound_data"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    int count = 0;
    bool tests_done = false;
    while (!tests_done && count++ < 100)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L,1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceTest, TestResourceScriptBuffer)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/script_buffer.goc", dmHashString64("/script_buffer"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceTest, TestResourceScriptRenderTarget)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/script_render_target.goc", dmHashString64("/script_render_target"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceTest, TestResourceScriptAtlas)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/script_atlas.goc", dmHashString64("/script_atlas"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceTest, TestSetTextureFromScript)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/set_texture.goc", dmHashString64("/set_texture"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    dmGameSystem::TextureSetResource* texture_set_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/tile/valid.t.texturesetc", (void**) &texture_set_res));

    dmGraphics::HTexture backing_texture = texture_set_res->m_Texture->m_Texture;
    ASSERT_EQ(dmGraphics::GetTextureWidth(m_GraphicsContext, backing_texture), 64);
    ASSERT_EQ(dmGraphics::GetTextureHeight(m_GraphicsContext, backing_texture), 64);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 1: Update a sub-region of the texture
    //      -> set_texture.script::test_success_simple
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 2: Update the entire texture, should be 256x256 after the call
    //      -> set_texture.script::test_success_resize
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(dmGraphics::GetTextureWidth(m_GraphicsContext, backing_texture), 256);
    ASSERT_EQ(dmGraphics::GetTextureHeight(m_GraphicsContext, backing_texture), 256);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 3: Try doing a region update, but outside the texture boundaries, which should fail
    //      -> set_texture.script::test_fail_out_of_bounds
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 4: Try updating the texture with a mipmap that's outside of the allowed range
    //      -> set_texture.script::test_fail_wrong_mipmap
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 5: Set texture with compressed / transcoded data
    //      -> set_texture.script::test_success_compressed
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_EQ(dmGraphics::GetTextureWidth(m_GraphicsContext, backing_texture), 32);
    ASSERT_EQ(dmGraphics::GetTextureHeight(m_GraphicsContext, backing_texture), 32);

    ///////////////////////////////////////////////////////////////////////////////////////////
    // Test 6: Set texture with mipmaps
    //      -> set_texture.script::test_success_mipmap
    ///////////////////////////////////////////////////////////////////////////////////////////
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // cleanup
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmResource::Release(m_Factory, texture_set_res);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(ResourceFailTest, Test)
{
    const ResourceFailParams& p = GetParam();
    const char* tmp_name = "tmp";

    void* resource;
    ASSERT_NE(dmResource::RESULT_OK, dmResource::Get(m_Factory, p.m_InvalidResource, &resource));

    bool exists = CopyResource(p.m_InvalidResource, tmp_name);
    ASSERT_TRUE(CopyResource(p.m_ValidResource, p.m_InvalidResource));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, p.m_InvalidResource, &resource));

    if (exists)
        ASSERT_TRUE(CopyResource(tmp_name, p.m_InvalidResource));
    else
        ASSERT_TRUE(UnlinkResource(p.m_InvalidResource));
    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, p.m_InvalidResource, 0));

    dmResource::Release(m_Factory, resource);

    UnlinkResource(tmp_name);
}

TEST_P(ComponentTest, Test)
{
    const char* go_name = GetParam();
    dmGameObjectDDF::PrototypeDesc* go_ddf;
    char path[128];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s", go_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, (void**)&go_ddf));
    ASSERT_LT(0u, go_ddf->m_Components.m_Count);
    const char* component_name = go_ddf->m_Components[0].m_Component;

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction input_action;
    input_action.m_ActionId = dmHashString64("test_action");
    input_action.m_Value = 1.0f;
    input_action.m_Pressed = 1;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, component_name, 0));

    for (size_t i = 0; i < sizeof(update_after_reload)/sizeof(update_after_reload[0]); ++i)
    {
        if(strcmp(update_after_reload[i], component_name) == 0)
        {
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            break;
        }
    }
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmDDF::FreeMessage(go_ddf);
}

TEST_P(ComponentTest, TestReloadFail)
{
    const char* go_name = GetParam();
    dmGameObjectDDF::PrototypeDesc* go_ddf;
    char path[128];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s", go_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, (void**)&go_ddf));
    ASSERT_LT(0u, go_ddf->m_Components.m_Count);
    const char* component_name = go_ddf->m_Components[0].m_Component;
    const char* temp_name = "tmp";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(CopyResource(component_name, temp_name));
    ASSERT_TRUE(UnlinkResource(component_name));

    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, component_name, 0));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction input_action;
    input_action.m_ActionId = dmHashString64("test_action");
    input_action.m_Value = 1.0f;
    input_action.m_Pressed = 1;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    ASSERT_TRUE(CopyResource(temp_name, component_name));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmDDF::FreeMessage(go_ddf);
}

TEST_F(ComponentTest, CameraTest)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/camera/camera_info.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Test that tries to reload shaders with errors in them.
TEST_F(ComponentTest, ReloadInvalidMaterial)
{
    const char path_material[] = "/material/valid.materialc";
    void* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_material, &resource));

    char path[1024];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test%s", path_material);

    dmRenderDDF::MaterialDesc* ddf = 0;
    dmDDF::Result res = dmDDF::LoadMessageFromFile(path, dmRenderDDF::MaterialDesc::m_DDFDescriptor, (void**) &ddf);
    ASSERT_EQ(dmDDF::RESULT_OK, res);

    const char* program = ddf->m_Program;

    // Modify resource with simulated syntax error
    dmGraphics::SetForceVertexReloadFail(true);

    // Reload, validate fail
    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, program, 0));

    // Modify resource with correction
    dmGraphics::SetForceVertexReloadFail(false);

    // Reload, validate success
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, program, 0));

    // Same as above but for fragment shader
    dmGraphics::SetForceFragmentReloadFail(true);
    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, program, 0));
    dmGraphics::SetForceFragmentReloadFail(false);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, program, 0));

    dmDDF::FreeMessage(ddf);
    dmResource::Release(m_Factory, resource);
}

TEST_P(InvalidVertexSpaceTest, InvalidVertexSpace)
{
    const char* resource_name = GetParam();
    void* resource;
    ASSERT_NE(dmResource::RESULT_OK, dmResource::Get(m_Factory, resource_name, &resource));
}

// Test for input consuming in collection proxy
TEST_F(ComponentTest, ConsumeInputInCollectionProxy)
{
    /* Setup:
    ** go_consume_no
    ** - [script] input_consume_sink.script
    ** go_consume_yes
    ** - collection_proxy
    ** -- go_consume_yes_proxy
    ** ---- [script] input_consume.script
    */

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    #define ASSERT_INPUT_OBJECT_EQUALS(hash) \
    { \
        lua_getglobal(L, "last_input_object"); \
        dmhash_t go_hash = dmScript::CheckHash(L, -1); \
        lua_pop(L,1); \
        ASSERT_EQ(hash,go_hash); \
    }

    const char* path_consume_yes = "/collection_proxy/input_consume_yes.goc";
    const char* path_consume_no  = "/collection_proxy/input_consume_no.goc";

    dmhash_t hash_go_consume_yes   = dmHashString64("/go_consume_yes");
    dmhash_t hash_go_consume_no    = dmHashString64("/go_consume_no");
    dmhash_t hash_go_consume_proxy = dmHashString64("/go_consume_proxy");

    dmGameObject::HInstance go_consume_yes = Spawn(m_Factory, m_Collection, path_consume_yes, hash_go_consume_yes, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_consume_yes);

    dmGameObject::HInstance go_consume_no = Spawn(m_Factory, m_Collection, path_consume_no, hash_go_consume_no, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_consume_no);

    // Iteration 1: Let script send the "enable" message
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Iteration 2: Handle proxy enable and input acquire messages from input_consume_sink.script
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Test 1: input consume in proxy with 1 input action
    dmGameObject::InputAction test_input_action;
    test_input_action.m_ActionId = dmHashString64("test_action_consume");
    test_input_action.m_Pressed  = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &test_input_action, 1));
    ASSERT_EQ(1,test_input_action.m_Consumed);
    ASSERT_INPUT_OBJECT_EQUALS(hash_go_consume_proxy)

    // Test 2: no consuming in proxy collection
    dmGameObject::InputAction test_input_action_consume_no;
    test_input_action_consume_no.m_ActionId = dmHashString64("test_action_consume");
    test_input_action_consume_no.m_Pressed  = 0;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &test_input_action_consume_no, 1));
    ASSERT_EQ(0,test_input_action_consume_no.m_Consumed);
    ASSERT_INPUT_OBJECT_EQUALS(hash_go_consume_no)

    // Test 3: dispatch input queue with more than one input actions that are consumed
    dmGameObject::InputAction test_input_action_queue[2];
    test_input_action_queue[0].m_ActionId = dmHashString64("test_action_consume");
    test_input_action_queue[0].m_Pressed  = 1;
    test_input_action_queue[1].m_ActionId = dmHashString64("test_action_consume");
    test_input_action_queue[1].m_Pressed  = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, test_input_action_queue, 2));
    ASSERT_EQ(1,test_input_action_queue[0].m_Consumed);
    ASSERT_EQ(1,test_input_action_queue[1].m_Consumed);
    ASSERT_INPUT_OBJECT_EQUALS(hash_go_consume_proxy)

    // Test 4: dispatch input queue with more than one input actions where one action is consumed and one isn't
    dmGameObject::InputAction test_input_action_queue_2[2];
    test_input_action_queue_2[0].m_ActionId = dmHashString64("test_action_consume");
    test_input_action_queue_2[0].m_Pressed  = 1;
    test_input_action_queue_2[1].m_ActionId = dmHashString64("test_action_consume");
    test_input_action_queue_2[1].m_Pressed  = 0;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, test_input_action_queue_2, 2));
    ASSERT_EQ(1,test_input_action_queue_2[0].m_Consumed);
    ASSERT_EQ(0,test_input_action_queue_2[1].m_Consumed);
    ASSERT_INPUT_OBJECT_EQUALS(hash_go_consume_no)

    // Test 5: Same as above, but with the action consume order swapped
    dmGameObject::InputAction test_input_action_queue_3[2];
    test_input_action_queue_3[0].m_ActionId = dmHashString64("test_action_consume");
    test_input_action_queue_3[0].m_Pressed  = 0;
    test_input_action_queue_3[1].m_ActionId = dmHashString64("test_action_consume");
    test_input_action_queue_3[1].m_Pressed  = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, test_input_action_queue_3, 2));
    ASSERT_EQ(0,test_input_action_queue_3[0].m_Consumed);
    ASSERT_EQ(1,test_input_action_queue_3[1].m_Consumed);
    ASSERT_INPUT_OBJECT_EQUALS(hash_go_consume_proxy)

    #undef ASSERT_INPUT_OBJECT_EQUALS
}

TEST_F(ComponentTest, CollectionProxySetCollectionLoadInitialize)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    const char* go_path = "/collection_proxy/set_collection_single_root.goc";
    dmhash_t go_hash = dmHashString64("/go");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (;;)
    {
        lua_getglobal(L, "cp_single_target_initialized");
        bool ready = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        if (ready)
            break;

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    lua_getglobal(L, "cp_single_root_set_ok");
    ASSERT_TRUE(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);

    lua_getglobal(L, "cp_single_root_proxy_loaded");
    ASSERT_TRUE(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);

    lua_getglobal(L, "cp_single_target_update_count");
    ASSERT_GE((int)lua_tointeger(L, -1), 1);
    lua_pop(L, 1);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    lua_getglobal(L, "cp_single_root_finalized");
    ASSERT_TRUE(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);

    lua_getglobal(L, "cp_single_target_finalized");
    ASSERT_TRUE(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);
}

TEST_F(ComponentTest, CollectionProxySetCollectionRecursiveLoadInitialize)
{
    const char* go_path = "/collection_proxy/set_collection_cpp_cycle_proxy.goc";
    dmhash_t go_hash = dmHashString64("/go");
    dmhash_t proxy_component_hash = dmHashString64("collectionproxy");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    dmLogInfo("collectionproxy cpp cycle root spawned");
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    CollectionProxyComponentRef root_proxy = GetCollectionProxyComponentRef(go, proxy_component_hash);
    dmLogInfo("collectionproxy cpp cycle root set_collection /collection_proxy/set_collection_cpp_cycle_level1.collectionc");
    ConfigureCollectionProxy(root_proxy, "/collection_proxy/set_collection_cpp_cycle_level1.collectionc");
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    dmGameObject::HCollection level1_collection = GetCollectionByName(m_Register, "set_collection_cpp_cycle_level1");
    dmLogInfo("collectionproxy cpp cycle level1 loaded and initialized");
    dmGameObject::HInstance level1_go = dmGameObject::GetInstanceFromIdentifier(level1_collection, dmHashString64("/go"));
    ASSERT_NE((void*)0, level1_go);
    CollectionProxyComponentRef level1_proxy = GetCollectionProxyComponentRef(level1_go, proxy_component_hash);
    dmLogInfo("collectionproxy cpp cycle level1 set_collection /collection_proxy/set_collection_cpp_cycle_level2.collectionc");
    ConfigureCollectionProxy(level1_proxy, "/collection_proxy/set_collection_cpp_cycle_level2.collectionc");
    UpdateAndPostUpdateCollection(level1_collection, &m_UpdateContext, m_Register);

    dmGameObject::HCollection level2_collection = GetCollectionByName(m_Register, "set_collection_cpp_cycle_level2");
    dmLogInfo("collectionproxy cpp cycle level2 loaded and initialized");
    dmGameObject::HInstance level2_go = dmGameObject::GetInstanceFromIdentifier(level2_collection, dmHashString64("/go"));
    ASSERT_NE((void*)0, level2_go);
    CollectionProxyComponentRef level2_proxy = GetCollectionProxyComponentRef(level2_go, proxy_component_hash);
    dmLogInfo("collectionproxy cpp cycle level2 set_collection /collection_proxy/set_collection_cpp_cycle_level3.collectionc");
    ConfigureCollectionProxy(level2_proxy, "/collection_proxy/set_collection_cpp_cycle_level3.collectionc");
    UpdateAndPostUpdateCollection(level2_collection, &m_UpdateContext, m_Register);

    dmGameObject::HCollection level3_collection = GetCollectionByName(m_Register, "set_collection_cpp_cycle_level3");
    dmLogInfo("collectionproxy cpp cycle level3 loaded and initialized");
    dmGameObject::HInstance level3_go = dmGameObject::GetInstanceFromIdentifier(level3_collection, dmHashString64("/go"));
    ASSERT_NE((void*)0, level3_go);
    CollectionProxyComponentRef level3_proxy = GetCollectionProxyComponentRef(level3_go, proxy_component_hash);

    // Establish the back-edge in C++ without loading it.
    // Loading/enabling this final edge is what drives the recursion scenario.
    dmLogInfo("collectionproxy cpp cycle level3 set_collection /collection_proxy/set_collection_cpp_cycle_level1.collectionc");
    // Try to create a cyclic graph
    ConfigureCollectionProxy(level3_proxy, "/collection_proxy/set_collection_cpp_cycle_level1.collectionc", dmGameObject::RESULT_ALREADY_REGISTERED);

    ASSERT_NE((void*)0, GetCollectionByName(m_Register, "set_collection_cpp_cycle_level1"));
    ASSERT_NE((void*)0, GetCollectionByName(m_Register, "set_collection_cpp_cycle_level2"));
    ASSERT_NE((void*)0, GetCollectionByName(m_Register, "set_collection_cpp_cycle_level3"));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);
}

TEST_P(ComponentFailTest, Test)
{
    const char* go_name = GetParam();

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_EQ((void*)0, go);
}

static void GetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t* out_val) {
    dmGameObject::PropertyDesc desc;
    dmGameObject::PropertyOptions opt;
    dmGameObject::PropertyResult r = dmGameObject::GetProperty(instance, comp_name, prop_name, opt, desc);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    dmGameObject::PropertyType type = desc.m_Variant.m_Type;
    ASSERT_TRUE(dmGameObject::PROPERTY_TYPE_HASH == type);
    *out_val = desc.m_Variant.m_Hash;
}

static dmGameObject::PropertyResult SetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val) {
    dmGameObject::PropertyVar prop_var(in_val);
    dmGameObject::PropertyOptions opt;
    return dmGameObject::SetProperty(instance, comp_name, prop_name, opt, prop_var);
}

static dmhash_t GetHashProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, const dmGameObject::PropertyOptions* options = 0)
{
    dmGameObject::PropertyDesc desc;
    dmGameObject::PropertyOptions default_options;
    dmGameObject::PropertyResult result = dmGameObject::GetProperty(instance, comp_name, prop_name, options ? *options : default_options, desc);

    EXPECT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    if (result != dmGameObject::PROPERTY_RESULT_OK)
        return 0;

    EXPECT_EQ(dmGameObject::PROPERTY_TYPE_HASH, desc.m_Variant.m_Type);
    if (desc.m_Variant.m_Type != dmGameObject::PROPERTY_TYPE_HASH)
        return 0;

    return desc.m_Variant.m_Hash;
}

static dmGameObject::PropertyResult SetHashProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val, const dmGameObject::PropertyOptions* options = 0)
{
    dmGameObject::PropertyVar prop_var(in_val);
    dmGameObject::PropertyOptions default_options;
    return dmGameObject::SetProperty(instance, comp_name, prop_name, options ? *options : default_options, prop_var);
}

class GamesysErrorLogCapture;
static void CaptureGamesysErrorLog(LogSeverity severity, const char* domain, const char* formatted_string);
static GamesysErrorLogCapture* g_GamesysErrorLogCapture = 0;

class GamesysErrorLogCapture
{
public:
    GamesysErrorLogCapture()
    {
        assert(g_GamesysErrorLogCapture == 0);
        g_GamesysErrorLogCapture = this;
        dmLogRegisterListener(CaptureGamesysErrorLog);
    }

    ~GamesysErrorLogCapture()
    {
        dmLogUnregisterListener(CaptureGamesysErrorLog);
        if (g_GamesysErrorLogCapture == this)
            g_GamesysErrorLogCapture = 0;
    }

    void Append(const char* formatted_string)
    {
        uint32_t len = (uint32_t)strlen(formatted_string);
        m_Output.OffsetCapacity(len + 1);
        m_Output.PushArray(formatted_string, len);
    }

    bool Empty() const
    {
        return m_Output.Size() == 0;
    }

    bool Contains(const char* needle)
    {
        if (m_Output.Size() == 0 || m_Output[m_Output.Size() - 1] != '\0')
        {
            m_Output.OffsetCapacity(1);
            m_Output.Push('\0');
        }
        return strstr(m_Output.Begin(), needle) != 0;
    }

private:
    dmArray<char> m_Output;
};

static void CaptureGamesysErrorLog(LogSeverity severity, const char* domain, const char* formatted_string)
{
    if (severity < LOG_SEVERITY_ERROR || g_GamesysErrorLogCapture == 0 || strcmp(domain, "GAMESYS") != 0)
        return;

    if (strstr(formatted_string, "Log server started on port") != 0)
        return;

    g_GamesysErrorLogCapture->Append(formatted_string);
}

static void PostSpritePlayAnimation(dmGameObject::HCollection collection, dmhash_t go_id, dmhash_t component_id, dmhash_t animation_id, float offset, float playback_rate)
{
    dmMessage::URL msg_url;
    dmMessage::ResetURL(&msg_url);
    msg_url.m_Socket = dmGameObject::GetMessageSocket(collection);
    msg_url.m_Path = go_id;
    msg_url.m_Fragment = component_id;

    dmGameSystemDDF::PlayAnimation msg;
    msg.m_Id = animation_id;
    msg.m_Offset = offset;
    msg.m_PlaybackRate = playback_rate;

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::PostDDF(&msg, &msg_url, &msg_url, (uintptr_t)go_id, 0, 0));
}

static void RenderCollection(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    dmRender::RenderListBegin(render_context);
    dmGameObject::Render(collection);
    dmRender::RenderListEnd(render_context);
    dmRender::DrawRenderList(render_context, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    dmRender::ClearRenderObjects(render_context);
}

static dmGameSystem::LabelComponent* GetLabelComponent(dmGameObject::HInstance instance, dmhash_t component_id)
{
    uint32_t component_type = 0;
    dmGameObject::HComponent component = 0;
    dmGameObject::HComponentWorld world = 0;
    EXPECT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(instance, component_id, &component_type, &component, &world));
    EXPECT_NE((void*)0, component);
    return (dmGameSystem::LabelComponent*) component;
}

static void* GetSpriteComponent(dmGameObject::HInstance instance, dmhash_t component_id)
{
    uint32_t component_type = 0;
    dmGameObject::HComponent component = 0;
    dmGameObject::HComponentWorld world = 0;
    EXPECT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(instance, component_id, &component_type, &component, &world));
    EXPECT_NE((void*)0, component);
    return component;
}

static dmGameSystem::GuiComponent* GetGuiComponent(dmGameObject::HCollection collection)
{
    uint32_t component_type_index = dmGameObject::GetComponentTypeIndex(collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world = (dmGameSystem::GuiWorld*) dmGameObject::GetWorld(collection, component_type_index);
    EXPECT_NE((void*)0, gui_world);
    EXPECT_GT(gui_world->m_Components.Size(), 0u);
    return gui_world->m_Components.Size() > 0 ? gui_world->m_Components[0] : 0;
}

static void PostLabelSetText(dmGameObject::HCollection collection, dmhash_t go_id, dmhash_t component_id, const char* text, uintptr_t user_data)
{
    dmMessage::URL url;
    dmMessage::ResetURL(&url);
    url.m_Socket = dmGameObject::GetMessageSocket(collection);
    url.m_Path = go_id;
    url.m_Fragment = component_id;

    uint32_t text_len = strlen(text);
    uint32_t data_size = sizeof(dmGameSystemDDF::SetText) + text_len + 1;
    ASSERT_LE(data_size, dmMessage::DM_MESSAGE_MAX_DATA_SIZE);

    uint8_t data[dmMessage::DM_MESSAGE_MAX_DATA_SIZE];
    dmGameSystemDDF::SetText* message = (dmGameSystemDDF::SetText*)data;
    message->m_Text = (const char*)sizeof(dmGameSystemDDF::SetText);
    memcpy(data + sizeof(dmGameSystemDDF::SetText), text, text_len + 1);

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::Post(&url, &url, dmGameSystemDDF::SetText::m_DDFDescriptor->m_NameHash, user_data, 0, (uintptr_t)dmGameSystemDDF::SetText::m_DDFDescriptor, data, data_size, 0));
}

static HTextLayout SubmitLabelAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection, bool draw, bool clear_render_objects)
{
    dmRender::RenderListBegin(render_context);
    dmGameObject::Render(collection);

    dmRender::RenderContext* render_context_ptr = (dmRender::RenderContext*)render_context;
    EXPECT_EQ(1u, render_context_ptr->m_TextContext.m_TextEntries.Size());
    HTextLayout layout = render_context_ptr->m_TextContext.m_TextEntries.Size() > 0 ? render_context_ptr->m_TextContext.m_TextEntries[0].m_TextLayout : 0;
    EXPECT_EQ(0u, render_context_ptr->m_TextContext.m_TextBuffer.Size());

    dmRender::RenderListEnd(render_context);
    if (draw)
    {
        dmRender::DrawRenderList(render_context, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    }
    if (clear_render_objects)
    {
        dmRender::ClearRenderObjects(render_context);
    }
    return layout;
}

static HTextLayout RenderLabelAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    return SubmitLabelAndGetTextLayout(render_context, collection, true, true);
}

static HTextLayout PrepareLabelAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    return SubmitLabelAndGetTextLayout(render_context, collection, false, true);
}

static HTextLayout QueueLabelAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    return SubmitLabelAndGetTextLayout(render_context, collection, false, false);
}

struct GuiTextSubmitResult
{
    HTextLayout m_TextLayout;
    uint32_t    m_TextEntryCount;
    uint32_t    m_TextBufferSize;
};

static GuiTextSubmitResult SubmitGuiAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection, bool draw, bool clear_render_objects)
{
    GuiTextSubmitResult result = {};

    dmRender::RenderListBegin(render_context);
    dmGameObject::Render(collection);

    dmRender::RenderContext* render_context_ptr = (dmRender::RenderContext*) render_context;
    result.m_TextEntryCount = render_context_ptr->m_TextContext.m_TextEntries.Size();
    result.m_TextBufferSize = render_context_ptr->m_TextContext.m_TextBuffer.Size();
    result.m_TextLayout = result.m_TextEntryCount > 0 ? render_context_ptr->m_TextContext.m_TextEntries[0].m_TextLayout : 0;

    dmRender::RenderListEnd(render_context);
    if (draw)
    {
        dmRender::DrawRenderList(render_context, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    }
    if (clear_render_objects)
    {
        dmRender::ClearRenderObjects(render_context);
    }
    return result;
}

static GuiTextSubmitResult PrepareGuiAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    return SubmitGuiAndGetTextLayout(render_context, collection, false, true);
}

static GuiTextSubmitResult QueueGuiAndGetTextLayout(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    return SubmitGuiAndGetTextLayout(render_context, collection, false, false);
}

static dmhash_t GetTextLayoutGlyphFontPathHash(dmGameSystem::FontResource* font_resource, HTextLayout layout)
{
    EXPECT_NE((HTextLayout)0, layout);
    if (!layout)
        return 0;

    uint32_t glyph_count = TextLayoutGetGlyphCount(layout);
    EXPECT_GT(glyph_count, 0u);
    if (glyph_count == 0)
        return 0;

    return dmGameSystem::ResFontGetPathHashFromFont(font_resource, TextLayoutGetGlyphs(layout)[0].m_Font);
}

static bool FindFallbackCodepoint(HFont primary_font, HFont fallback_font, uint32_t first_codepoint, uint32_t last_codepoint, uint32_t* out_codepoint)
{
    for (uint32_t codepoint = first_codepoint; codepoint <= last_codepoint; ++codepoint)
    {
        if (FontGetGlyphIndex(primary_font, codepoint) == 0 && FontGetGlyphIndex(fallback_font, codepoint) != 0)
        {
            *out_codepoint = codepoint;
            return true;
        }
    }
    return false;
}

TEST_F(BufferMetadataTest, MetadataLuaApi)
{
    // import 'resource' lua api among others
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* go_path = "/buffer/metadata.goc";

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    DeleteInstance(m_Collection, go);

    // release lua api deps
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

struct ScriptComponentTestData
{
    dmGameObject::HCollection m_Collection;
    const char*               m_ComponentType;
};

static int ScriptComponentTestCallback(lua_State* L)
{
    lua_getglobal(L, "test_data");
    ScriptComponentTestData* data = (ScriptComponentTestData*)lua_touserdata(L, -1);
    lua_pop(L, 1);

    dmGameObject::HComponent out_component = 0;
    dmGameObject::GetComponentFromLua(L, 1, data->m_Collection, data->m_ComponentType, &out_component, 0, 0);

    // We should have an actual pointer at this stage, and it is not likely it is less than a certain low number
    lua_pushboolean(L, (uintptr_t)out_component > 100000);
    lua_setglobal(L, "test_done");
    return 0;
}

TEST_P(ScriptComponentTest, GetComponentFromLua)
{
    // import 'resource' lua api among others
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    scriptlibcontext.m_JobContext      = m_JobContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const ScriptComponentTestParams& p = GetParam();
    printf("Testing '%s' with component type '%s', and component '%s'\n", p.m_GOPath, p.m_ComponentType, p.m_ComponentName);

    lua_State* L = scriptlibcontext.m_LuaState;

    lua_pushcfunction(L, ScriptComponentTestCallback);
    lua_setglobal(L, "test_callback");

    ScriptComponentTestData data;
    data.m_Collection = m_Collection;
    data.m_ComponentType = p.m_ComponentType;
    lua_pushlightuserdata(L, &data);
    lua_setglobal(L, "test_data");

    // TODO: Perhaps device a better way of passing the correct url
    dmMessage::URL url;
    dmMessage::ResetURL(&url);
    dmMessage::SetSocket(&url, dmGameObject::GetMessageSocket(m_Collection));
    dmMessage::SetPath(&url, dmHashString64("/go"));
    dmMessage::SetFragment(&url, dmHashString64(p.m_ComponentName));

    dmScript::PushURL(L, url);
    lua_setglobal(L, "test_url");

    // Create gameobject
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, p.m_GOPath, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    EXPECT_TRUE(UpdateAndWaitUntilDone(scriptlibcontext, m_Collection, &m_UpdateContext, false, "test_done"));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    // release GO
    DeleteInstance(m_Collection, go);

    // release lua api deps
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(SoundTest, UpdateSoundResource)
{
    // import 'resource' lua api among others
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* go_path = "/sound/updated_sound.goc";
    dmhash_t comp_name = dmHashString64("dynamic-sound"); // id of soundc component
    dmhash_t prop_name = dmHashString64("sound"); // property of sound data resource within a sound component

    // Create gameobject
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    // Get hash of the sounddata resource
    dmhash_t soundata_hash = 0;
    GetResourceProperty(go, comp_name, prop_name, &soundata_hash);

    HResourceDescriptor descp = dmResource::FindByHash(m_Factory, soundata_hash);
    dmLogInfo("Original size: %d", descp->m_ResourceSize);
    ASSERT_EQ(42270+32, dmResource::GetResourceSize(descp));  // valid.wav. Size returned is always +16 from size of wav: sound_data->m_Size + sizeof(SoundData) from sound_null.cpp;

    // Update sound component with custom buffer from lua. See set_sound.script:update()
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Check the size of the updated resource

    descp = dmResource::FindByHash(m_Factory, soundata_hash);
    dmLogInfo("New size: %d", descp->m_ResourceSize);
    ASSERT_EQ(98510+32, descp->m_ResourceSize);  // replacement.wav. Size returned is always +16 from size of wav: sound_data->m_Size + sizeof(SoundData) from sound_null.cpp;

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    // release GO
    DeleteInstance(m_Collection, go);

    // release lua api deps
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(SoundTest, LuaCallback)
{
    // import 'resource' lua api among others
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* go_path = "/sound/luacallback.goc";

    // Create gameobject
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    // Update sound component with custom buffer from lua. See set_sound.script:update()
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Update sound system once to ensure state updates etc.
    dmSound::Update();

    // Allow for one more update for messages to go through
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    // release GO
    DeleteInstance(m_Collection, go);

    // release lua api deps
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(SoundTest, DelayedSoundStoppedBeforePlay)
{
    // import 'resource' lua api among others
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* go_path = "/sound/delayed_sound_stopped_before_play.goc";

    // Create gameobject
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    lua_State* L = scriptlibcontext.m_LuaState;

    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    // release GO
    DeleteInstance(m_Collection, go);

    // release lua api deps
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(SoundTest, LuaSetSpeedToZero)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    scriptlibcontext.m_JobContext      = m_JobContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* go_path = "/sound/set_speed_zero.goc";
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    EXPECT_TRUE(UpdateAndWaitUntilDone(scriptlibcontext, m_Collection, &m_UpdateContext, false, "tests_done"));

    dmGameObject::PropertyDesc property_desc;
    dmGameObject::PropertyOptions property_opt;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, dmHashString64("sound"), dmHashString64("speed"), property_opt, property_desc));
    ASSERT_EQ(0.0f, property_desc.m_Variant.m_Number);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    DeleteInstance(m_Collection, go);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}


TEST_P(ResourcePropTest, ResourceRefCounting)
{
    const char* go_path = "/resource/res_getset_prop.goc";
    const ResourcePropParams& p = GetParam();
    void* resources[] = {0x0, 0x0, 0x0};
    const char* paths[] = {p.m_ResourcePath, p.m_ResourcePathNotFound, p.m_ResourcePathInvExt};
    dmhash_t path_hashes[] = {0, 0, 0};
    // Acquire new resource
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (*paths[i] != 0) {
            path_hashes[i] = dmHashString64(paths[i]);
            dmResource::Result res = dmResource::Get(m_Factory, paths[i], &resources[i]);
            if (i == 1) { // second resource is non-existing by design
                ASSERT_EQ(dmResource::RESULT_RESOURCE_NOT_FOUND, res);
            } else {
                ASSERT_EQ(dmResource::RESULT_OK, res);
            }
        }
    }
    dmhash_t prop_name = dmHashString64(p.m_PropertyName);
    dmhash_t new_res_hash = path_hashes[0];
    dmhash_t new_res_hash_not_found = path_hashes[1];
    dmhash_t new_res_hash_inv_ext = path_hashes[2];

    const char* component_name[] = {p.m_Component0, p.m_Component1, p.m_Component2, p.m_Component3, p.m_Component4, p.m_Component5};
    for(uint32_t i = 0; i < 6; ++i)
    {
        if(component_name[i] == 0)
            break;
        dmhash_t comp_name = dmHashString64(component_name[i]);

        // Spawn a go with all supported component types
        dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
        ASSERT_NE((void*)0, go);

        dmhash_t orig_res_hash;
        GetResourceProperty(go, comp_name, prop_name, &orig_res_hash);

        // Spawn is expected to inc the ref count
        uint32_t orig_rc = dmResource::GetRefCount(m_Factory, orig_res_hash);
        ASSERT_LT(0u, orig_rc);
        uint32_t new_rc = dmResource::GetRefCount(m_Factory, new_res_hash);
        ASSERT_LT(0u, new_rc);

        // Spawn/delete are balanced w.r.t ref count
        DeleteInstance(m_Collection, go);
        go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
        ASSERT_EQ(orig_rc, dmResource::GetRefCount(m_Factory, orig_res_hash));

        // Graceful failure when resource does not exist
        if (new_res_hash_not_found != 0)
        {
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND, SetResourceProperty(go, comp_name, prop_name, new_res_hash_not_found));
            ASSERT_EQ(orig_rc, dmResource::GetRefCount(m_Factory, orig_res_hash));
        }

        // Graceful failure when resource has incorrect extension
        if (new_res_hash_inv_ext != 0)
        {
            ASSERT_EQ(dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE, SetResourceProperty(go, comp_name, prop_name, new_res_hash_inv_ext));
            ASSERT_EQ(orig_rc, dmResource::GetRefCount(m_Factory, orig_res_hash));
        }

        // No release when setting prop to different resource
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetResourceProperty(go, comp_name, prop_name, new_res_hash));
        dmhash_t res_hash;
        GetResourceProperty(go, comp_name, prop_name, &res_hash);
        ASSERT_EQ(new_res_hash, res_hash);
        ASSERT_EQ(orig_rc, dmResource::GetRefCount(m_Factory, orig_res_hash));
        ASSERT_EQ(new_rc + 1, dmResource::GetRefCount(m_Factory, new_res_hash));

        // Acquire it again when setting prop back to original
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetResourceProperty(go, comp_name, prop_name, orig_res_hash));
        ASSERT_EQ(orig_rc + 1, dmResource::GetRefCount(m_Factory, orig_res_hash));
        ASSERT_EQ(new_rc, dmResource::GetRefCount(m_Factory, new_res_hash));

        // Setting to same val has no effect on ref counting
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetResourceProperty(go, comp_name, prop_name, orig_res_hash));
        ASSERT_EQ(orig_rc + 1, dmResource::GetRefCount(m_Factory, orig_res_hash));

        DeleteInstance(m_Collection, go);
    }
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (resources[i]) {
            dmResource::Release(m_Factory, resources[i]);
        }
    }
}

TEST_F(ComponentTest, ModelTexturePropertyAllTextureSlots)
{
    const char* go_path = "/resource/res_getset_prop.goc";
    const char* tex_path = "/tile/mario_tileset.texturec";
    dmhash_t tex_hash = dmHashString64(tex_path);
    dmhash_t comp_name = dmHashString64("model");

    void* tex_res = 0x0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, tex_path, &tex_res));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    char prop_buf[32];
    dmGameObject::PropertyOptions opt;
    for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
    {
        dmSnPrintf(prop_buf, sizeof(prop_buf), "texture%u", i);
        dmhash_t prop = dmHashString64(prop_buf);
        dmGameObject::PropertyVar v(tex_hash);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, comp_name, prop, opt, v));

        dmGameObject::PropertyDesc desc;
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, comp_name, prop, opt, desc));
        ASSERT_EQ(dmGameObject::PROPERTY_TYPE_HASH, desc.m_Variant.m_Type);
        ASSERT_EQ(tex_hash, desc.m_Variant.m_Hash);
    }

    DeleteInstance(m_Collection, go);
    dmResource::Release(m_Factory, tex_res);
}

// Test that go.delete() does not influence other sprite animations in progress
TEST_F(SpriteTest, GoDeletion)
{
    // Spawn 3 dumy game objects with one sprite in each
    dmGameObject::HInstance go1 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go1"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go2 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go2"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go3 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go3"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go1);
    ASSERT_NE((void*)0, go2);
    ASSERT_NE((void*)0, go3);

    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go_animater = Spawn(m_Factory, m_Collection, "/sprite/sprite_property_anim.goc", dmHashString64("/go_animater"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_animater);

    // 1st iteration:
    //  - go1 animation start
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // 2nd iteration:
    //  - go1 animation is over and removed
    //  - go2+go3 animations start
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // 3rd iteration:
    //  - go2 animation is over and removed
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // 4th iteration:
    //  - go3 should still be animating (not be influenced by the deletion of go1/go2)
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Test that animation done event reaches either callback or onmessage
TEST_F(SpriteTest, FlipbookAnim)
{
    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/sprite_flipbook_anim.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    lua_State* L = m_Scriptlibcontext.m_LuaState;

    WaitForTestsDone(10000, true, 0);

    lua_getglobal(L, "num_finished");
    int num_finished = lua_tointeger(L, -1);
    lua_pop(L, 1);

    lua_getglobal(L, "num_messages");
    int num_messages = lua_tointeger(L, -1);
    lua_pop(L, 1);

    ASSERT_EQ(2, num_finished);
    ASSERT_EQ(1, num_messages);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, FrameCount)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/frame_count/sprite_frame_count.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    WaitForTestsDone(100, false, 0);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(SpriteTest, GetSetSliceProperty)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/sprite_slice9.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, GetSetImagesByHash)
{
    void* atlas=0;
    dmResource::Result res = dmResource::Get(m_Factory, "/atlas/valid_64x64.t.texturesetc", &atlas);
    ASSERT_EQ(dmResource::RESULT_OK, res);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/image/get_set_image_by_hash.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmResource::Release(m_Factory, atlas);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, SetImageThenPlayAnimationDoesNotLogErrors)
{
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t sprite_comp_id = dmHashString64("sprite");
    dmhash_t image_prop_id = dmHashString64("image");
    dmhash_t animation_prop_id = dmHashString64("animation");
    dmhash_t atlas_b = dmHashString64("/atlas/valid_64x64.t.texturesetc");
    dmhash_t animation_b = dmHashString64("valid_png");
    dmGameSystem::TextureSetResource* atlas_resource = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/atlas/valid_64x64.t.texturesetc", (void**) &atlas_resource));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/image/get_set_image_by_hash_noscript.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::PropertyOptions tex1_options;
    ASSERT_TRUE(dmGameObject::AddPropertyOptionsKey(&tex1_options, dmHashString64("tex1")));

    {
        GamesysErrorLogCapture log_capture;

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetHashProperty(go, sprite_comp_id, image_prop_id, atlas_b));
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetHashProperty(go, sprite_comp_id, image_prop_id, atlas_b, &tex1_options));
        ASSERT_EQ(animation_b, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        PostSpritePlayAnimation(m_Collection, go_id, sprite_comp_id, animation_b, 0.0f, 1.0f);

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        ASSERT_EQ(atlas_b, GetHashProperty(go, sprite_comp_id, image_prop_id));
        ASSERT_EQ(atlas_b, GetHashProperty(go, sprite_comp_id, image_prop_id, &tex1_options));
        ASSERT_EQ(animation_b, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        ASSERT_TRUE(log_capture.Empty());
    }

    dmResource::Release(m_Factory, atlas_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, SetImageKeepsSharedAnimationWithoutLogging)
{
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t sprite_comp_id = dmHashString64("sprite");
    dmhash_t image_prop_id = dmHashString64("image");
    dmhash_t animation_prop_id = dmHashString64("animation");
    dmhash_t shared_animation_id = dmHashString64("anim");
    dmhash_t new_image_id = dmHashString64("/tile/valid.t.texturesetc");
    dmGameSystem::TextureSetResource* image_resource = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/tile/valid.t.texturesetc", (void**) &image_resource));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/cursor.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    ASSERT_EQ(shared_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

    {
        GamesysErrorLogCapture log_capture;

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetHashProperty(go, sprite_comp_id, image_prop_id, new_image_id));
        ASSERT_EQ(shared_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        RenderCollection(m_RenderContext, m_Collection);

        ASSERT_EQ(shared_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));
        ASSERT_TRUE(log_capture.Empty());
    }

    dmResource::Release(m_Factory, image_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, SetImageFallsBackToFirstAnimationWithoutLogging)
{
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t sprite_comp_id = dmHashString64("sprite");
    dmhash_t image_prop_id = dmHashString64("image");
    dmhash_t animation_prop_id = dmHashString64("animation");
    dmhash_t old_animation_id = dmHashString64("anim_loop_pingpong");
    dmhash_t new_animation_id = dmHashString64("anim");
    dmhash_t new_image_id = dmHashString64("/tile/valid.t.texturesetc");
    dmGameSystem::TextureSetResource* image_resource = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/tile/valid.t.texturesetc", (void**) &image_resource));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/cursor.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    {
        GamesysErrorLogCapture log_capture;

        PostSpritePlayAnimation(m_Collection, go_id, sprite_comp_id, old_animation_id, 0.0f, 1.0f);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        ASSERT_EQ(old_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetHashProperty(go, sprite_comp_id, image_prop_id, new_image_id));
        ASSERT_EQ(new_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        RenderCollection(m_RenderContext, m_Collection);
        ASSERT_EQ(new_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        PostSpritePlayAnimation(m_Collection, go_id, sprite_comp_id, new_animation_id, 0.0f, 1.0f);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        ASSERT_EQ(new_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        ASSERT_TRUE(log_capture.Empty());
    }

    dmResource::Release(m_Factory, image_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, SetImageFallsBackToFirstTrimmedAnimationWithoutLogging)
{
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t sprite_comp_id = dmHashString64("sprite");
    dmhash_t image_prop_id = dmHashString64("image");
    dmhash_t animation_prop_id = dmHashString64("animation");
    dmhash_t old_animation_id = dmHashString64("anim_loop_pingpong");
    dmhash_t fallback_animation_id = dmHashString64("frame_0");
    dmhash_t new_image_id = dmHashString64("/sprite/image/stale_animation_id_in_range.t.texturesetc");
    dmGameSystem::TextureSetResource* image_resource = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/sprite/image/stale_animation_id_in_range.t.texturesetc", (void**) &image_resource));
    ASSERT_TRUE(image_resource->m_TextureSet->m_Animations.m_Count > 6);
    ASSERT_NE(dmGameSystemDDF::SPRITE_TRIM_MODE_OFF, image_resource->m_TextureSet->m_Geometries[0].m_TrimMode);
    ASSERT_EQ((uint32_t*)0, image_resource->m_AnimationIds.Get(old_animation_id));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/cursor.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    void* sprite_component = GetSpriteComponent(go, sprite_comp_id);
    ASSERT_NE((void*)0, sprite_component);

    PostSpritePlayAnimation(m_Collection, go_id, sprite_comp_id, old_animation_id, 0.0f, 1.0f);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(old_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));
    ASSERT_GT(dmGameSystem::GetSpriteComponentAnimationIndex(sprite_component), 0u);

    {
        GamesysErrorLogCapture log_capture;

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetHashProperty(go, sprite_comp_id, image_prop_id, new_image_id));
        ASSERT_EQ(fallback_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));
        ASSERT_EQ(0u, dmGameSystem::GetSpriteComponentAnimationIndex(sprite_component));

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        RenderCollection(m_RenderContext, m_Collection);

        ASSERT_EQ(fallback_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));
        ASSERT_EQ(0u, dmGameSystem::GetSpriteComponentAnimationIndex(sprite_component));
        ASSERT_TRUE(log_capture.Empty());
    }

    dmResource::Release(m_Factory, image_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, ScaleAffectsWorldSize)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    // Let the sprite initialize
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    uint32_t component_type;
    dmGameObject::HComponent component;
    dmGameObject::HComponentWorld world;

    dmGameObject::Result res = dmGameObject::GetComponent(go, dmHashString64("sprite"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    Vector3 world_size_before;
    dmGameSystem::GetSpriteComponentScale(component, &world_size_before);
    float sx_before = world_size_before.getX();
    float sy_before = world_size_before.getY();

    // Set a non-uniform scale
    dmGameObject::PropertyOptions opts;
    dmGameObject::PropertyVar scale_var(dmVMath::Vector3(2.0f, 3.0f, 1.0f));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, dmHashString64("sprite"), dmHashString64("scale"), opts, scale_var));

    // Run an update so the transform and world_size are refreshed
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    Vector3 world_size_after;
    dmGameSystem::GetSpriteComponentScale(component, &world_size_after);
    float sx_after = world_size_after.getX();
    float sy_after = world_size_after.getY();

    // world_size should scale with the sprite's scale
    ASSERT_NEAR(sx_before * 2.0f, sx_after, 0.001f);
    ASSERT_NEAR(sy_before * 3.0f, sy_after, 0.001f);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Test that animation done event reaches callback
TEST_F(ParticleFxTest, PlayAnim)
{
    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/particlefx/particlefx_play.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    bool tests_done = false;
    WaitForTestsDone(100, true, &tests_done);

    if (!tests_done)
    {
        dmLogError("The playback didn't finish");
    }
    ASSERT_TRUE(tests_done);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ParticleFxTest, GetSetProperties)
{
    dmGameSystem::MaterialResource *material;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/particlefx/particlefx_get_set_properties.materialc", (void**) &material));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/particlefx/particlefx_get_set_properties.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmResource::Release(m_Factory, material);
}

TEST_F(ParticleFxTest, FrustumCullsParticleEmitters)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go_inside = Spawn(m_Factory, m_Collection, "/particlefx/valid_particlefx.goc", dmHashString64("/go_inside"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go_outside = Spawn(m_Factory, m_Collection, "/particlefx/valid_particlefx.goc", dmHashString64("/go_outside"), 0, Point3(1000, 1000, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_inside);
    ASSERT_NE((void*)0, go_outside);

    dmMessage::URL receiver_inside;
    receiver_inside.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver_inside.m_Path = dmGameObject::GetIdentifier(go_inside);
    receiver_inside.m_Fragment = dmHashString64("particlefx");
    dmMessage::Post(0, &receiver_inside, dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)go_inside, (uintptr_t)dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor, 0, 0, 0);

    dmMessage::URL receiver_outside;
    receiver_outside.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    receiver_outside.m_Path = dmGameObject::GetIdentifier(go_outside);
    receiver_outside.m_Fragment = dmHashString64("particlefx");
    dmMessage::Post(0, &receiver_outside, dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)go_outside, (uintptr_t)dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor, 0, 0, 0);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);

    dmRender::RenderContext* render_context_ptr = (dmRender::RenderContext*)m_RenderContext;
    void* particlefx_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("particlefxc")));
    ASSERT_NE((void*)0, particlefx_world);

    uint32_t particle_dispatch = UINT32_MAX;
    for (uint32_t i = 0; i < render_context_ptr->m_RenderListDispatch.Size(); ++i)
    {
        if (render_context_ptr->m_RenderListDispatch[i].m_UserData == particlefx_world)
        {
            particle_dispatch = i;
            break;
        }
    }
    ASSERT_NE(UINT32_MAX, particle_dispatch);

    dmRender::FrustumOptions frustum_options;
    frustum_options.m_Matrix = Matrix4::orthographic(-100.0f, 100.0f, -100.0f, 100.0f, -1.0f, 1.0f);
    frustum_options.m_NumPlanes = dmRender::FRUSTUM_PLANES_SIDES;
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, &frustum_options, dmRender::SORT_BACK_TO_FRONT);

    uint32_t inside_count = 0;
    uint32_t outside_count = 0;
    for (uint32_t i = 0; i < render_context_ptr->m_RenderList.Size(); ++i)
    {
        dmRender::RenderListEntry& entry = render_context_ptr->m_RenderList[i];
        if (entry.m_Dispatch != particle_dispatch)
            continue;

        dmParticle::EmitterRenderData* render_data = (dmParticle::EmitterRenderData*)entry.m_UserData;
        const bool inside_frustum = dmMath::Abs(render_data->m_FrustumCullingCenter.getX()) < 500.0f;
        if (inside_frustum)
        {
            ASSERT_EQ(dmRender::VISIBILITY_FULL, entry.m_Visibility);
            inside_count++;
        }
        else
        {
            ASSERT_EQ(dmRender::VISIBILITY_NONE, entry.m_Visibility);
            outside_count++;
        }
    }

    ASSERT_GT(inside_count, 0u);
    ASSERT_GT(outside_count, 0u);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

static float GetFloatProperty(dmGameObject::HInstance go, dmhash_t component_id, dmhash_t property_id)
{
    dmGameObject::PropertyDesc property_desc;
    dmGameObject::PropertyOptions property_opt;
    dmGameObject::GetProperty(go, component_id, property_id, property_opt, property_desc);
    return property_desc.m_Variant.m_Number;
}

TEST_F(CursorTest, GuiFlipbookCursor)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmhash_t go_id = dmHashString64("/go");
    dmhash_t gui_comp_id = dmHashString64("gui");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_flipbook_cursor.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    dmMessage::URL msg_url;
    dmMessage::ResetURL(&msg_url);
    msg_url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    msg_url.m_Path = go_id;
    msg_url.m_Fragment = gui_comp_id;

    // Update one second at a time.
    // The tilesource animation is one frame per second,
    // will make it easier to predict the cursor.
    m_UpdateContext.m_DT = 1.0f;

    bool continue_test = true;
    while (continue_test) {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if there was an error
        lua_getglobal(L, "test_err");
        bool test_err = lua_toboolean(L, -1);
        lua_pop(L, 1);
        lua_getglobal(L, "test_err_str");
        const char* test_err_str = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (test_err) {
            dmLogError("Lua Error: %s", test_err_str);
        }

        ASSERT_FALSE(test_err);

        // continue test?
        lua_getglobal(L, "continue_test");
        continue_test = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_P(CursorTest, Cursor)
{
    const CursorTestParams& params = GetParam();
    const char* anim_id_str = params.m_AnimationId;
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t cursor_prop_id = dmHashString64("cursor");
    dmhash_t sprite_comp_id = dmHashString64("sprite");
    dmhash_t animation_id = dmHashString64(anim_id_str);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/cursor.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    // Dummy URL, just needed to kick flipbook animation on sprite
    dmMessage::URL msg_url;
    dmMessage::ResetURL(&msg_url);
    msg_url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    msg_url.m_Path = go_id;
    msg_url.m_Fragment = sprite_comp_id;

    // Send animation to sprite component
    dmGameSystemDDF::PlayAnimation msg;
    msg.m_Id = animation_id;
    msg.m_Offset = params.m_CursorStart;
    msg.m_PlaybackRate = params.m_PlaybackRate;

    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::PostDDF(&msg, &msg_url, &msg_url, (uintptr_t)go, 0, 0));

    m_UpdateContext.m_DT = 0.0f;
    dmGameObject::Update(m_Collection, &m_UpdateContext);

    // Update one second at a time.
    // The tilesource animation is one frame per second,
    // will make it easier to predict the cursor.
    m_UpdateContext.m_DT = 1.0f;

    for (int i = 0; i < params.m_ExpectedCount; ++i)
    {
        ASSERT_EQ(params.m_Expected[i], GetFloatProperty(go, sprite_comp_id, cursor_prop_id));
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GetSetMaterialConstants)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/get_set_material_constants.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);
}

// Tests the animation done message/callback
TEST_F(GuiTest, GuiFlipbookAnim)
{
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t gui_comp_id = dmHashString64("gui");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_flipbook_anim.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    dmMessage::URL msg_url;
    dmMessage::ResetURL(&msg_url);
    msg_url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    msg_url.m_Path = go_id;
    msg_url.m_Fragment = gui_comp_id;

    m_UpdateContext.m_DT = 1.0f;

    bool tests_done = false;
    WaitForTestsDone(100, true, &tests_done);

    if (!tests_done)
    {
        dmLogError("The playback didn't finish");
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Tests the different types of textures (atlas, texture, dynamic)
// This test makes sure that we can use the correct resource pointers.
TEST_F(GuiTest, TextureResources)
{
    dmhash_t go_id = dmHashString64("/go");

    dmGameSystem::TextureSetResource* valid_atlas = 0;
    dmGameSystem::TextureResource* valid_texture = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/atlas/valid.t.texturesetc", (void**) &valid_atlas));
    ASSERT_TRUE(valid_atlas != 0x0);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/texture/valid_png.texturec", (void**) &valid_texture));
    ASSERT_TRUE(valid_atlas != 0x0);

    dmGraphics::HTexture valid_atlas_th = valid_atlas->m_Texture->m_Texture;
    dmGraphics::HTexture valid_texture_th = valid_texture->m_Texture;

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/texture_resources/texture_resources.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    dmResource::Release(m_Factory, valid_atlas);
    dmResource::Release(m_Factory, valid_texture);

    // Update + render the GO - this is needed to trigger the creation of the dynamic texture
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t component_type_index        = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world    = (dmGameSystem::GuiWorld*) dmGameObject::GetWorld(m_Collection, component_type_index);
    dmGameSystem::GuiComponent* gui_comp = gui_world->m_Components[0];

    {
        // Box1 is using the "texture" entry, which should equate to a texture resource
        dmGui::HNode box1 = dmGui::GetNodeById(gui_comp->m_Scene, "box1");
        ASSERT_NE(0, box1);

        dmGui::NodeTextureType texture_type;
        dmGui::HTextureSource texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box1, &texture_type);
        ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE, texture_type);

        dmGameSystem::TextureResource* texture_res = (dmGameSystem::TextureResource*) texture_source;
        ASSERT_EQ(texture_res, valid_texture);
        ASSERT_EQ(valid_texture_th, texture_res->m_Texture); // same texture

        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, texture_res->m_Texture));
    }

    {
        // Box2 is using the "texture set" entry, which should equate to a texture set resource
        dmGui::HNode box2 = dmGui::GetNodeById(gui_comp->m_Scene, "box2");
        ASSERT_NE(0, box2);

        dmGui::NodeTextureType texture_type;
        dmGui::HTextureSource texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box2, &texture_type);
        ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, texture_type);

        dmGameSystem::TextureSetResource* texture_set_res = (dmGameSystem::TextureSetResource*) texture_source;
        ASSERT_EQ(valid_atlas, texture_set_res);
        ASSERT_NE(valid_texture_th, texture_set_res->m_Texture->m_Texture); // NOT the same texture, we swap it out in the script

        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, texture_set_res->m_Texture->m_Texture));
        ASSERT_FALSE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, valid_atlas_th)); // Old texture has been removed
    }

    {
        // Box2 is using the "texture set" entry, which should equate to a texture set resource
        dmGui::HNode box3 = dmGui::GetNodeById(gui_comp->m_Scene, "box3");
        ASSERT_NE(0, box3);

        dmGui::NodeTextureType texture_type;
        dmGui::HTextureSource texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box3, &texture_type);
        ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE, texture_type);

        dmGameSystem::TextureResource* texture_res = (dmGameSystem::TextureResource*) texture_source;
        ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, texture_res->m_Texture));
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, TextureSetterOverrideRefreshesAtlasState)
{
    dmGameSystem::TextureSetResource* expected_atlas = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/gui/texture_setter_override_b.t.texturesetc", (void**)&expected_atlas));
    ASSERT_NE((void*)0x0, expected_atlas);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/texture_setter_override.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t component_type_index        = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world    = (dmGameSystem::GuiWorld*) dmGameObject::GetWorld(m_Collection, component_type_index);
    dmGameSystem::GuiComponent* gui_comp = gui_world->m_Components[0];

    dmGui::HNode box = dmGui::GetNodeById(gui_comp->m_Scene, "box");
    ASSERT_NE(0, box);

    dmGui::NodeTextureType texture_type;
    dmGui::HTextureSource texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box, &texture_type);
    ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, texture_type);
    ASSERT_EQ((dmGui::HTextureSource) expected_atlas, texture_source);

    dmGui::TextureSetAnimDesc* anim_desc = dmGui::GetNodeTextureSet(gui_comp->m_Scene, box);
    ASSERT_NE((void*)0x0, anim_desc);
    ASSERT_EQ((const void*) expected_atlas, anim_desc->m_TextureSet);
    ASSERT_EQ(64, anim_desc->m_State.m_OriginalTextureWidth);
    ASSERT_EQ(64, anim_desc->m_State.m_OriginalTextureHeight);

    Point3 size = dmGui::GetNodeSize(gui_comp->m_Scene, box);
    ASSERT_EQ(64.0f, size.getX());
    ASSERT_EQ(64.0f, size.getY());

    dmResource::Release(m_Factory, expected_atlas);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, TextureReloadRefreshesAtlasState)
{
    dmGameSystem::TextureSetResource* expected_atlas = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/gui/texture_setter_override_a.t.texturesetc", (void**)&expected_atlas));
    ASSERT_NE((void*)0x0, expected_atlas);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/texture_reload_override.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    // The atlas swap happens from script update(), so step one more frame to
    // exercise the GUI scene refresh in UpdateScene().
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    uint32_t component_type_index        = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world    = (dmGameSystem::GuiWorld*) dmGameObject::GetWorld(m_Collection, component_type_index);
    dmGameSystem::GuiComponent* gui_comp = gui_world->m_Components[0];

    dmGui::HNode box = dmGui::GetNodeById(gui_comp->m_Scene, "box");
    ASSERT_NE(0, box);

    dmGui::NodeTextureType texture_type;
    dmGui::HTextureSource texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box, &texture_type);
    ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, texture_type);
    ASSERT_EQ((dmGui::HTextureSource) expected_atlas, texture_source);

    dmGui::TextureSetAnimDesc* anim_desc = dmGui::GetNodeTextureSet(gui_comp->m_Scene, box);
    ASSERT_NE((void*)0x0, anim_desc);
    ASSERT_EQ((const void*) expected_atlas, anim_desc->m_TextureSet);
    ASSERT_EQ(64, anim_desc->m_State.m_OriginalTextureWidth);
    ASSERT_EQ(64, anim_desc->m_State.m_OriginalTextureHeight);

    Point3 size = dmGui::GetNodeSize(gui_comp->m_Scene, box);
    ASSERT_EQ(64.0f, size.getX());
    ASSERT_EQ(64.0f, size.getY());

    dmResource::Release(m_Factory, expected_atlas);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Tests creating and deleting dynamic textures
TEST_F(GuiTest, MaxDynamictextures)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_max_dynamic_textures.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    uint32_t component_type_index        = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world    = (dmGameSystem::GuiWorld*) dmGameObject::GetWorld(m_Collection, component_type_index);
    dmGameSystem::GuiComponent* gui_comp = gui_world->m_Components[0];

    dmGui::Scene* scene = gui_comp->m_Scene;

    ASSERT_EQ(32, scene->m_DynamicTextures.Capacity());
    ASSERT_EQ(0, scene->m_DynamicTextures.Size());

    // Test 1: create textures
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(32, scene->m_DynamicTextures.Size());

    // Test 2: delete textures
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Trigger a render to finalize deletion of the textures
    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    ASSERT_EQ(0, scene->m_DynamicTextures.Size());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}


// Test setting gui font
TEST_F(ResourceTest, ScriptSetFonts)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/goscript.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    void* font1 = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/valid_font.fontc", (void**) &font1));
    ASSERT_TRUE(font1 != 0x0);

    void* font2 = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/glyph_bank_test_1.fontc", (void**) &font2));
    ASSERT_TRUE(font2 != 0x0);

    for (int i = 0; i < 3; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        // Trigger a render to finalize deletion of the textures
        dmRender::RenderListBegin(m_RenderContext);
        dmGameObject::Render(m_Collection);

        dmRender::RenderListEnd(m_RenderContext);
        dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    }

    dmResource::Release(m_Factory, font1);
    dmResource::Release(m_Factory, font2);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(FontTest, GlyphBankTest)
{
    const char path_font_1[] = "/font/glyph_bank_test_1.fontc";
    const char path_font_2[] = "/font/glyph_bank_test_2.fontc";

    dmGameSystem::FontResource* font_1;
    dmGameSystem::FontResource* font_2;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font_1, (void**) &font_1));
    ASSERT_NE((void*)0, font_1);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font_2, (void**) &font_2));
    ASSERT_NE((void*)0, font_2);

    dmRender::HFontMap font_map_1 = dmGameSystem::ResFontGetHandle(font_1);
    ASSERT_NE((void*)0, font_map_1);
    dmRender::HFontMap font_map_2 = dmGameSystem::ResFontGetHandle(font_2);
    ASSERT_NE((void*)0, font_map_2);

    HFontCollection font_collection1 = dmRender::GetFontCollection(font_map_1);
    HFontCollection font_collection2 = dmRender::GetFontCollection(font_map_2);
    HFont hfont_1 = FontCollectionGetFont(font_collection1, 0);
    HFont hfont_2 = FontCollectionGetFont(font_collection2, 0);

    FontResult r;
    FontGlyph* glyph_1 = 0;
    r = GetGlyph(font_map_1, hfont_1, 'A', &glyph_1);
    ASSERT_EQ(FONT_RESULT_OK, r);
    ASSERT_NE((FontGlyph*)0, glyph_1);

    FontGlyph* glyph_2 = 0;
    r = GetGlyph(font_map_2, hfont_2, 'A', &glyph_2);
    ASSERT_EQ(FONT_RESULT_OK, r);
    ASSERT_NE((FontGlyph*)0, glyph_2);

    ASSERT_NE(glyph_1->m_Bitmap.m_Data, glyph_2->m_Bitmap.m_Data);

    dmResource::Release(m_Factory, font_1);
    dmResource::Release(m_Factory, font_2);
}

TEST_F(FontTest, DynamicGlyph)
{
    const char path_font[] = "/font/dyn_glyph_bank_test_1.fontc";
    dmGameSystem::FontResource* font;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font, (void**) &font));
    ASSERT_NE((void*)0, font);

    dmRender::HFontMap font_map = dmGameSystem::ResFontGetHandle(font);
    HFontCollection font_collection = dmRender::GetFontCollection(font_map);
    HFont hfont = FontCollectionGetFont(font_collection, 0);

    uint32_t codepoint = 'A';

    // Triggers a cache miss
    {
        FontGlyph* glyph = 0;
        FontResult r = GetGlyph(font_map, hfont, codepoint, &glyph);
        ASSERT_EQ(FONT_RESULT_OK, r); // glyph fallback returns OK, but cannot create a sdf bitmap from a ttf font via this api
        ASSERT_NE((FontGlyph*)0, glyph);
        ASSERT_EQ(codepoint, glyph->m_Codepoint);
        ASSERT_EQ(36U, glyph->m_GlyphIndex);
    }

    // Add a new glyph
    const char* data = "Test Image Data";
    {
        FontGlyph* glyph = new FontGlyph;
        memset(glyph, 0, sizeof(*glyph));

        glyph->m_Codepoint = codepoint;
        glyph->m_GlyphIndex = FontGetGlyphIndex(hfont, codepoint);
        glyph->m_Width = 1;
        glyph->m_Height = 2;
        glyph->m_Advance = 3;
        glyph->m_LeftBearing = 4;
        glyph->m_Ascent = 5;
        glyph->m_Descent = 6;
        glyph->m_Bitmap.m_Width = 10;
        glyph->m_Bitmap.m_Height = 11;
        glyph->m_Bitmap.m_Channels = 12;
        glyph->m_Bitmap.m_Flags = 0;
        glyph->m_Bitmap.m_Data = (uint8_t*)strdup(data);;

        dmResource::Result r = dmGameSystem::ResFontAddGlyph(font, 0, glyph);
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    {
        FontGlyph* glyph = 0;
        FontResult r = GetGlyph(font_map, hfont, codepoint, &glyph);
        ASSERT_EQ(FONT_RESULT_OK, r);
        ASSERT_NE((FontGlyph*)0, glyph);
        ASSERT_EQ(codepoint, glyph->m_Codepoint);
        ASSERT_EQ(36U, glyph->m_GlyphIndex);

        ASSERT_EQ(1U, glyph->m_Width);
        ASSERT_EQ(2U, glyph->m_Height);
        ASSERT_EQ(3U, glyph->m_Advance);
        ASSERT_EQ(4U, glyph->m_LeftBearing);
        ASSERT_EQ(5U, glyph->m_Ascent);
        ASSERT_EQ(6U, glyph->m_Descent);

        ASSERT_EQ(10U, glyph->m_Bitmap.m_Width);
        ASSERT_EQ(11U, glyph->m_Bitmap.m_Height);
        ASSERT_EQ(12U, glyph->m_Bitmap.m_Channels);
        ASSERT_EQ(0U, (uint32_t)glyph->m_Bitmap.m_Flags);
        ASSERT_STREQ(data, (const char*)glyph->m_Bitmap.m_Data);
    }

    dmResource::Release(m_Factory, font);
}

TEST_F(FontTest, ReloadCancelsPendingDynamicFontJobs)
{
    const char path_font[] = "/font/dyn_glyph_bank_test_1.fontc";
    dmGameSystem::FontResource* font = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font, (void**) &font));
    ASSERT_NE((void*)0, font);
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontPrewarmText(font, "Reload pending jobs", 0, 0));
    ASSERT_GT(font->m_PendingJobs.Size(), 0u);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path_font, 0));
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    dmResource::Release(m_Factory, font);
}

// Verifies the Lua API for dynamic font collections updates resource refs and collection membership.
TEST_F(FontTest, ScriptAddRemoveFont)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    scriptlibcontext.m_JobContext      = m_JobContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // Create a temporary .ttf resource by copying an existing test font into the custom file mount.
    // We avoid the default font path used by the dynamic font to exercise add/remove behavior.
    const char* ttf_source_path = "/font/valid.ttf";
    const char* ttf_test_path = "/font/valid_copy.ttf";
    void* ttf_data = 0;
    uint32_t ttf_size = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetRaw(m_Factory, ttf_source_path, &ttf_data, &ttf_size));
    ASSERT_NE((void*)0, ttf_data);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, ttf_test_path, ttf_size, ttf_data));

    dmhash_t ttf_hash = dmHashString64(ttf_test_path);
    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, ttf_hash));

    // Load the temp font so the hash-based Lua API can find it via the resource system.
    dmGameSystem::TTFResource* ttf_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetWithExt(m_Factory, ttf_test_path, "ttf", (void**) &ttf_resource));
    ASSERT_NE((void*)0, ttf_resource);
    ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, ttf_hash));

    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", (void**) &font));
    ASSERT_NE((void*)0, font);

    dmRender::HFontMap font_map = dmGameSystem::ResFontGetHandle(font);
    HFontCollection font_collection = dmRender::GetFontCollection(font_map);
    uint32_t font_count_before = FontCollectionGetFontCount(font_collection);
    uint32_t font_version = dmGameSystem::ResFontGetVersion(font);

    // Add the font via Lua and verify refcount + collection size.
    lua_State* L = scriptlibcontext.m_LuaState;
    ASSERT_TRUE(RunString(L, "font.add_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), hash(\"/font/valid_copy.ttf\"))"));
    ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, ttf_hash));
    ASSERT_EQ(font_count_before + 1, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(font_version + 1, dmGameSystem::ResFontGetVersion(font));
    font_version = dmGameSystem::ResFontGetVersion(font);

    dmLogInfo("Expected errors ->");
    // Adding the same font twice should fail and keep counts intact.
    ASSERT_FALSE(RunString(L, "font.add_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), hash(\"/font/valid_copy.ttf\"))"));
    ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, ttf_hash));
    ASSERT_EQ(font_count_before + 1, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(font_version, dmGameSystem::ResFontGetVersion(font));

    // Remove the font via Lua and verify refcount + collection size restored.
    ASSERT_TRUE(RunString(L, "font.remove_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), hash(\"/font/valid_copy.ttf\"))"));
    ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, ttf_hash));
    ASSERT_EQ(font_count_before, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(font_version + 1, dmGameSystem::ResFontGetVersion(font));
    font_version = dmGameSystem::ResFontGetVersion(font);

    // The default font referenced by the .fontc should not be re-added (string path).
    ASSERT_FALSE(RunString(L, "font.add_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), \"/font/valid.ttf\")"));
    ASSERT_EQ(font_count_before, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(font_version, dmGameSystem::ResFontGetVersion(font));

    // The default font referenced by the .fontc should not be re-added (hashed path).
    ASSERT_FALSE(RunString(L, "font.add_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), hash(\"/font/valid.ttf\"))"));
    ASSERT_EQ(font_count_before, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(font_version, dmGameSystem::ResFontGetVersion(font));

    // The default font referenced by the .fontc should not be removable.
    ASSERT_FALSE(RunString(L, "font.remove_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), hash(\"/font/valid.ttf\"))"));
    ASSERT_EQ(font_count_before, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(font_version, dmGameSystem::ResFontGetVersion(font));

    dmLogInfo("<- End of expected errors.");

    dmResource::Release(m_Factory, font);
    dmResource::Release(m_Factory, ttf_resource);
    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, ttf_hash));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, ttf_test_path));
    free(ttf_data);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(WindowTest, MouseLock)
{
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_GraphicsApi            = WINDOW_GRAPHICS_API_NULL;

    dmHID::NewContextParams hid_params = {};
    dmHID::HContext hid_context = dmHID::NewContext(hid_params);
    dmHID::Init(hid_context);

    HWindow window = dmPlatform::NewWindow();
    dmPlatform::OpenWindow(window, window_params);
    dmHID::SetWindow(hid_context, window);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_HidContext      = hid_context;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/window/mouse_lock.goc", dmHashString64("/mouse_lock"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // cleanup
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);

    dmHID::Final(hid_context);
    dmHID::DeleteContext(hid_context);
    dmPlatform::DeleteWindow(window);
}

TEST_F(WindowTest, Events)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/window/window_events.goc", dmHashString64("/window_events"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::AcquireInputFocus(m_Collection, go);
    dmGameObject::InputAction input_action;
    input_action.m_ActionId = dmHashString64("test_action");

    // Set test state 1
    input_action.m_Value = 1.0f;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    dmGameSystem::OnWindowFocus(false);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Set test state 2
    input_action.m_Value = 2.0f;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    dmGameSystem::OnWindowFocus(true);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Set test state 3
    input_action.m_Value = 3.0f;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    dmGameSystem::OnWindowResized(123, 456);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Set test state 4
    input_action.m_Value = 4.0f;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    dmGameSystem::OnWindowFocus(false);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Set final test state, check that all tests passed
    input_action.m_Value = 0.0f;
    dmGameObject::DispatchInput(m_Collection, &input_action, 1);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // cleanup
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

/* Factory dynamic and static loading */

TEST_P(FactoryTest, Test)
{
    const char* resource_path[] = {
            "/factory/factory_resource.goc",
            "/sprite/valid.spritec",
            "/tile/valid.t.texturesetc",
            "/sprite/sprite.materialc",
    };
    const char* dyn_prototype_empty_resource_path[] = {
            "/factory/empty.goc",
    };
    const char* dyn_prototype_sprite_resource_path[] = {
            "/factory/dynamic_prototype_sprite.goc",
            "/sprite/valid.spritec",
            "/tile/valid.t.texturesetc",
            "/sprite/sprite.materialc",
    };
    const char** dyn_prototype_resource_path = 0;
    uint32_t num_dyn_prototype_resources = 0;
    bool custom_prototype_has_subresources = false;
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const FactoryTestParams& param = GetParam();

    if (param.m_PrototypePath)
    {
        char buffer[256];
        dmSnPrintf(buffer, sizeof(buffer), "prototype_path = '%s'", param.m_PrototypePath);
        RunString(L, buffer);

        if (strcmp(param.m_PrototypePath, "/factory/empty.goc") == 0)
        {
            dyn_prototype_resource_path = dyn_prototype_empty_resource_path;
            num_dyn_prototype_resources = DM_ARRAY_SIZE(dyn_prototype_empty_resource_path);
        }
        else if (strcmp(param.m_PrototypePath, "/factory/dynamic_prototype_sprite.goc") == 0)
        {
            dyn_prototype_resource_path = dyn_prototype_sprite_resource_path;
            num_dyn_prototype_resources = DM_ARRAY_SIZE(dyn_prototype_sprite_resource_path);
            custom_prototype_has_subresources = true;
        }
        else
        {
            ASSERT_TRUE(false);
        }
    }

    // Conditional preload. This is essentially testing async loading vs sync loading of parent collection
    // This only affects non-dynamic factories.
    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
        while (dmTime::GetMonotonicTime() < stop_time)
        {
            r = dmResource::UpdatePreloader(go_pr, 0, 0, 16*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(16*1000);
        }
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    // Spawn the game object with the script we want to call
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);
    if(go_pr)
    {
        dmResource::DeletePreloader(go_pr);
    }

    if(param.m_IsDynamic)
    {
        // validate that resources from dynamic factory is not loaded at this point. They will start loading from the script when updated below
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        // --- step 1 ---
        // update until instances are created through test script (factory.load and create)
        // 1) load factory resource using factory.load
        // 2) create 2 instances (two factory.create calls)
        // Do this twice in order to ensure load/unload can be called multiple times, with and without deleting created objects
        for(uint32_t i = 0; i < 2; ++i)
        {
            for(;;)
            {
                lua_getglobal(L, "global_created");
                bool ready = !lua_isnil(L, -1);
                lua_pop(L, 1);
                if(ready)
                {
                    lua_getglobal(L, "first_instance");
                    dmhash_t first_instance = dmScript::CheckHash(L, -1);
                    lua_pop(L, 1);
                    lua_getglobal(L, "second_instance");
                    dmhash_t second_instance = dmScript::CheckHash(L, -1);
                    lua_pop(L, 1);
                    dmhash_t last_object_id = i == 0 ? second_instance : first_instance; // stacked index list in dynamic spawning
                    if (dmGameObject::GetInstanceFromIdentifier(m_Collection, last_object_id) != 0x0)
                        break;
                }
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }
            ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

            // --- step 2 ---
            // call factory.unload, derefencing factory reference.
            // first iteration will delete gameobjects created with factories, second will keep
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(i*2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        }

        // --- step 3 ---
        // call factory.unload again, which is ok by design (no operation)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        // --- step 4 ---
        // delete resources created by factory.create calls. All resource should be released
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        // --- step 5 ---
        // recreate resources without factory.load having been called (sync load on demand)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        if (param.m_PrototypePath)
        {
            // --- step 6 ---
            // unload the factory after the sync create so only the live instances keep the default prototype
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

            // --- step 7 ---
            // delete the instances created in the previous step so the new prototype starts from zero refs
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

            for (uint32_t i = 0; i < num_dyn_prototype_resources; ++i)
            {
                ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
            }

            // --- step 8 ---
            // set and load a custom prototype, then create two instances
            for(;;)
            {
                lua_getglobal(L, "global_created");
                bool ready = !lua_isnil(L, -1);
                lua_pop(L, 1);
                if(ready)
                {
                    lua_getglobal(L, "second_instance");
                    dmhash_t second_instance = dmScript::CheckHash(L, -1);
                    lua_pop(L, 1);
                    if (dmGameObject::GetInstanceFromIdentifier(m_Collection, second_instance) != 0x0)
                        break;
                }
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }

            ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            if (custom_prototype_has_subresources)
            {
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[2])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[3])));
            }

            // --- step 9 ---
            // unload and reset the prototype; only the created instances should keep the resource alive
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            if (custom_prototype_has_subresources)
            {
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[2])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[3])));
            }

            // --- step 10 ---
            // delete the created instances and release the custom prototype resource
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            for (uint32_t i = 0; i < num_dyn_prototype_resources; ++i)
            {
                ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
            }
        }

        // delete the root go and update so deferred deletes will be executed.
        dmGameObject::Delete(m_Collection, go, true);
        dmGameObject::Final(m_Collection);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
    }
    else
    {
        // validate that resources from factory is loaded with the parent collection.
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        // --- step 1 ---
        // call update which will create two instances (two collectionfactory.create)
        // We also call factory.load to ensure this does nothing except always invoke the loadcomplete callback (by design)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);

        // verify two instances created + one reference from factory prototype
        ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        // --- step 2 ---
        // call factory.unload which is a no-operation for non-dynamic factories
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

        if (param.m_PrototypePath)
        {
            // --- step 3 ---
            // delete the current instances before switching to a custom prototype
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            if (custom_prototype_has_subresources)
            {
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[2])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[3])));
            }
            else
            {
                for (uint32_t i = 1; i < num_dyn_prototype_resources; ++i)
                {
                    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
                }
            }

            // --- step 4 ---
            // set and load a custom prototype, then create two instances
            for (;;)
            {
                lua_getglobal(L, "global_created");
                bool ready = !lua_isnil(L, -1);
                lua_pop(L, 1);
                if (ready)
                {
                    lua_getglobal(L, "second_instance");
                    dmhash_t second_instance = dmScript::CheckHash(L, -1);
                    lua_pop(L, 1);
                    if (dmGameObject::GetInstanceFromIdentifier(m_Collection, second_instance) != 0x0)
                        break;
                }
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }
            ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            if (custom_prototype_has_subresources)
            {
                ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[2])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[3])));
            }

            // --- step 5 ---
            // unload and reset the prototype; only the created instances should keep the resource alive
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            if (custom_prototype_has_subresources)
            {
                ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[2])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[3])));
            }

            // --- step 6 ---
            // delete the created instances and release the custom prototype resource
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            if (custom_prototype_has_subresources)
            {
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[2])));
                ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[3])));
            }
            else
            {
                for (uint32_t i = 1; i < num_dyn_prototype_resources; ++i)
                {
                    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
                }
            }
        }

        // Delete the root go and update so deferred deletes will be executed.
        dmGameObject::Delete(m_Collection, go, true);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
    }

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(FactoryTest, IdHashTest)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // Spawn the game object with the script we want to call
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/factory/factory_hash_test.goc", go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(FactoryTest, Create)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    
    lua_pushnumber(L, m_projectOptions.m_MaxInstances);
    lua_setglobal(L, "max_instances");

    // Spawn the game object with the script we want to call
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/factory/factory_create_test.goc", go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(FactoryRecursivePrototypeTest, RecursivePrototype)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const FactoryTestParams& param = GetParam();

    char buffer[256];
    dmSnPrintf(buffer, sizeof(buffer), "recursive_prototype_path = '%s'", param.m_GOPath);
    RunString(L, buffer);

    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
        while (dmTime::GetMonotonicTime() < stop_time)
        {
            r = dmResource::UpdatePreloader(go_pr, 0, 0, 16*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(16*1000);
        }
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);

    if(go_pr)
    {
        dmResource::DeletePreloader(go_pr);
    }

    dmhash_t recursive_instance = 0;
    for(;;)
    {
        lua_getglobal(L, "global_recursive_created");
        bool ready = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if(ready)
        {
            lua_getglobal(L, "recursive_instance");
            recursive_instance = dmScript::CheckHash(L, -1);
            lua_pop(L, 1);
            if (dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance) != 0x0)
                break;
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
    }

    ASSERT_NE((dmhash_t)0, recursive_instance);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    ASSERT_EQ((dmGameObject::HInstance)0x0, dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance));

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(FactoryRecursivePrototypeTest, RecursivePrototypeCppCleanup)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const FactoryTestParams& param = GetParam();

    char buffer[256];
    dmSnPrintf(buffer, sizeof(buffer), "recursive_prototype_path = '%s'", param.m_GOPath);
    RunString(L, buffer);
    RunString(L, "recursive_cleanup_in_script = false");

    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
        while (dmTime::GetMonotonicTime() < stop_time)
        {
            r = dmResource::UpdatePreloader(go_pr, 0, 0, 16*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(16*1000);
        }
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);

    if(go_pr)
    {
        dmResource::DeletePreloader(go_pr);
    }

    dmhash_t recursive_instance = 0;
    for(;;)
    {
        lua_getglobal(L, "global_recursive_created");
        bool ready = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if(ready)
        {
            lua_getglobal(L, "recursive_instance");
            recursive_instance = dmScript::CheckHash(L, -1);
            lua_pop(L, 1);
            if (dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance) != 0x0)
                break;
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
    }

    ASSERT_NE((dmhash_t)0, recursive_instance);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    ASSERT_EQ((dmGameObject::HInstance)0x0, dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash));
    ASSERT_NE((dmGameObject::HInstance)0x0, dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

/* Collection factory dynamic and static loading */

TEST_P(CollectionFactoryTest, Test)
{
    const char* resource_path[] = {
            "/collection_factory/collectionfactory_test.collectionc", // prototype resource (loaded in collection factory resource)
            "/collection_factory/collectionfactory_resource.goc", // two instances referenced in factory collection protoype
            "/sprite/valid.spritec", // single instance (subresource of go's)
            "/tile/valid.t.texturesetc", // single instance (subresource of sprite)
            "/sprite/sprite.materialc", // single instance (subresource of sprite)
    };
    uint32_t num_resources = DM_ARRAY_SIZE(resource_path);

    const char* dyn_prototype_resource_path[] = {
            "/collection_factory/dynamic_prototype.goc",
            "/collection_factory/dynamic_prototype.scriptc", // referenced by the goc
    };
    uint32_t num_dyn_prototype_resources = DM_ARRAY_SIZE(dyn_prototype_resource_path);

    dmHashEnableReverseHash(true);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const CollectionFactoryTestParams& param = GetParam();

    if (param.m_PrototypePath)
    {
        lua_State* L = dmScript::GetLuaState(m_ScriptContext);
        char buffer[256];
        dmSnPrintf(buffer, sizeof(buffer), "prototype_path = '%s'", param.m_PrototypePath);
        RunString(L, buffer);
    }

    // Conditional preload. This is essentially testing async loading vs sync loading of parent collection
    // This only affects non-dynamic collection factories.
    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
        while (dmTime::GetMonotonicTime() < stop_time)
        {
            r = dmResource::UpdatePreloader(go_pr, 0, 0, 16*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(16*1000);
        }
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    // Spawn the game object with the script we want to call
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);
    if(go_pr)
    {
        dmResource::DeletePreloader(go_pr);
    }

    if(param.m_IsDynamic)
    {
        // validate that resources from dynamic collection factory is not loaded at this point. They will start loading from the script when updated below
        for (uint32_t i = 0; i < num_resources; ++i) {
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[i])));
        }

        // Do this twice in order to ensure load/unload can be called multiple times, with and without deleting created objects
        for(uint32_t i = 0; i < 2; ++i)
        {
            // state: load
            // update until instances are created through test script (collectionfactory.load and create)
            // 1) load factory resource using collectionfactory.load
            // 2) create 4 instances (two collectionfactory.create calls with a collection prototype that containes 2 references to gameobjects)
            dmhash_t last_object_id = i == 0 ? dmHashString64("/collection1/go") : dmHashString64("/collection3/go");
            for(;;)
            {
                if(dmGameObject::GetInstanceFromIdentifier(m_Collection, last_object_id) != 0x0)
                    break;
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(6, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

            // state: delete
            // first iteration will delete gameobjects created with factories, second will keep
            if (i == 0)
            {
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }

            // state: unload
            // call collectionfactory.unload, dereferencing 2 factory references.
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(i*0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(i*4, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));
        }

        // state: unload
        // call collectionfactory.unload again, which is ok by design (no operation)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(4, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // state: delete
        // delete resources created by collectionfactory.create calls. All resource should be released
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        for (uint32_t i = 0; i < num_resources; ++i) {
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[i])));
        }

        // state: create
        // recreate resources without collectionfactoy.load having been called (sync load on demand)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(4, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // state: delete
        // recreate resources without collectionfactoy.load having been called (sync load on demand)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);

        // Verify that we can unload the resources after the game objects have been deleted
        for (uint32_t i = 0; i < num_resources; ++i) {
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[i])));
        }

        if (param.m_PrototypePath)
        {
            // verify that the dynamic prototype resource hasn't been loaded yet
            for (uint32_t i = 0; i < num_dyn_prototype_resources; ++i) {
                ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
            }

            // --- state: load prototype ---
            // the default prototype is unloaded, and we've queued the new prototype to load
            dmhash_t last_object_id = dmHashString64("/collection6/go");
            for(;;)
            {
                if(dmGameObject::GetInstanceFromIdentifier(m_Collection, last_object_id) != 0x0)
                    break;
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }

            // The factory holds the prototype resource
            ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));

            // --- state: unload prototype ---
            // the custom prototype is unloaded
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);

            // Only the game objects hold the resources now
            ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));

            // --- state: delete game objects ---
            // the custom prototype is unloaded
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);

            for (uint32_t i = 0; i < num_dyn_prototype_resources; ++i) {
                ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
            }
        }

        // delete the root go and update so deferred deletes will be executed.
        dmGameObject::Delete(m_Collection, go, true);
        dmGameObject::Final(m_Collection);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        for (uint32_t i = 0; i < num_resources; ++i) {
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[i])));
        }
    }
    else
    {
        // validate that resources from collection factory is loaded with the parent collection.
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- state: load ---
        // call update which will create four instances (two collectionfactory.create calls with a collection prototype that containes two references to go)
        // We also call collectionfactory.load to ensure this does nothing except always invoke the loadcomplete callback (by design)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);

        // verify six instances created + two references from factory collection prototype
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(8, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- state: unload ---
        // call collectionfactory.unload which is a no-operation for non-dynamic factories
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(8, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- state: delete game objects ---
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);

        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        if (param.m_PrototypePath)
        {
            for (uint32_t i = 0; i < num_dyn_prototype_resources; ++i) {
                ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
            }

            // --- state: load prototype ---
            // the default prototype is unloaded, and we've queued the new prototype to load
            // update until instances are created through test script (collectionfactory.load and create)
            dmhash_t last_object_id = dmHashString64("/collection3/go");
            for(;;)
            {
                if(dmGameObject::GetInstanceFromIdentifier(m_Collection, last_object_id) != 0x0)
                    break;
                ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
                ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
                dmGameObject::PostUpdate(m_Register);
            }

            ASSERT_EQ(4, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[0])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[1])));

            // --- state: unload prototype ---
            // the custom prototype is unloaded
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);

            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
            ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

            // --- state: delete game objects ---
            // the custom prototype is unloaded
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);

            for (uint32_t i = 0; i < num_dyn_prototype_resources; ++i) {
                ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(dyn_prototype_resource_path[i])));
            }
        }

        // Delete the root go and update so deferred deletes will be executed.
        dmGameObject::Delete(m_Collection, go, true);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        for (uint32_t i = 0; i < num_resources; ++i) {
            ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[i])));
        }
    }

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(CollectionFactoryRecursivePrototypeTest, RecursivePrototype)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const CollectionFactoryTestParams& param = GetParam();
    const char* recursive_prototype_path = param.m_IsDynamic
        ? "/collection_factory/recursive_dynamic_collectionfactory_test.collectionc"
        : "/collection_factory/recursive_collectionfactory_test.collectionc";

    char buffer[256];
    dmSnPrintf(buffer, sizeof(buffer), "recursive_prototype_path = '%s'", recursive_prototype_path);
    RunString(L, buffer);

    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
        while (dmTime::GetMonotonicTime() < stop_time)
        {
            r = dmResource::UpdatePreloader(go_pr, 0, 0, 16*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(16*1000);
        }
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);

    if(go_pr)
    {
        dmResource::DeletePreloader(go_pr);
    }

    dmhash_t recursive_instance = 0;
    for(;;)
    {
        lua_getglobal(L, "global_recursive_created");
        bool ready = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if(ready)
        {
            lua_getglobal(L, "recursive_instance");
            recursive_instance = dmScript::CheckHash(L, -1);
            lua_pop(L, 1);
            if (dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance) != 0x0)
                break;
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
    }

    ASSERT_NE((dmhash_t)0, recursive_instance);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    ASSERT_EQ((dmGameObject::HInstance)0x0, dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance));

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(CollectionFactoryRecursivePrototypeTest, RecursivePrototypeCppCleanup)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const CollectionFactoryTestParams& param = GetParam();
    const char* recursive_prototype_path = param.m_IsDynamic
        ? "/collection_factory/recursive_dynamic_collectionfactory_test.collectionc"
        : "/collection_factory/recursive_collectionfactory_test.collectionc";

    char buffer[256];
    dmSnPrintf(buffer, sizeof(buffer), "recursive_prototype_path = '%s'", recursive_prototype_path);
    RunString(L, buffer);
    RunString(L, "recursive_cleanup_in_script = false");

    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetMonotonicTime() + 30*10e6;
        while (dmTime::GetMonotonicTime() < stop_time)
        {
            r = dmResource::UpdatePreloader(go_pr, 0, 0, 16*1000);
            if (r != dmResource::RESULT_PENDING)
                break;
            dmTime::Sleep(16*1000);
        }
        ASSERT_EQ(dmResource::RESULT_OK, r);
    }

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmhash_t go_hash = dmHashString64("/go");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE((void*)0, go);

    if(go_pr)
    {
        dmResource::DeletePreloader(go_pr);
    }

    dmhash_t recursive_instance = 0;
    for(;;)
    {
        lua_getglobal(L, "global_recursive_created");
        bool ready = !lua_isnil(L, -1);
        lua_pop(L, 1);
        if(ready)
        {
            lua_getglobal(L, "recursive_instance");
            recursive_instance = dmScript::CheckHash(L, -1);
            lua_pop(L, 1);
            if (dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance) != 0x0)
                break;
        }
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
    }

    ASSERT_NE((dmhash_t)0, recursive_instance);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    ASSERT_EQ((dmGameObject::HInstance)0x0, dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash));
    ASSERT_NE((dmGameObject::HInstance)0x0, dmGameObject::GetInstanceFromIdentifier(m_Collection, recursive_instance));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

/* Draw Count */

TEST_P(DrawCountTest, DrawCount)
{
    const DrawCountParams& p = GetParam();
    const char* go_path = p.m_GOPath;
    const uint64_t expected_draw_count = p.m_ExpectedDrawCount;

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Make the render list that will be used later.
    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_EQ(expected_draw_count, dmGraphics::GetDrawCount());
    dmGraphics::Flip(m_GraphicsContext);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Test that a GUI with mixed nodes (box + multiple text nodes) produces a single font
// dispatch for all text (not one per text node), and that text render order is preserved.
TEST_F(ComponentTest, GuiTextSingleFlushAndOrder)
{
    const char* go_path = "/gui/gui_text_flush_test.goc";
    const uint32_t num_text_nodes = 5;

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);

    dmRender::RenderContext* render_context_ptr = (dmRender::RenderContext*)m_RenderContext;

    // Count how many entries use each dispatch
    uint32_t dispatch_entry_count[256] = {};
    for (uint32_t i = 0; i < render_context_ptr->m_RenderList.Size(); ++i)
    {
        dispatch_entry_count[render_context_ptr->m_RenderList[i].m_Dispatch]++;
    }

    // With mixed nodes (4 boxes, 5 text interleaved) we get two batched dispatches:
    // one for the 4 box nodes, one for the 5 text nodes (single FlushTexts at end of RenderNodes).
    // The text dispatch is the only one with exactly num_text_nodes entries.
    uint32_t max_entries_for_any_dispatch = 0;
    for (uint32_t i = 0; i < render_context_ptr->m_RenderListDispatch.Size(); ++i)
    {
        if (dispatch_entry_count[i] > max_entries_for_any_dispatch)
        {
            max_entries_for_any_dispatch = dispatch_entry_count[i];
        }
    }
    ASSERT_GE(max_entries_for_any_dispatch, num_text_nodes);

    uint32_t dispatches_with_multiple_entries = 0;
    for (uint32_t i = 0; i < render_context_ptr->m_RenderListDispatch.Size(); ++i)
    {
        if (dispatch_entry_count[i] >= 2u)
        {
            dispatches_with_multiple_entries++;
        }
    }
    ASSERT_EQ(2u, dispatches_with_multiple_entries);

    // Find the text dispatch (only one with exactly num_text_nodes entries; boxes have 4).
    uint8_t text_dispatch = 255;
    for (uint32_t i = 0; i < render_context_ptr->m_RenderListDispatch.Size(); ++i)
    {
        if (dispatch_entry_count[i] == num_text_nodes)
        {
            text_dispatch = (uint8_t)i;
            break;
        }
    }
    ASSERT_LT(text_dispatch, render_context_ptr->m_RenderListDispatch.Size());

    uint32_t text_orders[32] = {};
    uint32_t num_text_orders = 0;
    for (uint32_t i = 0; i < render_context_ptr->m_RenderList.Size() && num_text_orders < 32; ++i)
    {
        if (render_context_ptr->m_RenderList[i].m_Dispatch == text_dispatch)
        {
            text_orders[num_text_orders++] = render_context_ptr->m_RenderList[i].m_Order;
        }
    }
    ASSERT_EQ(num_text_nodes, num_text_orders);

    // Order should be strictly increasing (scene order preserved)
    for (uint32_t i = 1; i < num_text_orders; ++i)
    {
        ASSERT_LT(text_orders[i - 1], text_orders[i]);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiPreparedTextLayoutInvalidation)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);

    dmGui::HScene scene = gui_component->m_Scene;
    dmGui::HNode node = dmGui::GetNodeById(scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);

    dmGameSystem::FontResource* dynamic_font_resource = (dmGameSystem::FontResource*) dmGui::GetNodeFont(scene, node);
    ASSERT_NE((void*)0, dynamic_font_resource);

    dmGui::TextLayout text_layout = {};
    dmGui::GetNodeTextLayout(scene, node, &text_layout);
    ASSERT_EQ((HTextLayout)0, text_layout.m_Handle);

    GuiTextSubmitResult initial = QueueGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(1u, initial.m_TextEntryCount);
    ASSERT_NE((HTextLayout)0, initial.m_TextLayout);
    ASSERT_EQ(0u, initial.m_TextBufferSize);
    ASSERT_EQ(dmHashString64("/font/valid.ttf"), GetTextLayoutGlyphFontPathHash(dynamic_font_resource, initial.m_TextLayout));

    dmGui::GetNodeTextLayout(scene, node, &text_layout);
    ASSERT_EQ(initial.m_TextLayout, text_layout.m_Handle);

    GuiTextSubmitResult repeated = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(initial.m_TextLayout, repeated.m_TextLayout);

    dmGui::SetNodeText(scene, node, "Cache me differently");
    GuiTextSubmitResult text_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(1u, text_changed.m_TextEntryCount);
    ASSERT_NE(initial.m_TextLayout, text_changed.m_TextLayout);
    ASSERT_EQ(0u, text_changed.m_TextBufferSize);

    Vector4 size = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_SIZE);
    size.setX(size.getX() + 32.0f);
    dmGui::SetNodeProperty(scene, node, dmGui::PROPERTY_SIZE, size);
    GuiTextSubmitResult width_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(text_changed.m_TextLayout, width_changed.m_TextLayout);

    dmGui::SetNodeLineBreak(scene, node, false);
    GuiTextSubmitResult line_break_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(width_changed.m_TextLayout, line_break_changed.m_TextLayout);

    dmGui::SetNodeTextLeading(scene, node, 1.5f);
    GuiTextSubmitResult leading_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(line_break_changed.m_TextLayout, leading_changed.m_TextLayout);

    dmGui::SetNodeTextTracking(scene, node, 0.5f);
    GuiTextSubmitResult tracking_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(leading_changed.m_TextLayout, tracking_changed.m_TextLayout);

    Vector4 position = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_POSITION);
    position.setX(position.getX() + 10.0f);
    position.setY(position.getY() - 5.0f);
    dmGui::SetNodeProperty(scene, node, dmGui::PROPERTY_POSITION, position);

    Vector4 color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
    color.setXYZ(Vector3(0.25f, 0.5f, 0.75f));
    color.setW(0.8f);
    dmGui::SetNodeProperty(scene, node, dmGui::PROPERTY_COLOR, color);

    GuiTextSubmitResult transform_color_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(tracking_changed.m_TextLayout, transform_color_changed.m_TextLayout);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeFont(scene, node, "secondary_font"));
    GuiTextSubmitResult font_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(transform_color_changed.m_TextLayout, font_changed.m_TextLayout);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeFont(scene, node, "dynamic_font"));
    GuiTextSubmitResult dynamic_font_changed = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(font_changed.m_TextLayout, dynamic_font_changed.m_TextLayout);

    uint32_t dynamic_font_version = dmGameSystem::ResFontGetVersion(dynamic_font_resource);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", 0));
    ASSERT_EQ(dynamic_font_version + 1, dmGameSystem::ResFontGetVersion(dynamic_font_resource));

    GuiTextSubmitResult reloaded_font = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE(dynamic_font_changed.m_TextLayout, reloaded_font.m_TextLayout);

    dmGui::SetNodeText(scene, node, "");
    GuiTextSubmitResult empty_text = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(0u, empty_text.m_TextEntryCount);
    ASSERT_EQ((HTextLayout)0, empty_text.m_TextLayout);
    ASSERT_EQ(0u, empty_text.m_TextBufferSize);

    dmGui::GetNodeTextLayout(scene, node, &text_layout);
    ASSERT_EQ((HTextLayout)0, text_layout.m_Handle);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiPreparedTextLayoutLifecycle)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);

    dmGui::Scene* scene = gui_component->m_Scene;
    dmGui::HNode node = dmGui::GetNodeById(scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);

    GuiTextSubmitResult initial = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, initial.m_TextLayout);

    dmGui::TextLayout text_layout = {};
    dmGui::GetNodeTextLayout(scene, node, &text_layout);
    ASSERT_EQ(initial.m_TextLayout, text_layout.m_Handle);

    dmGui::HNode cloned_node = 0;
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::CloneNode(scene, node, &cloned_node));
    ASSERT_NE((dmGui::HNode)0, cloned_node);

    dmGui::TextLayout cloned_text_layout = {};
    dmGui::GetNodeTextLayout(scene, cloned_node, &cloned_text_layout);
    ASSERT_EQ((HTextLayout)0, cloned_text_layout.m_Handle);

    dmGui::DeleteNode(scene, cloned_node);
    dmGui::ClearNodes(scene);
    ASSERT_EQ(0u, scene->m_Nodes.Size());

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiPreparedTextLayoutDestroyedBeforeDraw)
{
    // The GUI node can clear its cached prepared layout after queueing text but
    // before DrawRenderList. The queued render entry still needs its own ref.
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_RenderContext));

    GuiTextSubmitResult initial = QueueGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(1u, initial.m_TextEntryCount);
    ASSERT_NE((HTextLayout)0, initial.m_TextLayout);

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);

    dmGui::Scene* scene = gui_component->m_Scene;
    dmGui::HNode node = dmGui::GetNodeById(scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);

    dmGui::TextLayout text_layout = {};
    dmGui::GetNodeTextLayout(scene, node, &text_layout);
    ASSERT_EQ(initial.m_TextLayout, text_layout.m_Handle);

    dmGui::TextLayout cleared_layout = {};
    dmGui::SetNodeTextLayout(scene, node, cleared_layout);

    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_RenderContext));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, LabelPreparedTextLayoutInvalidation)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* label_component = GetLabelComponent(go, label_id);
    ASSERT_NE((void*)0, label_component);

    dmRender::TextMetrics initial_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, initial_metrics);
    ASSERT_GT(initial_metrics.m_Width, 0.0f);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    PostLabelSetText(m_Collection, go_id, label_id, "Label Label Label", (uintptr_t)go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmRender::TextMetrics text_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, text_metrics);
    ASSERT_GT(text_metrics.m_Width, initial_metrics.m_Width);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    dmGameObject::PropertyOptions options;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, dmHashString64("tracking"), options, dmGameObject::PropertyVar(1.0f)));

    dmRender::TextMetrics tracking_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, tracking_metrics);
    ASSERT_GT(tracking_metrics.m_Width, text_metrics.m_Width);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, dmHashString64("line_break"), options, dmGameObject::PropertyVar(true)));

    dmRender::TextMetrics wrapped_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, wrapped_metrics);
    ASSERT_GT(wrapped_metrics.m_LineCount, 1u);
    ASSERT_GT(wrapped_metrics.m_Height, tracking_metrics.m_Height);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, dmHashString64("leading"), options, dmGameObject::PropertyVar(2.0f)));

    dmRender::TextMetrics leading_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, leading_metrics);
    ASSERT_GT(leading_metrics.m_Height, wrapped_metrics.m_Height);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, dmHashString64("size"), options, dmGameObject::PropertyVar(Vector3(1000.0f, 1.0f, 1.0f))));

    dmRender::TextMetrics resized_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, resized_metrics);
    ASSERT_LT(resized_metrics.m_Height, leading_metrics.m_Height);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    dmGameSystem::FontResource* dynamic_font_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", (void**) &dynamic_font_resource));
    ASSERT_NE((void*)0, dynamic_font_resource);

    const dmhash_t dynamic_font = dmHashString64("/font/dyn_glyph_bank_test_1.fontc");
    const dmhash_t default_dynamic_ttf = dmHashString64("/font/valid.ttf");
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetResourceProperty(go, label_id, dmHashString64("font"), dynamic_font));

    dmRender::TextMetrics dynamic_font_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, dynamic_font_metrics);
    ASSERT_GT(dynamic_font_metrics.m_Width, 0.0f);

    HTextLayout dynamic_font_layout = PrepareLabelAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, dynamic_font_layout);
    ASSERT_EQ(default_dynamic_ttf, GetTextLayoutGlyphFontPathHash(dynamic_font_resource, dynamic_font_layout));

    uint32_t dynamic_font_version = dmGameSystem::ResFontGetVersion(dynamic_font_resource);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", 0));
    ASSERT_EQ(dynamic_font_version + 1, dmGameSystem::ResFontGetVersion(dynamic_font_resource));

    dmRender::TextMetrics reloaded_dynamic_font_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, reloaded_dynamic_font_metrics);
    ASSERT_GT(reloaded_dynamic_font_metrics.m_Width, 0.0f);

    HTextLayout reloaded_dynamic_font_layout = PrepareLabelAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, reloaded_dynamic_font_layout);
    ASSERT_EQ(default_dynamic_ttf, GetTextLayoutGlyphFontPathHash(dynamic_font_resource, reloaded_dynamic_font_layout));

    DeleteInstance(m_Collection, go);
    dmResource::Release(m_Factory, dynamic_font_resource);

    const dmhash_t font_go_id = dmHashString64("/font_go");
    dmGameObject::HInstance font_go = Spawn(m_Factory, m_Collection, "/resource/res_getset_prop.goc", font_go_id);
    ASSERT_NE((void*)0, font_go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* font_label_component = GetLabelComponent(font_go, label_id);
    ASSERT_NE((void*)0, font_label_component);

    void* replacement_font_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/resource/font.fontc", &replacement_font_resource));

    const dmhash_t replacement_font = dmHashString64("/resource/font.fontc");
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetResourceProperty(font_go, label_id, dmHashString64("font"), replacement_font));

    dmhash_t font_hash = 0;
    GetResourceProperty(font_go, label_id, dmHashString64("font"), &font_hash);
    ASSERT_EQ(replacement_font, font_hash);

    dmRender::TextMetrics font_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(font_label_component, font_metrics);
    ASSERT_GT(font_metrics.m_Width, 0.0f);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    dmResource::Release(m_Factory, replacement_font_resource);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, LabelPreparedTextLayoutDestroyedBeforeDraw)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");

    // Labels hit the same lifetime issue as GUI text: mutating the component
    // after queueing must not free the prepared layout out from under render.
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Queue a prepared layout, then invalidate the label before the queued text
    // is consumed. Without render-queue ownership this used to crash later in
    // CreateFontVertexDataFromTextLayout()/OutputGlyph() on a freed layout.
    HTextLayout prepared_layout = QueueLabelAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, prepared_layout);

    dmGameObject::PropertyOptions options = {};
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK,
              dmGameObject::SetProperty(go, label_id, dmHashString64("tracking"), options, dmGameObject::PropertyVar(1.0f)));

    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    ASSERT_EQ(dmRender::RESULT_OK, dmRender::ClearRenderObjects(m_RenderContext));

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, LabelPreparedTextLayoutFallbackMutation)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");
    const dmhash_t dynamic_font = dmHashString64("/font/dyn_glyph_bank_test_1.fontc");
    const char* extra_ttf_path = "/font/NotoSansArabic-Regular.ttf";
    const dmhash_t extra_ttf_hash = dmHashString64(extra_ttf_path);
    const dmhash_t default_ttf_hash = dmHashString64("/font/valid.ttf");
#if !defined(FONT_USE_HARFBUZZ) || !defined(FONT_USE_SKRIBIDI)
    (void)extra_ttf_hash;
#endif

    uint32_t extra_ttf_size = 0;
    uint8_t* extra_ttf_data = dmTestUtil::ReadHostFile("src/gamesys/test/font/NotoSansArabic-Regular.ttf", &extra_ttf_size);
    ASSERT_NE((void*)0, extra_ttf_data);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, extra_ttf_path, extra_ttf_size, extra_ttf_data));

    dmGameSystem::FontResource* dynamic_font_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", (void**) &dynamic_font_resource));
    ASSERT_NE((void*)0, dynamic_font_resource);

    dmGameSystem::TTFResource* extra_ttf_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetWithExt(m_Factory, extra_ttf_path, "ttf", (void**) &extra_ttf_resource));
    ASSERT_NE((void*)0, extra_ttf_resource);

    HFontCollection font_collection = dmRender::GetFontCollection(dmGameSystem::ResFontGetHandle(dynamic_font_resource));
    HFont default_font = FontCollectionGetFont(font_collection, 0);
    HFont extra_font = dmGameSystem::GetFont(extra_ttf_resource);

    uint32_t fallback_codepoint = 0;
    ASSERT_TRUE(FindFallbackCodepoint(default_font, extra_font, 0x0600, 0x06ff, &fallback_codepoint));

    char fallback_text[8] = {0};
    uint32_t fallback_text_len = dmUtf8::ToUtf8((uint16_t)fallback_codepoint, fallback_text);
    ASSERT_GT(fallback_text_len, 0u);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetResourceProperty(go, label_id, dmHashString64("font"), dynamic_font));

    PostLabelSetText(m_Collection, go_id, label_id, fallback_text, (uintptr_t)go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    HTextLayout initial_layout = PrepareLabelAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, initial_layout);
    ASSERT_EQ(default_ttf_hash, GetTextLayoutGlyphFontPathHash(dynamic_font_resource, initial_layout));

    lua_State* L = m_Scriptlibcontext.m_LuaState;
    ASSERT_TRUE(RunString(L, "font.add_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), \"/font/NotoSansArabic-Regular.ttf\")"));

    HTextLayout added_layout = PrepareLabelAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, added_layout);
    dmhash_t added_font_hash = GetTextLayoutGlyphFontPathHash(dynamic_font_resource, added_layout);
#if defined(FONT_USE_HARFBUZZ) && defined(FONT_USE_SKRIBIDI)
    ASSERT_EQ(extra_ttf_hash, added_font_hash);
#else
    ASSERT_EQ(default_ttf_hash, added_font_hash);
#endif

    ASSERT_TRUE(RunString(L, "font.remove_font(hash(\"/font/dyn_glyph_bank_test_1.fontc\"), \"/font/NotoSansArabic-Regular.ttf\")"));

    HTextLayout removed_layout = PrepareLabelAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, removed_layout);
    ASSERT_EQ(default_ttf_hash, GetTextLayoutGlyphFontPathHash(dynamic_font_resource, removed_layout));

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmResource::Release(m_Factory, dynamic_font_resource);
    dmResource::Release(m_Factory, extra_ttf_resource);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, extra_ttf_path));
    dmMemory::AlignedFree(extra_ttf_data);
}

/* GUI Box Render */

void AssertVertexEqual(const dmGameSystem::BoxVertex& lhs, const dmGameSystem::BoxVertex& rhs)
{
    EXPECT_NEAR(lhs.m_Position[0], rhs.m_Position[0], EPSILON);
    EXPECT_NEAR(lhs.m_Position[1], rhs.m_Position[1], EPSILON);
    EXPECT_NEAR(lhs.m_UV[0], rhs.m_UV[0], EPSILON);
    EXPECT_NEAR(lhs.m_UV[1], rhs.m_UV[1], EPSILON);
    EXPECT_NEAR(lhs.m_PageIndex, rhs.m_PageIndex, EPSILON);
}

TEST_P(BoxRenderTest, BoxRender)
{
    const BoxRenderParams& p = GetParam();
    const char* go_path = p.m_GOPath;

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Make the render list that will be used later.
    dmRender::RenderListBegin(m_RenderContext);

    uint32_t component_type_index = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world = (dmGameSystem::GuiWorld*)dmGameObject::GetWorld(m_Collection, component_type_index);

    // could use dmGameObject::GetWorld() if we had the component index
    dmGameSystem::GuiWorld* world = gui_world;
    dmGui::SetSceneAdjustReference(world->m_Components[0]->m_Scene, dmGui::ADJUST_REFERENCE_DISABLED);

    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    ASSERT_EQ(world->m_ClientVertexBuffer.Size(), (uint32_t)p.m_ExpectedVerticesCount);

    for (int i = 0; i < p.m_ExpectedVerticesCount; i++)
    {
        AssertVertexEqual(world->m_ClientVertexBuffer[i], p.m_ExpectedVertices[p.m_ExpectedIndices[i]]);
    }

    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGraphics::Flip(m_GraphicsContext);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

/* Gamepad connected */

TEST_F(GamepadConnectedTest, TestGamepadConnectedInputEvent)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/input/connected_event_test.goc", dmHashString64("/gamepad_connected"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    // test gamepad connected with device name and connected flag
    dmGameObject::InputAction input_action_connected;
    input_action_connected.m_ActionId = dmHashString64("gamepad_connected");
    input_action_connected.m_GamepadConnected = 1;
    input_action_connected.m_TextCount = dmStrlCpy(input_action_connected.m_Text, "null_device", sizeof(input_action_connected.m_Text));
    dmGameObject::UpdateResult res = dmGameObject::DispatchInput(m_Collection, &input_action_connected, 1);

    ASSERT_TRUE(res == dmGameObject::UpdateResult::UPDATE_RESULT_OK);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // test gamepad connected with empty device name
    dmGameObject::InputAction input_action_empty;
    input_action_empty.m_ActionId = dmHashString64("gamepad_connected_0");
    input_action_empty.m_TextCount = 0;
    input_action_empty.m_GamepadConnected = 1;
    res = dmGameObject::DispatchInput(m_Collection, &input_action_empty, 1);

    ASSERT_TRUE(res == dmGameObject::UpdateResult::UPDATE_RESULT_OK);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // test gamepad connected with device name and no connected flag
    dmGameObject::InputAction input_action_other;
    input_action_other.m_ActionId = dmHashString64("other_event");
    input_action_other.m_GamepadConnected = 0;
    input_action_other.m_TextCount = dmStrlCpy(input_action_other.m_Text, "null_device", sizeof(input_action_other.m_Text));
    res = dmGameObject::DispatchInput(m_Collection, &input_action_other, 1);

    ASSERT_TRUE(res == dmGameObject::UpdateResult::UPDATE_RESULT_OK);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // cleanup
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(CollisionObject2DTest, WakingCollisionObjectTest)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_sleepy_go = "/collision_object/sleepy_base.goc";
    dmhash_t hash_base_go = dmHashString64("/base-go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance base_go = Spawn(m_Factory, m_Collection, path_sleepy_go, hash_base_go, 0, Point3(50, -10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, base_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body_go = "/collision_object/body.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (10,10)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body1_go, 0, Point3(10,10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (50,10)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body2_go, 0, Point3(50,10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);


    // iterate until the lua env signals the end of the test of error occurs
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

// Test case for collision-object properties
TEST_F(CollisionObject2DTest, PropertiesTest)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_go = "/collision_object/properties.goc";
    dmhash_t hash_go = dmHashString64("/go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance properties_go = Spawn(m_Factory, m_Collection, path_go, hash_go, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, properties_go);

    // iterate until the lua env signals the end of the test of error occurs
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(Trigger2DTest, EventTriggerFalseTest)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* path_trigger_go = "/collision_object/eventtrigger_false_trigger.goc";
    dmhash_t hash_trigger_go = dmHashString64("/trigger-go");
    // place this body standing on the base with its center at (20,5)
    dmGameObject::HInstance trigger_go = Spawn(m_Factory, m_Collection, path_trigger_go, hash_trigger_go, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, trigger_go);

    const char* path_body1_go = "/collision_object/eventtrigger_false_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);

    // iterate until the lua env signals the end of the test of error occurs
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(GroupAndMask2DTest, GroupAndMaskTest )
{
    const GroupAndMaskParams& params = GetParam();

    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

/*
    An example actions set:

    "group body1-go#co user\n"
    "addmask body1-go#co enemy\n"
    "removemask body1-go#co default\n"
    "group body2-go#co enemy\n"
    "addmask body2-go#co user\n"
    "removemask body2-go#co default"
    ;
*/

    lua_pushstring(L, params.m_Actions); //actions);
    lua_setglobal(L, "actions");
    lua_pushboolean(L, params.m_CollisionExpected); //true);
    lua_setglobal(L, "collision_expected");

    // Note, body2 should get spawned before body1. body1 contains script code and init() function of that code is run when it's spawned thus missing body2.
    const char* path_body2_go = "/collision_object/groupmask_body2.goc";
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (20,5)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body2_go, hash_body2_go, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body1_go = "/collision_object/groupmask_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);

    // iterate until the lua env signals the end of the test of error occurs
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_P(GroupAndMask3DTest, GroupAndMaskTest)
{
    const GroupAndMaskParams& params = GetParam();

    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

/*
    An example actions set:

    "group body1-go#co user\n"
    "addmask body1-go#co enemy\n"
    "removemask body1-go#co default\n"
    "group body2-go#co enemy\n"
    "addmask body2-go#co user\n"
    "removemask body2-go#co default"
    ;
*/

    lua_pushstring(L, params.m_Actions); //actions);
    lua_setglobal(L, "actions");
    lua_pushboolean(L, params.m_CollisionExpected); //true);
    lua_setglobal(L, "collision_expected");

    // Note, body2 should get spawned before body1. body1 contains script code and init() function of that code is run when it's spawned thus missing body2.
    const char* path_body2_go = "/collision_object/groupmask_body2.goc";
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (20,5)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body2_go, hash_body2_go, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body1_go = "/collision_object/groupmask_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);

    // iterate until the lua env signals the end of the test of error occurs
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

GroupAndMaskParams groupandmask_params[] = {
    {"", true}, // group1: default, mask1: default,user,enemy group2: default, mask2: default,user,enemy
    {"removemask body1-go#co default", false},
    {"removemask body2-go#co default", false},
    {"group body1-go#co user\nremovemask body1-go#co enemy\naddmask body1-go#co enemy\nremovemask body1-go#co default\nremovemask body1-go#co user\ngroup body2-go#co enemy\nremovemask body2-go#co user\naddmask body2-go#co user\nremovemask body2-go#co default", true},

};
INSTANTIATE_TEST_CASE_P(GroupAndMaskTest, GroupAndMask2DTest, jc_test_values_in(groupandmask_params));
INSTANTIATE_TEST_CASE_P(GroupAndMaskTest, GroupAndMask3DTest, jc_test_values_in(groupandmask_params));

TEST_F(VelocityThreshold2DTest, VelocityThresholdTest)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = L;
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body_go = "/collision_object/body.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (20,5)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body2_go, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_sleepy_go = "/collision_object/velocity_threshold_base.goc";
    dmhash_t hash_base_go = dmHashString64("/base-go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance base_go = Spawn(m_Factory, m_Collection, path_sleepy_go, hash_base_go, 0, Point3(50, -10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, base_go);

    // iterate until the lua env signals the end of the test of error occurs
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ComponentTest, DispatchBuffersTest)
{
    dmHashEnableReverseHash(true);

    dmRender::RenderContext* render_context_ptr  = (dmRender::RenderContext*) m_RenderContext;
    render_context_ptr->m_MultiBufferingRequired = 1;

    void* sprite_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
    ASSERT_NE((void*) 0, sprite_world);

    void* model_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc")));
    ASSERT_NE((void*) 0, model_world);

    void* particlefx_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("particlefxc")));
    ASSERT_NE((void*) 0, particlefx_world);

    void* tilegrid_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("tilemapc")));
    ASSERT_NE((void*) 0, tilegrid_world);

    ////////////////////////////////////////////////////////////////////////////////////////////
    // Test setup
    // ----------
    // The idea of this test is to make sure that we produce the correct vertex buffers
    // when using a "multi-buffered" render approach, which should be the case for
    // non-opengl graphics adapters.
    //
    // To test this, the test .go file contains a bunch of components that support this feature.
    // We instantiate it and then dispatch a number of draw calls, which should trigger the
    // multi-buffering of the vertex and index buffers (where applicable).
    //
    // Furthermore, each component type is represented twice, with a different material per
    // instance. The two materials have different vertex formats, which we also account for
    // when producing our "expected" data for this test.
    ////////////////////////////////////////////////////////////////////////////////////////////

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/misc/dispatch_buffers_test/dispatch_buffers_test.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    // Play particlefx
    dmMessage::URL receiver;
    receiver.m_Socket   = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path     = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = 0;
    dmMessage::Post(
            0, &receiver,
            dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash,
            (uintptr_t) go,
            (uintptr_t) dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor,
            0, 0, 0);

    // Update and sleep to force generation of a particle
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmTime::Sleep(16*1000);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);

    // Do a couple of dispatches, this should allocate multiple buffers since we have forced multi-buffering
    const uint8_t num_draws = 4;
    for (int i = 0; i < num_draws; ++i)
    {
        dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    }

    // Vertex format for /misc/dispatch_buffers_test/vs_format_a.vp:
    // attribute vec4 position;
    // attribute float page_index;
    struct vs_format_a
    {
        float position[3];
        float page_index;
    };

    // Vertex format for /misc/dispatch_buffers_test/vs_format_b.vp:
    // attribute vec4 position;
    // attribute vec3 my_custom_attribute;
    struct vs_format_b
    {
        float position[3];
        float my_custom_attribute[4];
    };

    const uint32_t vertex_stride_a = sizeof(vs_format_a);
    const uint32_t vertex_stride_b = sizeof(vs_format_b);

    #define SET_VTX_A(vtx, x,y,z, pi) \
        vtx.position[0] = x; \
        vtx.position[1] = y; \
        vtx.position[2] = z; \
        vtx.page_index = pi;
    #define SET_VTX_B(vtx, x,y,z, c0,c1,c2,c3) \
        vtx.position[0] = x; \
        vtx.position[1] = y; \
        vtx.position[2] = z; \
        vtx.my_custom_attribute[0] = c0; \
        vtx.my_custom_attribute[1] = c1; \
        vtx.my_custom_attribute[2] = c2; \
        vtx.my_custom_attribute[3] = c3;

    #define ASSERT_VTX_A_EQ(vtx_1, vtx_2) \
        ASSERT_NEAR(vtx_1.position[0], vtx_2.position[0], EPSILON); \
        ASSERT_NEAR(vtx_1.position[1], vtx_2.position[1], EPSILON); \
        ASSERT_NEAR(vtx_1.position[2], vtx_2.position[2], EPSILON); \
        ASSERT_NEAR(vtx_1.page_index, vtx_2.page_index, EPSILON);

    #define ASSERT_VTX_B_EQ(vtx_1, vtx_2) \
        ASSERT_NEAR(vtx_1.position[0], vtx_2.position[0], EPSILON); \
        ASSERT_NEAR(vtx_1.position[1], vtx_2.position[1], EPSILON); \
        ASSERT_NEAR(vtx_1.position[2], vtx_2.position[2], EPSILON); \
        ASSERT_NEAR(vtx_1.my_custom_attribute[0], vtx_2.my_custom_attribute[0], EPSILON); \
        ASSERT_NEAR(vtx_1.my_custom_attribute[1], vtx_2.my_custom_attribute[1], EPSILON); \
        ASSERT_NEAR(vtx_1.my_custom_attribute[2], vtx_2.my_custom_attribute[2], EPSILON); \
        ASSERT_NEAR(vtx_1.my_custom_attribute[3], vtx_2.my_custom_attribute[3], EPSILON);

    ///////////////////////////////////////
    // Sprite
    ///////////////////////////////////////
    {
        dmRender::BufferedRenderBuffer* vx_buffer;
        dmRender::BufferedRenderBuffer* ix_buffer;
        dmGameSystem::GetSpriteWorldRenderBuffers(sprite_world,  &vx_buffer, &ix_buffer);

        ASSERT_EQ(num_draws, vx_buffer->m_Buffers.Size());
        ASSERT_EQ(num_draws, ix_buffer->m_Buffers.Size());

        ASSERT_EQ(dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER, vx_buffer->m_Type);
        ASSERT_EQ(dmRender::RENDER_BUFFER_TYPE_INDEX_BUFFER, ix_buffer->m_Type);

        const uint32_t vertex_count   = 4;
        const uint32_t vertex_padding = vertex_stride_b - (vertex_stride_a * vertex_count) % vertex_stride_b;
        const uint32_t buffer_size    = (vertex_stride_a + vertex_stride_b) * vertex_count + vertex_padding;
        uint8_t buffer[buffer_size];

        vs_format_a* sprite_a = (vs_format_a*) &buffer[0];
        vs_format_b* sprite_b = (vs_format_b*) &buffer[vertex_stride_a * vertex_count + vertex_padding];

        const float sprite_a_w = 32.0f;
        const float sprite_a_h = 32.0f;
        const float sprite_b_w = 16.0f;
        const float sprite_b_h = 16.0f;

        // Notice: z value is 1.0f here to make the sorting stable
        SET_VTX_A(sprite_a[0], -sprite_a_w / 2.0f, -sprite_a_h / 2.0f, 1.0f, 0.0f);
        SET_VTX_A(sprite_a[1], -sprite_a_w / 2.0f,  sprite_a_h / 2.0f, 1.0f, 0.0f);
        SET_VTX_A(sprite_a[2],  sprite_a_w / 2.0f,  sprite_a_h / 2.0f, 1.0f, 0.0f);
        SET_VTX_A(sprite_a[3],  sprite_a_w / 2.0f, -sprite_a_h / 2.0f, 1.0f, 0.0f);

        SET_VTX_B(sprite_b[0], -sprite_b_w / 2.0f, -sprite_b_h / 2.0f, 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(sprite_b[1], -sprite_b_w / 2.0f,  sprite_b_h / 2.0f, 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(sprite_b[2],  sprite_b_w / 2.0f,  sprite_b_h / 2.0f, 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(sprite_b[3],  sprite_b_w / 2.0f, -sprite_b_h / 2.0f, 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);

        for (int i = 0; i < num_draws; ++i)
        {
            // TODO: Maybe validate index buffer here as well
            dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer->m_Buffers[i];
            ASSERT_EQ(buffer_size, gfx_vx_buffer->m_Size);

            vs_format_a* written_sprite_a = (vs_format_a*) &gfx_vx_buffer->m_Buffer[0];
            vs_format_b* written_sprite_b = (vs_format_b*) &gfx_vx_buffer->m_Buffer[vertex_stride_a * vertex_count + vertex_padding];

            for (int j = 0; j < vertex_count; ++j)
            {
                ASSERT_VTX_A_EQ(sprite_a[j], written_sprite_a[j]);
                ASSERT_VTX_B_EQ(sprite_b[j], written_sprite_b[j]);
            }
        }
    }

    ///////////////////////////////////////
    // Model
    ///////////////////////////////////////
    {
        uint32_t vx_buffers_count;
        dmRender::BufferedRenderBuffer** vx_buffers;
        dmGameSystem::GetModelWorldRenderBuffers(model_world, &vx_buffers, &vx_buffers_count);
        ASSERT_TRUE(vx_buffers_count > 0);

        dmRender::BufferedRenderBuffer* vx_buffer = vx_buffers[0];
        ASSERT_EQ(num_draws, vx_buffer->m_Buffers.Size());
        ASSERT_EQ(dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER, vx_buffer->m_Type);

        const uint32_t vertex_count        = 6;
        const uint32_t buffer_write_offset = vertex_stride_a * vertex_count;
        ASSERT_TRUE(buffer_write_offset % vertex_stride_b != 0);

        const uint32_t vertex_padding = vertex_stride_b - (vertex_stride_a * vertex_count) % vertex_stride_b;
        const uint32_t buffer_size    = vertex_stride_a * vertex_count + vertex_padding + vertex_stride_b * vertex_count;

        uint8_t buffer[buffer_size];
        vs_format_a* model_a = (vs_format_a*) &buffer[0];
        vs_format_b* model_b = (vs_format_b*) &buffer[vertex_stride_a * vertex_count + vertex_padding];

        // NOTE: The z component here is different between these two components since we want to sort them in a specific order.
        float p0[] = {  1.0,  1.0 };
        float p1[] = { -1.0,  1.0 };
        float p2[] = { -1.0, -1.0 };
        float p3[] = {  1.0, -1.0 };

        SET_VTX_A(model_a[0], p0[0], p0[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[1], p1[0], p1[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[2], p2[0], p2[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[3], p0[0], p0[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[4], p2[0], p2[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[5], p3[0], p3[1], 1.0f, 0.0f);

        SET_VTX_B(model_b[0], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[1], p1[0], p1[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[2], p2[0], p2[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[3], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[4], p2[0], p2[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[5], p3[0], p3[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);

        for (int i = 0; i < num_draws; ++i)
        {
            // TODO: Maybe validate index buffer here as well
            dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer->m_Buffers[i];
            ASSERT_EQ(buffer_size, gfx_vx_buffer->m_Size);

            vs_format_a* written_model_a = (vs_format_a*) &gfx_vx_buffer->m_Buffer[0];
            vs_format_b* written_model_b = (vs_format_b*) &gfx_vx_buffer->m_Buffer[vertex_stride_a * vertex_count + vertex_padding];

            for (int j = 0; j < vertex_count; ++j)
            {
                ASSERT_VTX_A_EQ(model_a[j], written_model_a[j]);
                ASSERT_VTX_B_EQ(model_b[j], written_model_b[j]);
            }
        }
    }

    ///////////////////////////////////////
    // Particle
    ///////////////////////////////////////
    {
        dmRender::BufferedRenderBuffer* vx_buffer;
        dmGameSystem::GetParticleFXWorldRenderBuffers(particlefx_world, &vx_buffer);
        ASSERT_EQ(num_draws, vx_buffer->m_Buffers.Size());
        ASSERT_EQ(dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER, vx_buffer->m_Type);

        const uint32_t vertex_count   = 6;
        const uint32_t vertex_padding = vertex_stride_b - (vertex_stride_a * vertex_count) % vertex_stride_b;
        const uint32_t buffer_size    = vertex_stride_a * (vertex_count + 6) + vertex_stride_b * (vertex_count + 6); // we allocate for an extra particle
        uint8_t buffer[buffer_size];

        vs_format_a* pfx_a = (vs_format_a*) &buffer[0];
        vs_format_b* pfx_b = (vs_format_b*) &buffer[vertex_stride_a * vertex_count + vertex_padding];

        const float pfx_s = 20.0f / 2.0f;

        float p0[] = { -pfx_s, -pfx_s};
        float p1[] = { -pfx_s,  pfx_s};
        float p2[] = {  pfx_s, -pfx_s};
        float p3[] = {  pfx_s,  pfx_s};

        SET_VTX_A(pfx_a[0], p0[0], p0[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[1], p1[0], p1[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[2], p3[0], p3[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[3], p3[0], p3[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[4], p2[0], p2[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[5], p0[0], p0[1], 1.0f, 0.0f);

        SET_VTX_B(pfx_b[0], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[1], p1[0], p1[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[2], p3[0], p3[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[3], p3[0], p3[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[4], p2[0], p2[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[5], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);

        for (int i = 0; i < num_draws; ++i)
        {
            dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer->m_Buffers[i];
            ASSERT_EQ(buffer_size, gfx_vx_buffer->m_Size);

            vs_format_a* written_pfx_a = (vs_format_a*) &gfx_vx_buffer->m_Buffer[0];
            vs_format_b* written_pfx_b = (vs_format_b*) &gfx_vx_buffer->m_Buffer[vertex_stride_a * vertex_count + vertex_padding];

            for (int j = 0; j < vertex_count; ++j)
            {
                ASSERT_VTX_A_EQ(pfx_a[j], written_pfx_a[j]);
                ASSERT_VTX_B_EQ(pfx_b[j], written_pfx_b[j]);
            }
        }
    }

    ///////////////////////////////////////
    // Tilegrid
    ///////////////////////////////////////
    {
        dmRender::BufferedRenderBuffer* vx_buffer;
        dmGameSystem::GetTileGridWorldRenderBuffers(tilegrid_world, &vx_buffer);
        ASSERT_EQ(num_draws, vx_buffer->m_Buffers.Size());
        ASSERT_EQ(dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER, vx_buffer->m_Type);

        // Note: Tilegrids does not support custom vertex formats, so for the sake of this test
        //       we only care about the buffer dispatching part.
    }

    #undef SET_VTX_A
    #undef SET_VTX_B
    #undef ASSERT_VTX_A_EQ
    #undef ASSERT_VTX_B_EQ

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, ParticleFXRenderScriptMaterialOverrideAttributeSizeMismatch)
{
    dmHashEnableReverseHash(true);

    dmRender::RenderContext* render_context_ptr  = (dmRender::RenderContext*) m_RenderContext;
    render_context_ptr->m_MultiBufferingRequired = 1;

    void* particlefx_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("particlefxc")));
    ASSERT_NE((void*) 0, particlefx_world);

    dmParticle::Prototype* particlefx_prototype = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/particlefx/attribute_mismatch.particlefxc", (void**) &particlefx_prototype));
    ASSERT_EQ(1u, particlefx_prototype->m_DDF->m_Emitters.m_Count);
    ASSERT_EQ(1u, particlefx_prototype->m_DDF->m_Emitters[0].m_Attributes.m_Count);

    dmGraphics::VertexAttribute& scalar_attribute = particlefx_prototype->m_DDF->m_Emitters[0].m_Attributes[0];

    // The waf test compiler does not run Bob's particlefx attribute packing/name-hash step.
    // Patch the runtime fields here, and keep the scalar backing allocation to one float
    // so ASan catches the mat4 read performed after the material override below.
    scalar_attribute.m_NameHash = dmHashString64("crash_attr");
    ASSERT_EQ((uint32_t) sizeof(float), scalar_attribute.m_Values.m_BinaryValues.m_Count);
    uint8_t* original_scalar_values = scalar_attribute.m_Values.m_BinaryValues.m_Data;
    float* scalar_value = new float(1.0f);
    scalar_attribute.m_Values.m_BinaryValues.m_Data = (uint8_t*) scalar_value;

    dmGameSystem::MaterialResource* mat4_material_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/particlefx/attribute_mismatch_mat4.materialc", (void**) &mat4_material_resource));

    const char* render_script_source =
        "function init(self)\n"
        "    self.predicate = render.predicate({\"particle\"})\n"
        "end\n"
        "function update(self)\n"
        "    render.enable_material(\"attribute_mismatch_mat4\")\n"
        "    render.draw(self.predicate)\n"
        "    render.disable_material()\n"
        "end\n";
    dmLuaDDF::LuaSource lua_source;
    memset(&lua_source, 0, sizeof(lua_source));
    lua_source.m_Script.m_Data = (uint8_t*) render_script_source;
    lua_source.m_Script.m_Count = strlen(render_script_source);
    lua_source.m_Bytecode.m_Data = (uint8_t*) render_script_source;
    lua_source.m_Bytecode.m_Count = strlen(render_script_source);
    lua_source.m_Bytecode64.m_Data = (uint8_t*) render_script_source;
    lua_source.m_Bytecode64.m_Count = strlen(render_script_source);
    lua_source.m_Filename = "particlefx-attribute-mismatch-render-script";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_RenderContext, &lua_source);
    ASSERT_NE((dmRender::HRenderScript) 0, render_script);
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_RenderContext, render_script);
    ASSERT_NE((dmRender::HRenderScriptInstance) 0, render_script_instance);
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "attribute_mismatch_mat4", (uint64_t) mat4_material_resource->m_Material, dmRender::RENDER_RESOURCE_TYPE_MATERIAL);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/particlefx/attribute_mismatch.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    dmMessage::URL receiver;
    receiver.m_Socket   = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path     = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = 0;
    dmMessage::Post(
            0, &receiver,
            dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash,
            (uintptr_t) go,
            (uintptr_t) dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor,
            0, 0, 0);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    dmTime::Sleep(16*1000);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    dmRender::RenderListEnd(m_RenderContext);
    dmGraphics::BeginFrame(m_GraphicsContext);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, m_UpdateContext.m_DT));

    dmRender::BufferedRenderBuffer* vx_buffer;
    dmGameSystem::GetParticleFXWorldRenderBuffers(particlefx_world, &vx_buffer);
    ASSERT_EQ(1u, vx_buffer->m_Buffers.Size());

    scalar_attribute.m_Values.m_BinaryValues.m_Data = original_scalar_values;
    delete scalar_value;
    dmResource::Release(m_Factory, particlefx_prototype);
    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_RenderContext, render_script);
    dmResource::Release(m_Factory, mat4_material_resource);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, DispatchBuffersInstancingTest)
{
    dmHashEnableReverseHash(true);

    dmRender::RenderContext* render_context_ptr  = (dmRender::RenderContext*) m_RenderContext;

    render_context_ptr->m_MultiBufferingRequired = 1;

    void* model_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc")));
    ASSERT_NE((void*) 0, model_world);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/misc/dispatch_buffers_instancing_test/dispatch_buffers_instancing_test.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);

    struct vs_format_a
    {
        float position[3];
        float page_index;
    };

    struct inst_format_a
    {
        float mtx_world[16];
    };

    struct vs_format_b
    {
        float position[3];
        float my_custom_vertex_attribute[2];
    };

    struct inst_format_b
    {
        float mtx_world[16];
        float my_custom_instance_attribute[4];
    };

    const uint32_t vertex_stride_a   = sizeof(vs_format_a);
    const uint32_t vertex_stride_b   = sizeof(vs_format_b);
    const uint32_t instance_stride_a = sizeof(inst_format_a);
    const uint32_t instance_stride_b = sizeof(inst_format_b);

    /////////////////////////////////////////////
    // Model
    /////////////////////////////////////////////
    {
        dmGameSystem::MaterialResource *material_a, *material_b;
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/misc/dispatch_buffers_instancing_test/material_a.materialc", (void**) &material_a));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/misc/dispatch_buffers_instancing_test/material_b.materialc", (void**) &material_b));

        ASSERT_NE((void*)0, material_a->m_Material);
        ASSERT_NE((void*)0, material_b->m_Material);

        dmGraphics::HVertexDeclaration vx_decl_a   = dmRender::GetVertexDeclaration(material_a->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
        dmGraphics::HVertexDeclaration vx_decl_b   = dmRender::GetVertexDeclaration(material_b->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
        dmGraphics::HVertexDeclaration inst_decl_a = dmRender::GetVertexDeclaration(material_a->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
        dmGraphics::HVertexDeclaration inst_decl_b = dmRender::GetVertexDeclaration(material_b->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);

        ASSERT_EQ(vertex_stride_a, dmGraphics::GetVertexDeclarationStride(vx_decl_a));
        ASSERT_EQ(vertex_stride_b, dmGraphics::GetVertexDeclarationStride(vx_decl_b));

        ASSERT_EQ(instance_stride_a, dmGraphics::GetVertexDeclarationStride(inst_decl_a));
        ASSERT_EQ(instance_stride_b, dmGraphics::GetVertexDeclarationStride(inst_decl_b));

        // TODO: Ideally we should test the actual result of the dispatch here, but there are
        //       currently limitations in how the content is generated via waf_gamesys.
        //       Right now all rig scenes will be referencing a skeleton, which isn't compatible
        //       with local spaced models.

        dmResource::Release(m_Factory, material_a);
        dmResource::Release(m_Factory, material_b);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

/* Camera */

const char* valid_camera_resources[] = {"/camera/valid.camerac"};
INSTANTIATE_TEST_CASE_P(Camera, ResourceTest, jc_test_values_in(valid_camera_resources));

ResourceFailParams invalid_camera_resources[] =
{
    {"/camera/valid.camerac", "/camera/missing.camerac"},
};
INSTANTIATE_TEST_CASE_P(Camera, ResourceFailTest, jc_test_values_in(invalid_camera_resources));

const char* valid_camera_gos[] = {"/camera/valid_camera.goc"};
INSTANTIATE_TEST_CASE_P(Camera, ComponentTest, jc_test_values_in(valid_camera_gos));

const char* invalid_camera_gos[] = {"/camera/invalid_camera.goc"};
INSTANTIATE_TEST_CASE_P(Camera, ComponentFailTest, jc_test_values_in(invalid_camera_gos));

/* Collection Proxy */

const char* valid_collection_proxy_resources[] = {"/collection_proxy/valid.collectionproxyc"};
INSTANTIATE_TEST_CASE_P(CollectionProxy, ResourceTest, jc_test_values_in(valid_collection_proxy_resources));

const char* valid_collection_proxy_gos[] = {"/collection_proxy/valid_collection_proxy.goc"};
INSTANTIATE_TEST_CASE_P(CollectionProxy, ComponentTest, jc_test_values_in(valid_collection_proxy_gos));

/* Collision Object */

const char* valid_collision_object_resources[] = {"/collision_object/valid.collisionobjectc",
                                                  "/collision_object/valid_tilegrid.collisionobjectc",
                                                  "/collision_object/embedded_shapes.collisionobjectc"};

INSTANTIATE_TEST_CASE_P(CollisionObject, ResourceTest, jc_test_values_in(valid_collision_object_resources));

ResourceFailParams invalid_collision_object_resources[] =
{
    {"/collision_object/valid.collisionobjectc", "/collision_object/missing.collisionobjectc"},
    {"/collision_object/embedded_shapes.collisionobjectc", "/collision_object/invalid_embedded_shapes.collisionobjectc"},
};
INSTANTIATE_TEST_CASE_P(CollisionObject, ResourceFailTest, jc_test_values_in(invalid_collision_object_resources));

const char* valid_collision_object_gos[] = {"/collision_object/valid_collision_object.goc", "/collision_object/valid_tilegrid.goc"};
INSTANTIATE_TEST_CASE_P(CollisionObject, ComponentTest, jc_test_values_in(valid_collision_object_gos));

const char* invalid_collision_object_gos[] =
{
    "/collision_object/invalid_shape.goc"
};
INSTANTIATE_TEST_CASE_P(CollisionObject, ComponentFailTest, jc_test_values_in(invalid_collision_object_gos));

/* Convex Shape */

const char* valid_cs_resources[] =
{
    "/convex_shape/box.convexshapec",
    //"/convex_shape/capsule.convexshapec", // Temporarily disabling capsule since we are more interested in 2D atm
    "/convex_shape/hull.convexshapec",
    "/convex_shape/sphere.convexshapec",
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceTest, jc_test_values_in(valid_cs_resources));

ResourceFailParams invalid_cs_resources[] =
{
    {"/convex_shape/box.convexshapec", "/convex_shape/invalid_box.convexshapec"},
    //{"/convex_shape/capsule.convexshapec", "/convex_shape/invalid_capsule.convexshapec"},
    {"/convex_shape/hull.convexshapec", "/convex_shape/invalid_hull.convexshapec"},
    {"/convex_shape/sphere.convexshapec", "/convex_shape/invalid_sphere.convexshapec"},
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceFailTest, jc_test_values_in(invalid_cs_resources));

/* Font map */

const char* valid_font_resources[] = {"/font/valid_font.fontc"};
INSTANTIATE_TEST_CASE_P(FontMap, ResourceTest, jc_test_values_in(valid_font_resources));

ResourceFailParams invalid_font_resources[] =
{
    {"/font/valid_font.fontc", "/font/missing.fontc"},
    {"/font/valid_font.fontc", "/font/invalid_material.fontc"},
};
INSTANTIATE_TEST_CASE_P(FontMap, ResourceFailTest, jc_test_values_in(invalid_font_resources));


/* Gui Script */

const char* valid_gs_resources[] = {"/gui/valid.gui_scriptc"};
INSTANTIATE_TEST_CASE_P(GuiScript, ResourceTest, jc_test_values_in(valid_gs_resources));

ResourceFailParams invalid_gs_resources[] =
{
    {"/gui/valid.gui_scriptc", "/gui/missing.gui_scriptc"},
    {"/gui/valid.gui_scriptc", "/gui/missing_module.gui_scriptc"},
};
INSTANTIATE_TEST_CASE_P(GuiScript, ResourceFailTest, jc_test_values_in(invalid_gs_resources));

/* Gui */

const char* valid_gui_resources[] = {"/gui/valid.guic"};
INSTANTIATE_TEST_CASE_P(Gui, ResourceTest, jc_test_values_in(valid_gui_resources));

ResourceFailParams invalid_gui_resources[] =
{
    {"/gui/valid.guic", "/gui/missing.guic"},
    {"/gui/valid.guic", "/gui/invalid_font.guic"},
};
INSTANTIATE_TEST_CASE_P(Gui, ResourceFailTest, jc_test_values_in(invalid_gui_resources));

const char* valid_gui_gos[] = {"/gui/valid_gui.goc"};
INSTANTIATE_TEST_CASE_P(Gui, ComponentTest, jc_test_values_in(valid_gui_gos));

const char* invalid_gui_gos[] =
{
    "/gui/invalid_font.goc"
};
INSTANTIATE_TEST_CASE_P(Gui, ComponentFailTest, jc_test_values_in(invalid_gui_gos));

/* Input Binding */

const char* valid_input_resources[] = {"/input/valid.input_bindingc"};
INSTANTIATE_TEST_CASE_P(InputBinding, ResourceTest, jc_test_values_in(valid_input_resources));

ResourceFailParams invalid_input_resources[] =
{
    {"/input/valid.input_bindingc", "/input/missing.input_bindingc"},
};
INSTANTIATE_TEST_CASE_P(InputBinding, ResourceFailTest, jc_test_values_in(invalid_input_resources));

/* Material */

const char* valid_material_resources[] = {"/material/valid.materialc"};
INSTANTIATE_TEST_CASE_P(Material, ResourceTest, jc_test_values_in(valid_material_resources));

ResourceFailParams invalid_material_resources[] =
{
    {"/material/valid.materialc", "/material/missing.materialc"},
    {"/material/valid.materialc", "/material/missing_name.materialc"},
};
INSTANTIATE_TEST_CASE_P(Material, ResourceFailTest, jc_test_values_in(invalid_material_resources));

/* Buffer */

const char* valid_buffer_resources[] = {"/mesh/no_data.bufferc", "/mesh/triangle.bufferc"};
INSTANTIATE_TEST_CASE_P(Buffer, ResourceTest, jc_test_values_in(valid_buffer_resources));

/* Mesh */

const char* valid_mesh_resources[] = {"/mesh/no_data.meshc", "/mesh/triangle.meshc"};
INSTANTIATE_TEST_CASE_P(Mesh, ResourceTest, jc_test_values_in(valid_mesh_resources));

/* MeshSet */

const char* valid_meshset_resources[] = {"/meshset/valid_gltf.meshsetc", "/meshset/valid_gltf.skeletonc", "/meshset/valid_gltf.animationsetc"};
INSTANTIATE_TEST_CASE_P(MeshSet, ResourceTest, jc_test_values_in(valid_meshset_resources));

ResourceFailParams invalid_mesh_resources[] =
{
    {"/meshset/valid_gltf.meshsetc", "/meshset/missing.meshsetc"},
    {"/meshset/valid_gltf.skeletonc", "/meshset/missing.skeletonc"},
    {"/meshset/valid_gltf.animationsetc", "/meshset/missing.animationsetc"},
};
INSTANTIATE_TEST_CASE_P(MeshSet, ResourceFailTest, jc_test_values_in(invalid_mesh_resources));

/* Model */

const char* valid_model_resources[] = {"/model/valid.modelc", "/model/empty_texture.modelc"};
INSTANTIATE_TEST_CASE_P(Model, ResourceTest, jc_test_values_in(valid_model_resources));

ResourceFailParams invalid_model_resources[] =
{
    {"/model/valid.modelc", "/model/missing.modelc"},
    {"/model/valid.modelc", "/model/invalid_material.modelc"},
};
INSTANTIATE_TEST_CASE_P(Model, ResourceFailTest, jc_test_values_in(invalid_model_resources));

const char* valid_model_gos[] = {"/model/valid_model.goc"};
INSTANTIATE_TEST_CASE_P(Model, ComponentTest, jc_test_values_in(valid_model_gos));

const char* invalid_model_gos[] = {"/model/invalid_model.goc", "/model/invalid_material.goc"};
INSTANTIATE_TEST_CASE_P(Model, ComponentFailTest, jc_test_values_in(invalid_model_gos));

/* Animationset */

const char* valid_animationset_resources[] = {"/animationset/valid.animationsetc"};
INSTANTIATE_TEST_CASE_P(AnimationSet, ResourceTest, jc_test_values_in(valid_animationset_resources));

ResourceFailParams invalid_animationset_resources[] =
{
    {"/animationset/valid.animationsetc", "/animationset/missing.animationsetc"},
    {"/animationset/valid.animationsetc", "/animationset/invalid_animationset.animationsetc"},
};
INSTANTIATE_TEST_CASE_P(AnimationSet, ResourceFailTest, jc_test_values_in(invalid_animationset_resources));

/* Particle FX */

const char* valid_particlefx_resources[] = {"/particlefx/valid.particlefxc"};
INSTANTIATE_TEST_CASE_P(ParticleFX, ResourceTest, jc_test_values_in(valid_particlefx_resources));

ResourceFailParams invalid_particlefx_resources[] =
{
    {"/particlefx/valid.particlefxc", "/particlefx/invalid_material.particlefxc"},
};
INSTANTIATE_TEST_CASE_P(ParticleFX, ResourceFailTest, jc_test_values_in(invalid_particlefx_resources));

const char* valid_particlefx_gos[] = {"/particlefx/valid_particlefx.goc"};
INSTANTIATE_TEST_CASE_P(ParticleFX, ComponentTest, jc_test_values_in(valid_particlefx_gos));

const char* invalid_particlefx_gos[] =
{
    "/particlefx/invalid_material.goc",
    "/particlefx/invalid_texture.goc"
};
INSTANTIATE_TEST_CASE_P(ParticleFX, ComponentFailTest, jc_test_values_in(invalid_particlefx_gos));

/* Render */

const char* valid_render_resources[] = {"/render/valid.renderc"};
INSTANTIATE_TEST_CASE_P(Render, ResourceTest, jc_test_values_in(valid_render_resources));

ResourceFailParams invalid_render_resources[] =
{
    {"/render/valid.renderc", "/render/missing.renderc"},
    {"/render/valid.renderc", "/render/invalid_material.renderc"},
};
INSTANTIATE_TEST_CASE_P(Render, ResourceFailTest, jc_test_values_in(invalid_render_resources));

/* Render Script */

const char* valid_rs_resources[] = {"/render_script/valid.render_scriptc"};
INSTANTIATE_TEST_CASE_P(RenderScript, ResourceTest, jc_test_values_in(valid_rs_resources));

ResourceFailParams invalid_rs_resources[] =
{
    {"/render_script/valid.render_scriptc", "/render_script/missing.render_scriptc"},
};
INSTANTIATE_TEST_CASE_P(RenderScript, ResourceFailTest, jc_test_values_in(invalid_rs_resources));

/* Display Profiles */

const char* valid_dp_resources[] = {"/display_profiles/valid.display_profilesc"};
INSTANTIATE_TEST_CASE_P(DisplayProfiles, ResourceTest, jc_test_values_in(valid_dp_resources));

ResourceFailParams invalid_dp_resources[] =
{
    {"/display_profiles/valid.display_profilesc", "/display_profiles/missing.display_profilesc"},
};
INSTANTIATE_TEST_CASE_P(DisplayProfiles, ResourceFailTest, jc_test_values_in(invalid_dp_resources));

/* Data */

const char* valid_data_resources[] = {"/data/valid.datac"};
INSTANTIATE_TEST_CASE_P(Data, ResourceTest, jc_test_values_in(valid_data_resources));

ResourceFailParams invalid_data_resources[] =
{
    {"/data/valid.datac", "/data/missing.datac"},
};
INSTANTIATE_TEST_CASE_P(Data, ResourceFailTest, jc_test_values_in(invalid_data_resources));

/* Light */

const char* valid_light_resources[] = {
    "/light/valid_point.lightc",
    "/light/valid_directional_light.lightc",
    "/light/valid_spot_light.lightc"
};
INSTANTIATE_TEST_CASE_P(Light, ResourceTest, jc_test_values_in(valid_light_resources));

ResourceFailParams invalid_light_resources[] =
{
    {"/light/valid_point.lightc", "/light/invalid_point_missing_range.lightc"},
    {"/light/valid_directional_light.lightc", "/light/invalid_directional_missing_direction.lightc"},
    {"/light/valid_spot_light.lightc", "/light/invalid_spot_missing_outer_cone_angle.lightc"}
};
INSTANTIATE_TEST_CASE_P(Light, ResourceFailTest, jc_test_values_in(invalid_light_resources));

const char* valid_light_gos[] = {
    "/light/valid_point_light.goc",
    "/light/valid_directional_light.goc",
    "/light/valid_spot_light.goc"
};
INSTANTIATE_TEST_CASE_P(Light, ComponentTest, jc_test_values_in(valid_light_gos));

const char* invalid_light_gos[] = {
    "/light/invalid_point_light.goc",
    "/light/invalid_directional_light.goc",
    "/light/invalid_spot_light.goc"
};
INSTANTIATE_TEST_CASE_P(Light, ComponentFailTest, jc_test_values_in(invalid_light_gos));

/* Script */

const char* valid_script_resources[] = {"/script/valid.scriptc"};
INSTANTIATE_TEST_CASE_P(Script, ResourceTest, jc_test_values_in(valid_script_resources));

ResourceFailParams invalid_script_resources[] =
{
    {"/script/valid.scriptc", "/script/missing.scriptc"},
};
INSTANTIATE_TEST_CASE_P(Script, ResourceFailTest, jc_test_values_in(invalid_script_resources));

const char* valid_script_gos[] = {"/script/valid_script.goc"};
INSTANTIATE_TEST_CASE_P(Script, ComponentTest, jc_test_values_in(valid_script_gos));

const char* invalid_script_gos[] = {"/script/missing_script.goc", "/script/invalid_script.goc"};
INSTANTIATE_TEST_CASE_P(Script, ComponentFailTest, jc_test_values_in(invalid_script_gos));

/* Sound */

const char* valid_sound_resources[] = {"/sound/valid.soundc"};
INSTANTIATE_TEST_CASE_P(Sound, ResourceTest, jc_test_values_in(valid_sound_resources));

ResourceFailParams invalid_sound_resources[] =
{
    {"/sound/valid.soundc", "/sound/missing.soundc"},
};
INSTANTIATE_TEST_CASE_P(Sound, ResourceFailTest, jc_test_values_in(invalid_sound_resources));

const char* valid_sound_gos[] = {"/sound/valid_sound.goc"};
INSTANTIATE_TEST_CASE_P(Sound, ComponentTest, jc_test_values_in(valid_sound_gos));

const char* invalid_sound_gos[] = {"/sound/invalid_sound.goc", "/sound/invalid_sound.goc"};
INSTANTIATE_TEST_CASE_P(Sound, ComponentFailTest, jc_test_values_in(invalid_sound_gos));

/* Factory */

const char* valid_sp_resources[] = {"/factory/valid.factoryc"};
INSTANTIATE_TEST_CASE_P(Factory, ResourceTest, jc_test_values_in(valid_sp_resources));

ResourceFailParams invalid_sp_resources[] =
{
    {"/factory/valid.factoryc", "/factory/missing.factoryc"},
};
INSTANTIATE_TEST_CASE_P(Factory, ResourceFailTest, jc_test_values_in(invalid_sp_resources));

const char* valid_sp_gos[] = {"/factory/valid_factory.goc"};
INSTANTIATE_TEST_CASE_P(Factory, ComponentTest, jc_test_values_in(valid_sp_gos));

const char* invalid_sp_gos[] = {"/factory/invalid_factory.goc"};
INSTANTIATE_TEST_CASE_P(Factory, ComponentFailTest, jc_test_values_in(invalid_sp_gos));


/* Collection Factory */

const char* valid_cf_resources[] = {"/collection_factory/valid.collectionfactoryc"};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ResourceTest, jc_test_values_in(valid_cf_resources));

ResourceFailParams invalid_cf_resources[] =
{
    {"/collection_factory/valid.collectionfactoryc", "/collection_factory/missing.collectionfactoryc"},
};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ResourceFailTest, jc_test_values_in(invalid_cf_resources));

const char* valid_cf_gos[] = {"/collection_factory/valid_collectionfactory.goc"};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ComponentTest, jc_test_values_in(valid_cf_gos));

const char* invalid_cf_gos[] = {"/collection_factory/invalid_collectionfactory.goc"};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ComponentFailTest, jc_test_values_in(invalid_cf_gos));


/* Sprite */

const char* valid_sprite_resources[] = {"/sprite/valid.spritec"};
INSTANTIATE_TEST_CASE_P(Sprite, ResourceTest, jc_test_values_in(valid_sprite_resources));

ResourceFailParams invalid_sprite_resources[] =
{
    {"/sprite/valid.spritec", "/sprite/invalid_animation.spritec"},
};
INSTANTIATE_TEST_CASE_P(Sprite, ResourceFailTest, jc_test_values_in(invalid_sprite_resources));

const char* valid_sprite_gos[] = {"/sprite/valid_sprite.goc"};
INSTANTIATE_TEST_CASE_P(Sprite, ComponentTest, jc_test_values_in(valid_sprite_gos));

const char* invalid_sprite_gos[] = {"/sprite/invalid_sprite.goc"};
INSTANTIATE_TEST_CASE_P(Sprite, ComponentFailTest, jc_test_values_in(invalid_sprite_gos));

/* TileSet */
const char* valid_tileset_resources[] = {"/tile/valid.t.texturesetc"};
INSTANTIATE_TEST_CASE_P(TileSet, ResourceTest, jc_test_values_in(valid_tileset_resources));

/* TileGrid */
const char* valid_tilegrid_resources[] = {"/tile/valid.tilemapc"};
INSTANTIATE_TEST_CASE_P(TileGrid, ResourceTest, jc_test_values_in(valid_tilegrid_resources));

const char* valid_tileset_gos[] = {"/tile/valid_tilegrid.goc", "/tile/valid_tilegrid_collisionobject.goc"};
INSTANTIATE_TEST_CASE_P(TileSet, ComponentTest, jc_test_values_in(valid_tileset_gos));

/* Texture */

const char* valid_texture_resources[] = {"/texture/valid_png.texturec", "/texture/blank_4096_png.texturec"};
INSTANTIATE_TEST_CASE_P(Texture, ResourceTest, jc_test_values_in(valid_texture_resources));

ResourceFailParams invalid_texture_resources[] =
{
    {"/texture/valid_png.texturec", "/texture/missing.texturec"},
};
INSTANTIATE_TEST_CASE_P(Texture, ResourceFailTest, jc_test_values_in(invalid_texture_resources));

/* Label */

void AssertPointEquals(const Vector4& p, float x, float y)
{
    EXPECT_NEAR(p.getX(), x, EPSILON);
    EXPECT_NEAR(p.getY(), y, EPSILON);
}

TEST_F(LabelTest, LabelMovesWhenSwitchingPivot)
{
    // pivot = center
    Matrix4 mat = dmGameSystem::CompLabelLocalTransform(m_Position, Quat::identity(), m_Scale, m_Size, 0);

    AssertPointEquals(mat * m_BottomLeft, -1.0, -1.0);
    AssertPointEquals(mat * m_TopLeft, -1.0, 1.0);
    AssertPointEquals(mat * m_TopRight, 1.0, 1.0);
    AssertPointEquals(mat * m_BottomRight, 1.0, -1.0);

    // pivot = north east
    mat = dmGameSystem::CompLabelLocalTransform(m_Position, Quat::identity(), m_Scale, m_Size, 2);

    AssertPointEquals(mat * m_BottomLeft, -2.0, -2.0);
    AssertPointEquals(mat * m_TopLeft, -2.0, 0.0);
    AssertPointEquals(mat * m_TopRight, 0.0, 0.0);
    AssertPointEquals(mat * m_BottomRight, 0.0, -2.0);

    // pivot = west
    mat = dmGameSystem::CompLabelLocalTransform(m_Position, Quat::identity(), m_Scale, m_Size, 7);

    AssertPointEquals(mat * m_BottomLeft, 0.0, -1.0);
    AssertPointEquals(mat * m_TopLeft, 0.0, 1.0);
    AssertPointEquals(mat * m_TopRight, 2.0, 1.0);
    AssertPointEquals(mat * m_BottomRight, 2.0, -1.0);
}

TEST_F(LabelTest, LabelMovesWhenChangingPosition) {
    // pivot = center
    Matrix4 mat = dmGameSystem::CompLabelLocalTransform(Point3(1.0, 1.0, 1.0), Quat::identity(), m_Scale, m_Size, 0);

    AssertPointEquals(mat * m_BottomLeft, 0.0, 0.0);
    AssertPointEquals(mat * m_TopLeft, 0.0, 2.0);
    AssertPointEquals(mat * m_TopRight, 2.0, 2.0);
    AssertPointEquals(mat * m_BottomRight, 2.0, 0.0);
}

TEST_F(LabelTest, LabelRotatesAroundPivot) {
    // pivot = center, rotation = -180
    Matrix4 mat = dmGameSystem::CompLabelLocalTransform(Point3(1.0, 1.0, 1.0), m_Rotation, m_Scale, m_Size, 0);

    AssertPointEquals(mat * m_BottomLeft, 2.0, 2.0);
    AssertPointEquals(mat * m_TopLeft, 2.0, 0.0);
    AssertPointEquals(mat * m_TopRight, 0.0, 0.0);
    AssertPointEquals(mat * m_BottomRight, 0.0, 2.0);

    // pivot = north west, rotation = -180
    mat = dmGameSystem::CompLabelLocalTransform(Point3(-1.0, -2.0, 0.0), m_Rotation, m_Scale, m_Size, 8);

    AssertPointEquals(mat * m_BottomLeft, -1.0, 0.0);
    AssertPointEquals(mat * m_TopLeft, -1.0, -2.0);
    AssertPointEquals(mat * m_TopRight, -3.0, -2.0);
    AssertPointEquals(mat * m_BottomRight, -3.0, 0.0);
}

const char* valid_label_resources[] = {"/label/valid.labelc"};
INSTANTIATE_TEST_CASE_P(Label, ResourceTest, jc_test_values_in(valid_label_resources));

const char* valid_label_gos[] = {"/label/valid_label.goc"};
INSTANTIATE_TEST_CASE_P(Label, ComponentTest, jc_test_values_in(valid_label_gos));

const char* invalid_label_gos[] = {"/label/invalid_label.goc"};
INSTANTIATE_TEST_CASE_P(Label, ComponentFailTest, jc_test_values_in(invalid_label_gos));

/* Test material vertex space component compatibility */

const char* invalid_vertexspace_resources[] =
{
    "/sprite/invalid_vertexspace.spritec",
    "/tile/invalid_vertexspace.tilegridc",
    "/particlefx/invalid_vertexspace.particlefxc",
    "/gui/invalid_vertexspace.guic",
    "/label/invalid_vertexspace.labelc",
};
INSTANTIATE_TEST_CASE_P(InvalidVertexSpace, InvalidVertexSpaceTest, jc_test_values_in(invalid_vertexspace_resources));

/* Get and set resource properties on supported components */

ResourcePropParams res_prop_params[] =
{
    // property, val, val-not-found, val-invalid-ext, component0, ..., componentN
    {"material", "/resource/resource_alt.materialc", "/not_found.materialc", "/resource/res_getset_prop.goc", "label", "model", "gui", "tilemap", 0},
    {"font", "/resource/font.fontc", "/not_found.fontc", "/resource/res_getset_prop.goc", "label", 0},
    {"image", "/tile/valid2.t.texturesetc", "", "/resource/res_getset_prop.goc", "sprite", 0},
    {"texture0", "/tile/mario_tileset.texturec", "/not_found.texturec", "/resource/res_getset_prop.goc", "model", 0},
    {"texture1", "/tile/mario_tileset.texturec", "/not_found.texturec", "/resource/res_getset_prop.goc", "model", 0},
    {"tile_source", "/tile/valid2.t.texturesetc", "", "/resource/res_getset_prop.goc", "tilemap", 0},
};

INSTANTIATE_TEST_CASE_P(ResourceProperty, ResourcePropTest, jc_test_values_in(res_prop_params));

/* Validate default and dynamic gameobject factories */

FactoryTestParams factory_testparams [] =
{
    {"/factory/dynamic_factory_test.goc", 0, true, true},
    {"/factory/dynamic_factory_test.goc", 0, true, false},
    {"/factory/factory_test.goc", 0, false, true},
    {"/factory/factory_test.goc", 0, false, false},
    {"/factory/dynamic_factory_test.goc", "/factory/empty.goc", true, true},
    {"/factory/dynamic_factory_test.goc", "/factory/empty.goc", true, false},
    {"/factory/factory_test.goc", "/factory/empty.goc", false, true},
    {"/factory/factory_test.goc", "/factory/empty.goc", false, false},
    {"/factory/dynamic_factory_test.goc", "/factory/dynamic_prototype_sprite.goc", true, true},
    {"/factory/dynamic_factory_test.goc", "/factory/dynamic_prototype_sprite.goc", true, false},
    {"/factory/factory_test.goc", "/factory/dynamic_prototype_sprite.goc", false, true},
    {"/factory/factory_test.goc", "/factory/dynamic_prototype_sprite.goc", false, false},
};
INSTANTIATE_TEST_CASE_P(Factory, FactoryTest, jc_test_values_in(factory_testparams));

FactoryTestParams factory_recursive_testparams [] =
{
    {"/factory/dynamic_factory_test.goc", 0, true, true},
    {"/factory/dynamic_factory_test.goc", 0, true, false},
    {"/factory/factory_test.goc", 0, false, true},
    {"/factory/factory_test.goc", 0, false, false},
};
INSTANTIATE_TEST_CASE_P(FactoryRecursivePrototype, FactoryRecursivePrototypeTest, jc_test_values_in(factory_recursive_testparams));

/* Validate default and dynamic collection factories */

CollectionFactoryTestParams collection_factory_testparams [] =
{
    {"/collection_factory/dynamic_collectionfactory_test.goc", 0, true, true},
    {"/collection_factory/dynamic_collectionfactory_test.goc", 0, true, false},
    {"/collection_factory/collectionfactory_test.goc", 0, false, true},
    {"/collection_factory/collectionfactory_test.goc", 0, false, false},
    {"/collection_factory/dynamic_collectionfactory_test.goc", "/collection_factory/dynamic_prototype.collectionc", true, true},
    {"/collection_factory/dynamic_collectionfactory_test.goc", "/collection_factory/dynamic_prototype.collectionc", true, false},
    {"/collection_factory/collectionfactory_test.goc", "/collection_factory/dynamic_prototype.collectionc", false, true},
    {"/collection_factory/collectionfactory_test.goc", "/collection_factory/dynamic_prototype.collectionc", false, false},
};
INSTANTIATE_TEST_CASE_P(CollectionFactory, CollectionFactoryTest, jc_test_values_in(collection_factory_testparams));

CollectionFactoryTestParams collection_factory_recursive_testparams [] =
{
    {"/collection_factory/dynamic_collectionfactory_test.goc", 0, true, true},
    {"/collection_factory/dynamic_collectionfactory_test.goc", 0, true, false},
    {"/collection_factory/collectionfactory_test.goc", 0, false, true},
    {"/collection_factory/collectionfactory_test.goc", 0, false, false},
};
INSTANTIATE_TEST_CASE_P(CollectionFactoryRecursivePrototype, CollectionFactoryRecursivePrototypeTest, jc_test_values_in(collection_factory_recursive_testparams));

/* Validate draw count for different GOs */

DrawCountParams draw_count_params[] =
{
    {"/gui/draw_count_test.goc", 1},
    {"/gui/draw_count_test2.goc", 1},
    {"/gui/draw_count_empty_text_test.goc", 0},
};
INSTANTIATE_TEST_CASE_P(DrawCount, DrawCountTest, jc_test_values_in(draw_count_params));


// Spawn and run the script attached to these game objects

ScriptComponentTestParams script_component_test_params[] =
{
    // file,                              comp type,            comp name
    {"/camera/test_comp.goc",             "camerac",            "camera"},
    {"/factory/test_comp.goc",            "factoryc",           "factory"},
    {"/label/test_comp.goc",              "labelc",             "label"},
    {"/light/test_comp.goc",              "lightc",             "light"},
    {"/mesh/test_comp.goc",               "meshc",              "mesh"},
    {"/model/test_comp.goc",              "modelc",             "model"},
    {"/particlefx/test_comp.goc",         "particlefxc",        "particlefx"},
    {"/sound/test_comp.goc",              "soundc",             "sound"},
    {"/sprite/test_comp.goc",             "spritec",            "sprite"},
    {"/tilegrid/test_comp.goc",           "tilemapc",           "tilemap"},
    {"/collision_object/test_comp.goc",   "collisionobjectc",   "collisionobject"},
    {"/collection_proxy/test_comp.goc",   "collectionproxyc",   "collectionproxy"},
    {"/collection_factory/test_comp.goc", "collectionfactoryc", "collectionfactory"},
};

INSTANTIATE_TEST_CASE_P(ScriptComponent, ScriptComponentTest, jc_test_values_in(script_component_test_params));


/* Validate gui box rendering for different GOs. */

BoxRenderParams box_render_params[] =
{
    // 9-slice params: on | Use geometries: 8 | Flip uv: off | Texture: tilesource animation
    {
        "/gui/render_box_test1.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -16.000000, 0.0, 0.0), 0.031250, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -14.000000, 0.0, 0.0), 0.031250, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -14.000000, 0.0, 0.0), 0.000000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, -16.000000, 0.0, 0.0), 0.468750, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, -14.000000, 0.0, 0.0), 0.468750, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -14.000000, 0.0, 0.0), 0.500000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 14.000000, 0.0, 0.0), 0.031250, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 14.000000, 0.0, 0.0), 0.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, 14.000000, 0.0, 0.0), 0.468750, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 14.000000, 0.0, 0.0), 0.500000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 16.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, 16.000000, 0.0, 0.0), 0.468750, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        54,
        {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2, 4, 6, 7, 4, 7, 5, 3, 2, 8, 3, 8, 9, 2, 5, 10, 2, 10, 8, 5, 7, 11, 5, 11, 10, 9, 8, 12, 9, 12, 13, 8, 10, 14, 8, 14, 12, 10, 11, 15, 10, 15, 14}
    },
    // 9-slice params: off | Use geometries: 8 | Flip uv: off | Texture: tilesource animation
    {
        "/gui/render_box_test2.goc",
        {
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        18,
        {0, 1, 2, 0, 2, 2, 0, 2, 3, 0, 3, 3, 0, 3, 3, 0, 3, 3}
    },
    // 9-slice params: off | Use geometries: off | Flip uv: off | Texture: tilesource animation
    {
        "/gui/render_box_test3.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        6,
        {0, 1, 2, 0, 2, 3}
    },
    // 9-slice params: off | Use geometries: off | Flip uv: u | Texture: tilesource animation
    {
        "/gui/render_box_test4.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        6,
        {0, 1, 2, 0, 2, 3}
    },
    // 9-slice params: off | Use geometries: off | Flip uv: uv | Texture: tilesource animation
    {
        "/gui/render_box_test5.goc",
        {
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        6,
        {0, 1, 2, 0, 2, 3}
    },
    // 9-slice params: on | Use geometries: 8 | Flip uv: uv | Texture: tilesource animation
    {
        "/gui/render_box_test6.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -16.000000, 0.0, 0.0), 0.468750, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -14.000000, 0.0, 0.0), 0.468750, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -14.000000, 0.0, 0.0), 0.500000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, -16.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, -14.000000, 0.0, 0.0), 0.031250, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -14.000000, 0.0, 0.0), 0.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 14.000000, 0.0, 0.0), 0.468750, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 14.000000, 0.0, 0.0), 0.500000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, 14.000000, 0.0, 0.0), 0.031250, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 14.000000, 0.0, 0.0), 0.000000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 16.000000, 0.0, 0.0), 0.468750, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, 16.000000, 0.0, 0.0), 0.031250, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        54,
        {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2, 4, 6, 7, 4, 7, 5, 3, 2, 8, 3, 8, 9, 2, 5, 10, 2, 10, 8, 5, 7, 11, 5, 11, 10, 9, 8, 12, 9, 12, 13, 8, 10, 14, 8, 14, 12, 10, 11, 15, 10, 15, 14}
    },
    // 9-slice params: on | Use geometries: na | Flip uv: na | Texture: none
    {
        "/gui/render_box_test7.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-1.000000, -1.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(1.000000, -1.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-1.000000, 1.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(1.000000, 1.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        6,
        {0, 1, 3, 0, 3, 2}
    },
    // 9-slice params: off | Use geometries: na | Flip uv: na | Texture: script
    {
        "/gui/render_box_test8.goc",
        {
            dmGameSystem::BoxVertex(Vector4(68.000000, 68.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(132.000000, 68.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(68.000000, 132.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(132.000000, 132.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        6,
        {0, 1, 3, 0, 3, 2}
    },
    // 9-slice params: on | Use geometries: na | Flip uv: na | Texture: script
    {
        "/gui/render_box_test9.goc",
        {
            dmGameSystem::BoxVertex(Vector4(68.000000, 68.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(70.000000, 68.000000, 0.0, 0.0), 0.031250, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(70.000000, 70.000000, 0.0, 0.0), 0.031250, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(68.000000, 70.000000, 0.0, 0.0), 0.000000, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(130.000000, 68.000000, 0.0, 0.0), 0.968750, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(130.000000, 70.000000, 0.0, 0.0), 0.968750, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(132.000000, 68.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(132.000000, 70.000000, 0.0, 0.0), 1.000000, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(70.000000, 130.000000, 0.0, 0.0), 0.031250, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(68.000000, 130.000000, 0.0, 0.0), 0.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(130.000000, 130.000000, 0.0, 0.0), 0.968750, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(132.000000, 130.000000, 0.0, 0.0), 1.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(70.000000, 132.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(68.000000, 132.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(130.000000, 132.000000, 0.0, 0.0), 0.968750, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(132.000000, 132.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        54,
        {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2, 4, 6, 7, 4, 7, 5, 3, 2, 8, 3, 8, 9, 2, 5, 10, 2, 10, 8, 5, 7, 11, 5, 11, 10, 9, 8, 12, 9, 12, 13, 8, 10, 14, 8, 14, 12, 10, 11, 15, 10, 15, 14}
    },
    // 9-slice params: off | Use geometries: na | Flip uv: na | Texture: none
    {
        "/gui/render_box_test10.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-1.000000, -1.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(1.000000, -1.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-1.000000, 1.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(1.000000, 1.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0)
        },
        6,
        {0, 1, 3, 0, 3, 2}
    },
    // 9-slice params: off | Use geometries: 4 | Flip uv: na | Texture: paged atlas (64x64 texture)
    {
        "/gui/render_box_test11.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-32.000000, -32.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 1),
            dmGameSystem::BoxVertex(Vector4(-32.000000, 32.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0),  1),
            dmGameSystem::BoxVertex(Vector4(32.000000, 32.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0),   1),
            dmGameSystem::BoxVertex(Vector4(32.000000, -32.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0),  1)
        },
        6,
        {0, 1, 2, 0, 2, 3}
    }
};
INSTANTIATE_TEST_CASE_P(BoxRender, BoxRenderTest, jc_test_values_in(box_render_params));

/* Sprite cursor property */
#define F1T3 1.0f/3.0f
#define F2T3 2.0f/3.0f
const CursorTestParams cursor_properties[] = {

    // Forward & backward
    {"anim_once",       0.0f, 1.0f, {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}, 5},
    {"anim_once",      -1.0f, 1.0f, {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}, 5}, // Same as above, but cursor should be clamped
    {"anim_once",       1.0f, 1.0f, {1.0f, 1.0f}, 2},                     // Again, clamped, but will also be at end of anim.
    {"anim_once_back",  0.0f, 1.0f, {1.0f, 0.75f, 0.5f, 0.25f, 0.0f}, 5},
    {"anim_loop",       0.0f, 1.0f, {0.0f, 0.25f, 0.5f, 0.75f, 0.0f, 0.25f, 0.5f, 0.75f}, 8},
    {"anim_loop_back",  0.0f, 1.0f, {1.0f, 0.75f, 0.5f, 0.25f, 1.0f, 0.75f, 0.5f, 0.25f}, 8},

    // Ping-pong goes up to the "early end" and skip duplicate of "last" frame, this equals:
    // duration = orig_frame_count*2 - 2
    // In our test animation this equals; 4*2-2 = 6
    // However, the cursor will go from 0 -> 1 and back again during the whole ping pong animation.
    // This means the cursor will go in these steps: 0/3 -> 1/3 -> 2/3 -> 3/3 -> 2/3 -> 1/3
    {"anim_once_pingpong", 0.0f, 1.0f, {0.0f, F1T3, F2T3, 1.0f, F2T3, F1T3, 0.0f, 0.0f}, 8},
    {"anim_loop_pingpong", 0.0f, 1.0f, {0.0f, F1T3, F2T3, 1.0f, F2T3, F1T3, 0.0f, F1T3}, 8},

    // Cursor start
    {"anim_once",          0.5f, 1.0f, {0.5f, 0.75f, 1.0f, 1.0f}, 4},
    {"anim_once_back",     0.5f, 1.0f, {0.5f, 0.25f, 0.0f, 0.0f}, 4},
    {"anim_loop",          0.5f, 1.0f, {0.5f, 0.75f, 0.0f, 0.25f, 0.5f, 0.75f, 0.0f}, 7},
    {"anim_loop_back",     0.5f, 1.0f, {0.5f, 0.25f, 1.0f, 0.75f, 0.5f, 0.25f, 1.0f}, 7},
    {"anim_once_pingpong", F1T3, 1.0f, {F1T3, F2T3, 1.0f, F2T3, F1T3, 0.0f, 0.0f}, 7},
    {"anim_loop_pingpong", F1T3, 1.0f, {F1T3, F2T3, 1.0f, F2T3, F1T3, 0.0f, F1T3}, 7},

    // Playback rate, x2 speed
    {"anim_once",          0.0f, 2.0f, {0.0f, 0.5f, 1.0f, 1.0f}, 4},
    {"anim_once_back",     0.0f, 2.0f, {1.0f, 0.5f, 0.0f, 0.0f}, 4},
    {"anim_loop",          0.0f, 2.0f, {0.0f, 0.5f, 0.0f, 0.5f, 0.0f}, 5},
    {"anim_loop_back",     0.0f, 2.0f, {1.0f, 0.5f, 1.0f, 0.5f, 1.0f}, 5},
    {"anim_once_pingpong", 0.0f, 2.0f, {0.0f, F2T3, F2T3, 0.0f, 0.0f}, 5},
    {"anim_loop_pingpong", 0.0f, 2.0f, {0.0f, F2T3, F2T3, 0.0f, F2T3, F2T3, 0.0f}, 7},

    // Playback rate, x0 speed
    {"anim_once",          0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, 3},
    {"anim_once_back",     0.0f, 0.0f, {1.0f, 1.0f, 1.0f}, 3},
    {"anim_loop",          0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, 3},
    {"anim_loop_back",     0.0f, 0.0f, {1.0f, 1.0f, 1.0f}, 3},
    {"anim_once_pingpong", 0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, 3},
    {"anim_loop_pingpong", 0.0f, 0.0f, {0.0f, 0.0f, 0.0f}, 3},

    // Playback rate, -x2 speed
    {"anim_once",          0.0f, -2.0f, {0.0f, 0.0f, 0.0f}, 3},
    {"anim_once_back",     0.0f, -2.0f, {1.0f, 1.0f, 1.0f}, 3},
    {"anim_loop",          0.0f, -2.0f, {0.0f, 0.0f, 0.0f}, 3},
    {"anim_loop_back",     0.0f, -2.0f, {1.0f, 1.0f, 1.0f}, 3},
    {"anim_once_pingpong", 0.0f, -2.0f, {0.0f, 0.0f, 0.0f}, 3},
    {"anim_loop_pingpong", 0.0f, -2.0f, {0.0f, 0.0f, 0.0f}, 3},

};
INSTANTIATE_TEST_CASE_P(Cursor, CursorTest, jc_test_values_in(cursor_properties));
#undef F1T3
#undef F2T3

bool RunFile(lua_State* L, const char* filename)
{
    char path[1024];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s", filename);

    dmLuaDDF::LuaModule* ddf = 0;
    dmDDF::Result res = dmDDF::LoadMessageFromFile(path, dmLuaDDF::LuaModule::m_DDFDescriptor, (void**) &ddf);
    if (res != dmDDF::RESULT_OK)
        return false;

    char* buffer = (char*) malloc(ddf->m_Source.m_Script.m_Count + 1);
    memcpy((void*) buffer, ddf->m_Source.m_Script.m_Data, ddf->m_Source.m_Script.m_Count);
    buffer[ddf->m_Source.m_Script.m_Count] = '\0';

    int ret = luaL_dostring(L, buffer);
    free(buffer);
    dmDDF::FreeMessage(ddf);

    if (ret != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

TEST_F(ScriptImageTest, TestImage)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "image/test_image.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_images");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));

    char hostfs[64] = {};
    if (strlen(DM_HOSTFS) != 0)
        dmSnPrintf(hostfs, sizeof(hostfs), "%s/", DM_HOSTFS);
    lua_pushstring(L, hostfs);
    int result = dmScript::PCall(L, 1, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptImageTest, TestImageBuffer)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/image/test_image_buffer.goc", dmHashString64("/test_image"));
    ASSERT_NE((void*)0, go);

    if (DM_HOSTFS)
    {
        char run_str[128];
        dmSnPrintf(run_str, sizeof(run_str), "set_host_fs(%s)", DM_HOSTFS);
        ASSERT_TRUE(RunString(L, run_str));
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, PushCheckBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    dmScript::LuaHBuffer* buffer_ptr = dmScript::CheckBuffer(L, -1);
    ASSERT_NE((void*)0x0, buffer_ptr);
    ASSERT_EQ(m_Buffer, buffer_ptr->m_Buffer);

    dmScript::LuaHBuffer* buffer_ptr2 = dmScript::CheckBufferNoError(L, -1);
    ASSERT_NE((void*)0x0, buffer_ptr2);
    ASSERT_EQ(m_Buffer, buffer_ptr2->m_Buffer);

    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, PushCheckUnpackBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);

    dmBuffer::HBuffer buf = dmScript::CheckBufferUnpack(L, -1);
    ASSERT_NE((dmBuffer::HBuffer)0x0, buf);
    ASSERT_EQ(m_Buffer, buf);

    dmBuffer::HBuffer buf2 = dmScript::CheckBufferUnpackNoError(L, -1);
    ASSERT_NE((dmBuffer::HBuffer)0x0, buf2);
    ASSERT_EQ(m_Buffer, buf2);

    lua_pop(L, 1);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, IsBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_pushstring(L, "not_a_buffer");
    lua_pushnumber(L, 1337);
    ASSERT_FALSE(dmScript::IsBuffer(L, -1));
    ASSERT_FALSE(dmScript::IsBuffer(L, -2));
    ASSERT_TRUE(dmScript::IsBuffer(L, -3));
    lua_pop(L, 3);
    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, PrintBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "print(test_buffer)"));

    ASSERT_TRUE(RunString(L, "local stream = buffer.get_stream(test_buffer, \"rgb\"); print(stream)"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, GetCount)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "assert(256 == #test_buffer)"));

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CreateBuffer)
{
    int top = lua_gettop(L);

    dmScript::LuaHBuffer* buffer = 0;
    uint32_t element_count = 0;

    dmBuffer::ValueType type;
    uint32_t typecount;

    RunString(L, "test_buffer = buffer.create( 12, { {name=hash(\"rgba\"), type=buffer.VALUE_TYPE_UINT8, count=4 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetCount(buffer->m_Buffer, &element_count));
    ASSERT_EQ(12, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("rgba"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_UINT8, type);
    ASSERT_EQ(4, typecount);
    lua_pop(L, 1);

    RunString(L, "test_buffer = buffer.create( 24, { {name=hash(\"position\"), type=buffer.VALUE_TYPE_FLOAT32, count=3 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetCount(buffer->m_Buffer, &element_count));
    ASSERT_EQ(24, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("position"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);
    lua_pop(L, 1);

    RunString(L, "test_buffer = buffer.create( 10, { {name=hash(\"position\"), type=buffer.VALUE_TYPE_UINT16, count=4 }, \
                                                    {name=hash(\"normal\"), type=buffer.VALUE_TYPE_FLOAT32, count=3 } } )");
    lua_getglobal(L, "test_buffer");
    buffer = dmScript::CheckBuffer(L, -1);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetCount(buffer->m_Buffer, &element_count));
    ASSERT_EQ(10, element_count);
    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("position"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_UINT16, type);
    ASSERT_EQ(4, typecount);

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::GetStreamType(buffer->m_Buffer, dmHashString64("normal"), &type, &typecount ) );
    ASSERT_EQ(dmBuffer::VALUE_TYPE_FLOAT32, type);
    ASSERT_EQ(3, typecount);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, GetBytes)
{
    int top = lua_gettop(L);
    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(m_Buffer, (void**)&data, &datasize);

    for( uint32_t i = 0; i < datasize; ++i )
    {
        data[i] = i+1;
    }

    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    char str[1024];
    dmSnPrintf(str, sizeof(str), " local bytes = buffer.get_bytes(test_buffer, \"rgb\") \
                                    assert(#bytes == %u) \
                                    for i=1,#bytes do \
                                        assert( (i %% 256) == string.byte(bytes, i) )\
                                    end \
                                  ", datasize);
    bool run = RunString(L, str);
    ASSERT_TRUE(run);

    ASSERT_EQ(top, lua_gettop(L));
}


template<typename T>
static void memset_stream(T* data, uint32_t count, uint32_t components, uint32_t stride, T value)
{
    for(uint32_t i = 0; i < count; ++i) {
        for(uint32_t c = 0; c < components; ++c) {
            data[c] = value;
        }
        data += stride;
    }
}


TEST_F(ScriptBufferTest, Indexing)
{
    int top = lua_gettop(L);

    dmBuffer::Result r;
    uint16_t* stream_rgb = 0;
    uint32_t count_rgb = 0;
    uint32_t components_rgb = 0;
    uint32_t stride_rgb = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("rgb"), (void**)&stream_rgb, &count_rgb, &components_rgb, &stride_rgb);
    uint32_t size_rgb = count_rgb * components_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(uint16_t) * 3u, size_rgb);

    float* stream_a = 0;
    uint32_t count_a = 0;
    uint32_t components_a = 0;
    uint32_t stride_a = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("a"), (void**)&stream_a, &count_a, &components_a, &stride_a);
    uint32_t size_a = count_a * components_a * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_FLOAT32);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(float) * 1u, size_a);

    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    // Set full buffer (uint16)
    memset_stream(stream_rgb, count_rgb, components_rgb, stride_rgb, (uint16_t)0);

    RunString(L, "local stream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                  for i=1,#stream do \
                    stream[i] = i*3 + i \
                  end \
                  ");

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

    uint16_t* rgb = stream_rgb;
    for( uint32_t i = 0, x = 1; i < count_rgb; ++i )
    {
        for( uint32_t c = 0; c < components_rgb; ++c, ++x )
        {
            ASSERT_EQ(x*3 + x, (uint32_t)rgb[c]);
        }
        rgb += stride_rgb;
    }

    // Set full buffer (float)
    memset_stream(stream_a, count_a, components_a, stride_a, 0.0f);

    RunString(L, "local stream = buffer.get_stream(test_buffer, hash(\"a\")) \
                  for i=1,#stream do \
                    stream[i] = i*5 + 0.5 \
                  end \
                  ");

    ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

    float* a = stream_a;
    for( uint32_t i = 0, x = 1; i < count_a; ++i )
    {
        for( uint32_t c = 0; c < components_a; ++c, ++x )
        {
            ASSERT_EQ(x*5.0f + 0.5f, a[c]);
        }
        a += stride_a;
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptBufferTest, CopyStream)
{
    int top = lua_gettop(L);

    dmBuffer::Result r;
    uint16_t* stream_rgb = 0;
    uint32_t count_rgb = 0;
    uint32_t components_rgb = 0;
    uint32_t stride_rgb = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("rgb"), (void**)&stream_rgb, &count_rgb, &components_rgb, &stride_rgb);
    uint32_t size_rgb = count_rgb * components_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(uint16_t) * 3u, size_rgb);
    ASSERT_EQ(m_Count, count_rgb);

    float* stream_a = 0;
    uint32_t count_a = 0;
    uint32_t components_a = 0;
    uint32_t stride_a = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("a"), (void**)&stream_a, &count_a, &components_a, &stride_a);
    uint32_t size_a = count_a * components_a * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_FLOAT32);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(m_Count * sizeof(float) * 1u, size_a);
    ASSERT_EQ(m_Count, count_a);


    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");


    // Copy one stream to another
    {
        memset_stream(stream_a, count_a, components_a, stride_a, 0.0f);

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i*4 \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        float* a = stream_a;
        for(uint32_t i = 1, x = 1; i <= count_a; ++i)
        {
            for(uint32_t c = 0; c < components_a; ++c, ++x)
            {
                ASSERT_EQ(x*4, a[c]);
            }
            a += stride_a;
        }
    }

    // Copy first half of source to latter half of dest
    {
        memset_stream(stream_rgb, count_rgb, components_rgb, stride_rgb, (uint16_t)0);

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_UINT16, count=3 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i*3 \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                        buffer.copy_stream(dststream, #srcstream / 2, srcstream, 0, #srcstream / 2) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        uint16_t* rgb = stream_rgb;
        uint32_t halfcount = (count_rgb * components_rgb) / 2;
        for(uint32_t i = 1, x = 1; i <= count_rgb; ++i)
        {
            for(uint32_t c = 0; c < components_rgb; ++c, ++x)
            {
                if (x < halfcount)
                    ASSERT_EQ(0, rgb[c]);
                else
                    ASSERT_EQ((x - halfcount) * 3, rgb[c]);
            }
            rgb += stride_rgb;
        }
    }

    // Copy some elements offset from aligned boundaries, and a non multiple of components
    {
        memset_stream(stream_rgb, count_rgb, components_rgb, stride_rgb, (uint16_t)0);

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_UINT16, count=3 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        for i=1,#srcstream do \
                            srcstream[i] = i \
                        end \
                        local dststream = buffer.get_stream(test_buffer, hash(\"rgb\")) \
                        buffer.copy_stream(dststream, 4, srcstream, 8, 10) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        uint16_t* rgb = stream_rgb;
        for(uint32_t i = 0, x = 0; i <= 20; ++i)
        {
            for(uint32_t c = 0; c < components_rgb; ++c, ++x)
            {
                if (x < 4 || x >= 14)
                    ASSERT_EQ(0, rgb[c]);
                else
                    ASSERT_EQ(9 + (x-4), rgb[c]);
            }
            rgb += stride_rgb;
        }
    }


    dmLogWarning("Expected error outputs ->");

    // Too large count
    {
        memset_stream(stream_a, count_a, components_a, stride_a, 42.0f); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 0, #srcstream + 10) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));
        float* a = (float*)stream_a;
        for(uint32_t i = 1; i <= count_a; ++i)
        {
            for(uint32_t c = 0; c < components_a; ++c)
            {
                ASSERT_EQ(42.0f, a[c]);
            }
            a += stride_a;
        }
        lua_pop(L, 1);
    }

    // Buffer overrun (write)
    {
        memset_stream(stream_a, count_a, components_a, stride_a, 76.0f); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 5, srcstream, 0, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));
        float* a = (float*)stream_a;
        for(uint32_t i = 1; i <= count_a; ++i)
        {
            for(uint32_t c = 0; c < components_a; ++c)
            {
                ASSERT_EQ(76.0f, a[c]);
            }
            a += stride_a;
        }
        lua_pop(L, 1);
    }


    // Buffer overrun (read)
    {
        memset(stream_a, 'X', size_a); // memset the A stream

        RunString(L, "  local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        local dststream = buffer.get_stream(test_buffer, hash(\"a\")) \
                        buffer.copy_stream(dststream, 0, srcstream, 5, #srcstream) \
                      ");
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));
        uint8_t* a = (uint8_t*)stream_a;
        for( uint32_t i = 1; i <= m_Count; ++i )
        {
            ASSERT_EQ('X', a[i-1]);
        }
        lua_pop(L, 1);
    }

    dmLogWarning("<- Expected error outputs end.");

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_P(ScriptBufferCopyTest, CopyBuffer)
{
    const CopyBufferTestParams& p = GetParam();

    int top = lua_gettop(L);

    dmBuffer::Result r;
    uint16_t* stream_rgb = 0;
    uint32_t count_rgb = 0;
    uint32_t components_rgb = 0;
    uint32_t stride_rgb = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("rgb"), (void**)&stream_rgb, &count_rgb, &components_rgb, &stride_rgb);
    uint32_t size_rgb = count_rgb * components_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(p.m_Count * sizeof(uint16_t) * 3u, size_rgb);

    float* stream_a = 0;
    uint32_t count_a = 0;
    uint32_t components_a = 0;
    uint32_t stride_a = 0;
    r = dmBuffer::GetStream(m_Buffer, dmHashString64("a"), (void**)&stream_a, &count_a, &components_a, &stride_a);
    uint32_t size_a = count_a * components_a * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_FLOAT32);
    ASSERT_EQ(dmBuffer::RESULT_OK, r);
    ASSERT_EQ(p.m_Count * sizeof(float) * 1u, size_a);


    uint8_t* data;
    uint32_t datasize;
    dmBuffer::GetBytes(m_Buffer, (void**)&data, &datasize);
    uint32_t stride = stride_rgb * dmBuffer::GetSizeForValueType(dmBuffer::VALUE_TYPE_UINT16);
    memset(data, 0, datasize);

    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "dstbuffer");

        // Copy one stream to another
    {
        memset(data, 0, datasize);

        char str[1024];
        dmSnPrintf(str, sizeof(str), " local srcbuffer = buffer.create( %u, { {name=hash(\"rgb\"), type=buffer.VALUE_TYPE_UINT16, count=3 }, \
                                                                                  {name=hash(\"a\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                                        local rgb = buffer.get_stream(srcbuffer, \"rgb\") \
                                        local a = buffer.get_stream(srcbuffer, \"a\") \
                                        for i=1,%u do \
                                            rgb[(i-1)*3 + 0 + 1] = i*3 \
                                            rgb[(i-1)*3 + 1 + 1] = i*5 \
                                            rgb[(i-1)*3 + 2 + 1] = i*7 \
                                            a[i] = i*3 +  i*5 + i*7 \
                                        end \
                                        buffer.copy_buffer(dstbuffer, %u, srcbuffer, %u, %u) \
                                      ", p.m_Count, p.m_Count, p.m_DstOffset, p.m_SrcOffset, p.m_CopyCount);

        if( !p.m_ExpectedOk )
        {
            dmLogWarning("Expected error outputs ->");
        }

        bool run = RunString(L, str);
        ASSERT_EQ(p.m_ExpectedOk, run);
        ASSERT_EQ(dmBuffer::RESULT_OK, dmBuffer::ValidateBuffer(m_Buffer));

        if( p.m_ExpectedOk )
        {
            // Check that the buffer before the write area is intact
            for( uint32_t i = 1; i <= p.m_DstOffset; ++i)
            {
                uint32_t index = (i-1) * stride;
                ASSERT_EQ(0, data[index]);
            }

            // Check that the buffer after the write area is intact
            for( uint32_t i = p.m_DstOffset + p.m_CopyCount + 1; i <= p.m_Count; ++i )
            {
                uint32_t index = (i-1) * stride;
                ASSERT_EQ(0, data[index]);
            }

            // Loop over RGB and A to make sure we copied the streams correctly from the source buffer to the target buffer
            uint16_t* rgb = stream_rgb + p.m_DstOffset * stride_rgb;
            float* a = stream_a + p.m_DstOffset * stride_a;
            // Loop variable "i" is the struct index (i.e. vertex number)
            for( uint32_t i = p.m_DstOffset+1; i <= p.m_DstOffset + p.m_CopyCount; ++i )
            {
                uint32_t srci = p.m_SrcOffset + i - p.m_DstOffset;

                for( uint32_t c = 0; c < components_rgb; ++c)
                {
                    ASSERT_EQ( srci * (3 + c*2), rgb[c] );
                }
                rgb += stride_rgb;

                ASSERT_EQ( srci*3 + srci*5 + srci*7, a[0] );
                a += stride_a;
            }
        }
        else
        {
            for( uint32_t i = 1; i <= p.m_Count; ++i )
            {
                ASSERT_EQ(0, stream_rgb[(i-1)*3 + 0]);
                ASSERT_EQ(0, stream_rgb[(i-1)*3 + 1]);
                ASSERT_EQ(0, stream_rgb[(i-1)*3 + 2]);
                ASSERT_EQ(0, stream_a[i-1]);
            }
            lua_pop(L, 1); // ?
        }


        if( !p.m_ExpectedOk )
        {
            dmLogWarning("<- Expected error outputs end.");
        }
    }

    ASSERT_EQ(top, lua_gettop(L));
}


const CopyBufferTestParams buffer_copy_setups[] = {
    // VALID
    {64, 0, 0, 64, true}, // whole buffer
    {64, 0, 0, 16, true},  // copy first elements, to the start of the buffer
    {64, 64-16, 0, 16, true}, // copy first elements, to the end of the buffer
    {64, 64-16, 64-16, 16, true}, // copy last elements, to the end of the buffer
    {64, 0, 64-1, 1, true}, // single element
    // INVALID (leaves the buffer unchanged)
    {64, 0, 0, 100, false}, // too many elements
    {64, 0, 64-16, 32, false},  // overrun (read)
    {64, 64-16, 0, 32, false},  // overrun (write)
    {64, 0, 0, 0, false}, // zero elements
};

INSTANTIATE_TEST_CASE_P(ScriptBufferCopySequence, ScriptBufferCopyTest, jc_test_values_in(buffer_copy_setups));


TEST_F(ScriptBufferTest, RefCount)
{
    bool run;
    run = RunString(L, "local srcbuffer = buffer.create( 16*16, { {name=hash(\"temp\"), type=buffer.VALUE_TYPE_FLOAT32, count=1 } } ) \
                        local srcstream = buffer.get_stream(srcbuffer, \"temp\") \
                        srcbuffer = nil \
                        collectgarbage() \
                        srcstream[1] = 1.0 \
                       ");
    ASSERT_TRUE(run);

    // Create a buffer, store it globally, test that it works, remove buffer, test that the script usage throws an error
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    lua_setglobal(L, "test_buffer");

    ASSERT_TRUE(RunString(L, "print(test_buffer)"));

    dmBuffer::Destroy(m_Buffer);
    m_Buffer = 0;

    // DE 190114: DEF-3677
    // This test is disabled since the Buffer_tostring issues a lua_error which results
    // in follow up failures with ASAN enabled in the test ScriptBufferCopyTest.CopyBuffer
    // Why this happens is unclear and the Jira will remain open until this is sorted out.
    // Attempting other actions on the test_buffer that issues a lua_error works fine, it
    // is just when Buffer_tostring issues a lua_error that we end up with a ASAN error
    // in ScriptBufferCopyTest.CopyBuffer
    // Disabling this test to make the dev nightly builds pass.
#if defined(GITHUB_CI) && !defined(DM_SANITIZE_ADDRESS)
    dmLogWarning("Expected error outputs ->");

    ASSERT_FALSE(RunString(L, "print(test_buffer)"));

    dmLogWarning("<- Expected error outputs end.");
#endif
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
            float* f_ptr = (float*) value_ptr;
            ASSERT_NEAR(position_expected[i], f_ptr[i], EPSILON);
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
            int16_t* short_values = (int16_t*) value_ptr;
            ASSERT_EQ(texcoord0_expected[i], short_values[i]);
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
            float* f_ptr = (float*) value_ptr;
            ASSERT_NEAR(color_expected[i], f_ptr[i], EPSILON);
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

TEST_F(ComponentTest, TextureTransformVertexBuffer)
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
    ASSERT_NE((void*)0, sprite_go);

    dmGameObject::HInstance model_go = Spawn(m_Factory, m_Collection, "/model/texture_transform_model.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, model_go);

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
        ASSERT_EQ(sprite_vertex_count * vertex_stride, ((dmGraphics::VertexBuffer*)sprite_vx_buffer->m_Buffers[0])->m_Size);

        const char* sprite_vb_base = ((dmGraphics::VertexBuffer*)sprite_vx_buffer->m_Buffers[0])->m_Buffer;
        const float* written_sprite_tt = (const float*)(sprite_vb_base + tt_offset);
        for (int i = 0; i < 9; ++i)
        {
            ASSERT_NEAR(expected_sprite_tt[i], written_sprite_tt[i], EPSILON);
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
            const float* tt = (const float*)(model_vb_base + v * vertex_stride + tt_offset);
            for (int i = 0; i < 9; ++i)
            {
                ASSERT_NEAR(identity_mat3[i], tt[i], EPSILON);
            }
        }
        dmGraphics::UnmapVertexBuffer(m_GraphicsContext, model_vx_buffer);
    }

    dmResource::Release(m_Factory, material_res);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, SpriteTextureTransformMultiAtlasVertexBuffer)
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
    ASSERT_NE((void*)0, go);

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
    ASSERT_EQ(vertex_count * vertex_stride, ((dmGraphics::VertexBuffer*)vx_buffer->m_Buffers[0])->m_Size);

    const char* vb_base = ((dmGraphics::VertexBuffer*)vx_buffer->m_Buffers[0])->m_Buffer;
    const float* written_tt0 = (const float*)(vb_base + tt0_offset);
    const float* written_tt1 = (const float*)(vb_base + tt1_offset);
    for (int i = 0; i < 9; ++i)
    {
        ASSERT_NEAR(expected_tt0[i], written_tt0[i], EPSILON);
        ASSERT_NEAR(expected_tt1[i], written_tt1[i], EPSILON);
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
    uint8_t value_buffer[sizeof(float) * 4];
    T* values = (T*) value_buffer;

    dmGraphics::VertexAttributeInfo attr = {};
    attr.m_ElementCount = num_values;
    attr.m_DataType = data_type;

    dmGameSystem::ConvertMaterialAttributeValuesToDataType(info, info_index, &attr, value_buffer);

    for (int i = 0; i < num_values; ++i)
    {
        ASSERT_EQ(expected_values[i], values[i]);
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

TEST_F(MaterialTest, DynamicVertexAttributesWithGoAnimate)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/attributes_dynamic_go_animate.goc", dmHashString64("/attributes_go_animate"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (int i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(MaterialTest, DynamicVertexAttributesGoSetGetSparse)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/material/attributes_dynamic_go_set_get_sparse.goc", dmHashString64("/attributes_dynamic_go_set_get_sparse"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

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
        ASSERT_NE((void*)0, go);
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
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
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

    dmGraphics::UniformBufferLayout layout;
    dmGraphics::GetUniformBufferLayout(0, types, DM_ARRAY_SIZE(types), &layout);

    dmGraphics::HProgram program = dmRender::GetMaterialProgram(material);
    const dmGraphics::ShaderMeta* program_meta = dmGraphics::GetShaderMeta(program);

    dmGraphics::UniformBufferLayout built_layout;
    dmGraphics::GetUniformBufferLayout(0, program_meta->m_TypeInfos.Begin(), program_meta->m_TypeInfos.Size(), &built_layout);

    ASSERT_EQ(layout.m_Size, built_layout.m_Size);
    ASSERT_EQ(layout.m_Hash, built_layout.m_Hash);

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

TEST_F(ResourceTest, TestLightBufferWriteIntoUbo)
{
    // Spawns multiple point-light components from the same .lightc with distinct transforms, runs the
    // game-object update (LateUpdate -> SetLightInstance -> scratch), then applies light_buffer.material
    // (fragment shader sums lights[i].color from LightBuffer) so ApplyMaterialProgramLightBuffers uploads
    // scratch + active light count into the render light uniform buffer. On the null graphics adapter we
    // memcmp the UBO backing store against scratch to prove the upload path ran with the expected layout.
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

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
    ASSERT_EQ(10u, render_ctx->m_LightBufferScratch.Size());
    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_VEC3(positions[i], render_ctx->m_LightBufferScratch[i].m_Position);
    }

    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/light_buffer.materialc", (void**) &material_res));
    ASSERT_NE((void*)0, material_res);
    dmRender::HMaterial material = material_res->m_Material;
    ASSERT_NE((void*)0, material);
    ASSERT_TRUE(material->m_HasLightBuffer);

    // Writes the light count into the light uniform buffer
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material);

    dmGraphics::NullUniformBuffer* ubo = (dmGraphics::NullUniformBuffer*) render_ctx->m_LightUniformBuffer;
    ASSERT_NE((void*)0, ubo);
    ASSERT_NE((void*)0, ubo->m_Buffer);

    float count_written = 0.0f;
    memcpy(&count_written, ubo->m_Buffer, sizeof(float));
    ASSERT_NEAR(10.0f, count_written, EPSILON);

    const uint32_t light_data_offset = render_ctx->m_LightBufferDataWriteStart;
    const uint32_t light_data_bytes  = 10u * (uint32_t) sizeof(dmRender::LightSTD140);
    ASSERT_LE(light_data_offset + light_data_bytes, ubo->m_BufferSize);
    ASSERT_EQ(0, memcmp(ubo->m_Buffer + light_data_offset, render_ctx->m_LightBufferScratch.Begin(), light_data_bytes));

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
TEST_F(ResourceTest, TestLightBufferWriteIntoUboCompute)
{
    // Same as TestLightBufferWriteIntoUbo, but uses a compute program that declares LightBuffer
    // and dmRender::ApplyComputeProgramLightBuffers to upload scratch into the light UBO.
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

    dmGameSystem::ComputeResource* compute_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/shader/light_buffer.computec", (void**) &compute_res));
    ASSERT_NE((void*)0, compute_res);
    dmRender::HComputeProgram compute_program = compute_res->m_Program;
    ASSERT_NE((void*)0, compute_program);
    ASSERT_TRUE(compute_program->m_HasLightBuffer);

    dmRender::ApplyComputeProgramLightBuffers(m_RenderContext, compute_program);

    dmGraphics::NullUniformBuffer* ubo = (dmGraphics::NullUniformBuffer*) render_ctx->m_LightUniformBuffer;
    ASSERT_NE((void*)0, ubo);
    ASSERT_NE((void*)0, ubo->m_Buffer);

    float count_written = 0.0f;
    memcpy(&count_written, ubo->m_Buffer, sizeof(float));
    ASSERT_NEAR(10.0f, count_written, EPSILON);

    const uint32_t light_data_offset = render_ctx->m_LightBufferDataWriteStart;
    const uint32_t light_data_bytes  = 10u * (uint32_t) sizeof(dmRender::LightSTD140);
    ASSERT_LE(light_data_offset + light_data_bytes, ubo->m_BufferSize);
    ASSERT_EQ(0, memcmp(ubo->m_Buffer + light_data_offset, render_ctx->m_LightBufferScratch.Begin(), light_data_bytes));

    dmResource::Release(m_Factory, (void*) compute_res);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}
#endif // DM_HAVE_PLATFORM_COMPUTE_SUPPORT

#endif // !defined(DM_PLATFORM_VENDOR)

TEST_F(ComponentTest, GetSetCollisionShape)
{
    dmHashEnableReverseHash(true);

    dmGameObject::HInstance go_base = Spawn(m_Factory, m_Collection, "/collision_object/get_set_shape.goc", dmHashString64("/get_set_shape_go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_base);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, GetSetErrorCollisionShape)
{
    dmHashEnableReverseHash(true);

    dmGameObject::HInstance go_base = Spawn(m_Factory, m_Collection, "/collision_object/get_set_error_shape.goc", dmHashString64("/get_set_error_shape_go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_base);

    ASSERT_FALSE(dmGameObject::Final(m_Collection));
}

TEST_F(SysTest, LoadBufferSync)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sys/load_buffer_sync.goc", dmHashString64("/load_buffer_sync"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

static bool RunTestLoadBufferASync(int test_n,
    dmGameSystem::ScriptLibContext& scriptlibcontext,
    dmGameObject::HCollection collection,
    dmGameObject::UpdateContext* update_context,
    bool ignore_script_update_fail)
{
    char buffer[256];
    dmSnPrintf(buffer, sizeof(buffer), "test_n = %d", test_n);

    if (!RunString(scriptlibcontext.m_LuaState, buffer))
        return false;

    return UpdateAndWaitUntilDone(scriptlibcontext, collection, update_context, ignore_script_update_fail, "tests_done", 3);
}

TEST_F(SysTest, LoadBufferASync)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sys/load_buffer_async.goc", dmHashString64("/load_buffer_async"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    // Test 1
    ASSERT_TRUE(RunTestLoadBufferASync(1, m_Scriptlibcontext, m_Collection, &m_UpdateContext, false));

    // Test 2
    uint32_t large_buffer_size = 16 * 1024 * 1024;
    uint8_t* large_buffer = new uint8_t[large_buffer_size];
    memset(large_buffer, 0, large_buffer_size);

    large_buffer[0]                   = 127;
    large_buffer[large_buffer_size-1] = 255;

    dmResource::AddFile(m_Factory, "/sys/non_disk_content/large_file.raw", large_buffer_size, large_buffer);
    ASSERT_TRUE(RunTestLoadBufferASync(2, m_Scriptlibcontext, m_Collection, &m_UpdateContext, false));
    dmResource::RemoveFile(m_Factory, "/sys/non_disk_content/large_file.raw");
    delete[] large_buffer;

    // Test 3
    ASSERT_TRUE(RunTestLoadBufferASync(3, m_Scriptlibcontext, m_Collection, &m_UpdateContext, true));

    // Test 4
    ASSERT_TRUE(RunTestLoadBufferASync(4, m_Scriptlibcontext, m_Collection, &m_UpdateContext, true));

    // Test 5
    ASSERT_TRUE(RunTestLoadBufferASync(5, m_Scriptlibcontext, m_Collection, &m_UpdateContext, true));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

#ifdef DM_HAVE_PLATFORM_COMPUTE_SUPPORT

TEST_F(ShaderTest, Compute)
{
    dmGraphics::ShaderDesc* ddf;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile("build/src/gamesys/test/shader/valid.cp.spc", dmGraphics::ShaderDesc::m_DDFDescriptor, (void**) &ddf));
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
    ASSERT_EQ(9, dmGraphics::GetUniformCount(graphics_compute_program));

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
    ASSERT_EQ(dmGraphics::TEXTURE_TYPE_TEXTURE_2D,    sampler_tex_b->m_Type);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST,     sampler_tex_b->m_MinFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_NEAREST,     sampler_tex_b->m_MagFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_REPEAT,        sampler_tex_b->m_UWrap);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_REPEAT,        sampler_tex_b->m_VWrap);
    ASSERT_NEAR(0.0f, sampler_tex_b->m_MaxAnisotropy, EPSILON);

    ASSERT_NE((dmRender::Sampler*) 0, sampler_tex_c);
    ASSERT_EQ(dmHashString64("texture_c"),             sampler_tex_c->m_NameHash);
    ASSERT_EQ(dmGraphics::TEXTURE_TYPE_TEXTURE_2D,     sampler_tex_c->m_Type);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR,       sampler_tex_c->m_MinFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_FILTER_LINEAR,       sampler_tex_c->m_MagFilter);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE,  sampler_tex_c->m_UWrap);
    ASSERT_EQ(dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE,  sampler_tex_c->m_VWrap);
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
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PlayAnim)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_anim.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "play_anim_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PlayAnimMessage)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_anim_message.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "play_anim_message_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, PlayAnimMissingAnimation)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_anim_missing.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "play_anim_missing_done", 5));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, BlendWeightsScript)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/script_model_blend_weights.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

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
    ASSERT_NE((void*)0, go);

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

TEST_F(ModelTest, DynamicVertexAttributes)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/dynamic_vertex_attributes.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

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

    dmGameObject::Result res = dmGameObject::GetComponent(go, dmHashString64("model"), &component_type, &component, &world);
    ASSERT_EQ(dmGameObject::RESULT_OK, res);

    // Inspect the render data after render
    dmGraphics::HVertexBuffer vx_buffer;
    dmGraphics::HVertexDeclaration vx_decl;
    dmGraphics::HVertexDeclaration inst_decl;
    dmGameSystem::GetModelComponentAttributeRenderData(component, 0, &vx_buffer, &vx_decl, &inst_decl);

    ASSERT_EQ(1, vx_decl->m_StreamCount);
    ASSERT_EQ(dmHashString64("custom_color"), vx_decl->m_Streams[0].m_NameHash);

    // Note: The vertex buffer contains only the custom data, not the position stream!
    struct vx_format
    {
        dmVMath::Vector4 custom_color;
    };

    // Should be a cube with 24 vertices
    uint32_t exp_num_vertices = 24;
    uint32_t vx_buffer_size = dmGraphics::GetVertexBufferSize(vx_buffer);
    ASSERT_EQ(exp_num_vertices, vx_buffer_size / sizeof(vx_format));

    vx_format* vx_data = (vx_format*) dmGraphics::MapVertexBuffer(m_GraphicsContext, vx_buffer, dmGraphics::BUFFER_ACCESS_READ_ONLY);

    // This should be the last value that the script "dynamic_vertex_attributes.script" sets
    dmVMath::Vector4 exp = dmVMath::Vector4(0.0f, 1.0f, 0.0f, 1.0f);

    for (int i = 0; i < exp_num_vertices; ++i)
    {
        ASSERT_VEC4(exp, vx_data[i].custom_color);
    }

    dmGraphics::UnmapVertexBuffer(m_GraphicsContext, vx_buffer);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ModelTest, MeshAttributeRenderDataPurge)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/model/dynamic_vertex_attributes.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

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
    dmGameSystem::HComponentRenderConstants render_constants;

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
    ASSERT_NE((void*)0, go);

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

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Mock per-property handlers for testing
static dmHashTable64<dmhash_t> g_MockPerPropertyValues;

static dmGameObject::PropertyResult MockSpineSceneSetProperty(dmGui::HScene scene, const dmGameObject::ComponentSetPropertyParams& params)
{
    dmhash_t key;
    if (GetPropertyOptionsKey(params.m_Options, 0, &key) != dmGameObject::PROPERTY_RESULT_OK)
    {
        return dmGameObject::PROPERTY_RESULT_INVALID_KEY;
    }
    g_MockPerPropertyValues.Put(key, params.m_Value.m_Hash);
    return dmGameObject::PROPERTY_RESULT_OK;
}

static dmGameObject::PropertyResult MockSpineSceneGetProperty(dmGui::HScene scene, const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
{
    dmhash_t key;
    if (GetPropertyOptionsKey(params.m_Options, 0, &key) != dmGameObject::PROPERTY_RESULT_OK)
    {
        return dmGameObject::PROPERTY_RESULT_INVALID_KEY;
    }
    dmhash_t* value = g_MockPerPropertyValues.Get(key);
    if (!value)
    {
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }
    out_value.m_Variant.m_Hash = *value;
    out_value.m_ValueType = dmGameObject::PROP_VALUE_HASHTABLE;
    return dmGameObject::PROPERTY_RESULT_OK;
}

TEST_F(GuiTest, PerPropertyRegistration)
{
    g_MockPerPropertyValues.Clear();
    g_MockPerPropertyValues.SetCapacity(8, 16);
    
    dmhash_t spine_scene_hash = dmHashString64("spine_scene");
    
    // Test registering per-property setter and getter
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiRegisterSetPropertyFn(spine_scene_hash, MockSpineSceneSetProperty));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiRegisterGetPropertyFn(spine_scene_hash, MockSpineSceneGetProperty));
    
    // Create a GUI component
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/valid_gui.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);
    
    // Test setting per-property with key
    dmGameObject::PropertyOptions options;
    dmGameObject::AddPropertyOptionsKey(&options, dmHashString64("test_node"));
    
    dmGameObject::PropertyVar value(dmHashString64("test_spine_scene"));
    dmGameObject::PropertyResult result = dmGameObject::SetProperty(go, dmHashString64("gui"), spine_scene_hash, options, value);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    
    // Test getting per-property
    dmGameObject::PropertyDesc desc;
    result = dmGameObject::GetProperty(go, dmHashString64("gui"), spine_scene_hash, options, desc);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    ASSERT_EQ(dmHashString64("test_spine_scene"), desc.m_Variant.m_Hash);
    
    // Test unregistering setter
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiUnregisterSetPropertyFn(spine_scene_hash));
    
    // Verify setter no longer works
    dmGameObject::PropertyVar newValue(dmHashString64("new_spine_scene"));
    result = dmGameObject::SetProperty(go, dmHashString64("gui"), spine_scene_hash, options, newValue);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_NOT_FOUND, result);
    
    // But getter should still work
    result = dmGameObject::GetProperty(go, dmHashString64("gui"), spine_scene_hash, options, desc);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    ASSERT_EQ(dmHashString64("test_spine_scene"), desc.m_Variant.m_Hash);
    
    // Test unregistering getter
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiUnregisterGetPropertyFn(spine_scene_hash));
    
    // Verify getter no longer works
    result = dmGameObject::GetProperty(go, dmHashString64("gui"), spine_scene_hash, options, desc);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_NOT_FOUND, result);
    
    // Test unregistering non-existent handler
    ASSERT_EQ(dmGameObject::RESULT_ALREADY_REGISTERED, dmGameSystem::CompGuiUnregisterSetPropertyFn(spine_scene_hash));
    ASSERT_EQ(dmGameObject::RESULT_INVALID_OPERATION, dmGameSystem::CompGuiUnregisterGetPropertyFn(spine_scene_hash));
    
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, PerPropertyPrecedence)
{
    g_MockPerPropertyValues.Clear();
    g_MockPerPropertyValues.SetCapacity(8, 16);
    
    dmhash_t test_prop_hash = dmHashString64("test_prop");
    
    // Register per-property handlers first
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiRegisterSetPropertyFn(test_prop_hash, MockSpineSceneSetProperty));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiRegisterGetPropertyFn(test_prop_hash, MockSpineSceneGetProperty));
    
    // Create a GUI component
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/valid_gui.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);
    
    dmGameObject::PropertyOptions options;
    dmGameObject::AddPropertyOptionsKey(&options, dmHashString64("test_node"));
    
    // Test that per-property handler is called (not extension handlers)
    dmGameObject::PropertyVar value(dmHashString64("per_property_value"));
    dmGameObject::PropertyResult result = dmGameObject::SetProperty(go, dmHashString64("gui"), test_prop_hash, options, value);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    
    dmGameObject::PropertyDesc desc;
    result = dmGameObject::GetProperty(go, dmHashString64("gui"), test_prop_hash, options, desc);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    ASSERT_EQ(dmHashString64("per_property_value"), desc.m_Variant.m_Hash);
    
    // Clean up
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiUnregisterSetPropertyFn(test_prop_hash));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompGuiUnregisterGetPropertyFn(test_prop_hash));
    
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();

    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    dmHashEnableReverseHash(true);
    // Enable message descriptor translation when sending messages
    dmDDF::RegisterAllTypes();

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmLog::LogFinalize();
    return result;
}
