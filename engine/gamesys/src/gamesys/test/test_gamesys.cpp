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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

using namespace dmVMath;

#if !defined(DM_TEST_EXTERN_INIT_FUNCTIONS)
    bool GameSystemTest_PlatformInit()
    {
        return true;
    }
    void GameSystemTest_PlatformExit()
    {
    }
#endif

// Reloading these resources needs an update to clear any dirty data and get to a good state.
static const char* update_after_reload[] = {"/tile/valid.tilemapc", "/tile/valid_tilegrid_collisionobject.goc"};

bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

bool CopyResource(const char* content_folder, const char* src, const char* dst)
{
    char src_path[128];
    dmTestUtil::MakeHostPathf(src_path, sizeof(src_path), "build/src/gamesys/test/%s/%s", content_folder, src);
    FILE* src_f = fopen(src_path, "rb");
    if (src_f == 0x0)
        return false;
    char dst_path[128];
    dmTestUtil::MakeHostPathf(dst_path, sizeof(dst_path), "build/src/gamesys/test/%s/%s", content_folder, dst);
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

bool UnlinkResource(const char* content_folder, const char* name)
{
    char path[128];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s/%s", content_folder, name);
    return dmSys::Unlink(path) == 0;
}

void DeleteInstance(dmGameObject::HCollection collection, dmGameObject::HInstance instance) {
    dmGameObject::UpdateContext ctx;
    dmGameObject::Update(collection, &ctx);
    dmGameObject::Delete(collection, instance, false);
    dmGameObject::PostUpdate(collection);
}

CollectionProxyComponentRef GetCollectionProxyComponentRef(dmGameObject::HInstance instance, dmhash_t component_id)
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

dmGameObject::HCollection GetCollectionByName(dmGameObject::HRegister regist, const char* name)
{
    dmGameObject::HCollection collection = dmGameObject::GetCollectionByHash(regist, dmHashString64(name));
    EXPECT_NE(0, collection);
    return collection;
}

void ConfigureCollectionProxy(CollectionProxyComponentRef proxy, const char* collection_path, dmGameObject::Result expected_load_result)
{
    ASSERT_EQ(dmGameSystem::SET_COLLECTION_PATH_RESULT_OK, dmGameSystem::CollectionProxySetCollectionPath(proxy.m_World, proxy.m_Component, collection_path));
    ASSERT_EQ(expected_load_result, dmGameSystem::CompCollectionProxyLoad(proxy.m_World, proxy.m_Component, 0, 0));
    if (expected_load_result != dmGameObject::RESULT_OK)
        return;

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyInitialize(proxy.m_World, proxy.m_Component));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyEnable(proxy.m_World, proxy.m_Component));
}

void UpdateAndPostUpdateCollection(dmGameObject::HCollection collection, dmGameObject::UpdateContext* update_context, dmGameObject::HRegister regist)
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

class RenderTargetResourceTest : public ResourceTest { public: RenderTargetResourceTest() { SetContentFolder("render_target"); } };
class TileGrid3DResourceTest : public ResourceTest
{
public:
    TileGrid3DResourceTest()
    {
        SetContentFolder("tile");
        m_projectOptions.m_3D = true;
    }
};

TEST_F(TileGrid3DResourceTest, LoadTileGrid)
{
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/tile/valid.tilemapc", &resource));
    ASSERT_NE((void*) 0, resource);
    dmResource::Release(m_Factory, resource);
}

TEST_F(RenderTargetResourceTest, InvalidCubemapFailsToLoad)
{
    void* resource = 0;
    ASSERT_EQ(dmResource::RESULT_FORMAT_ERROR, dmResource::Get(m_Factory, "/render_target/invalid_cubemap.render_targetc", &resource));
    ASSERT_EQ((void*) 0, resource);
}

TEST_F(RenderTargetResourceTest, InvalidCubemapFailsToReload)
{
    const char* valid_path = "/render_target/valid.render_targetc";
    dmGameSystem::RenderTargetResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, valid_path, (void**) &resource));
    ASSERT_NE((void*) 0, resource);

    dmGraphics::HRenderTarget original_render_target = resource->m_RenderTarget;
    ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, original_render_target));

    char invalid_host_path[256];
    dmTestUtil::MakeHostPathf(invalid_host_path, sizeof(invalid_host_path), "build/src/gamesys/test/%s/render_target/invalid_cubemap.render_targetc", GetContentFolder());
    uint32_t invalid_data_size = 0;
    uint8_t* invalid_data = dmTestUtil::ReadFile(invalid_host_path, &invalid_data_size);
    ASSERT_NE((uint8_t*) 0, invalid_data);

    ASSERT_EQ(dmResource::RESULT_FORMAT_ERROR, dmResource::SetResource(m_Factory, dmHashString64(valid_path), invalid_data, invalid_data_size));
    dmMemory::AlignedFree(invalid_data);

    ASSERT_EQ(original_render_target, resource->m_RenderTarget);
    ASSERT_TRUE(dmGraphics::IsAssetHandleValid(m_GraphicsContext, resource->m_RenderTarget));
    dmResource::Release(m_Factory, resource);
}

TEST_F(TextureSetResourceTest, TestReloadTextureSet)
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
    ASSERT_TRUE(CopyResource(GetContentFolder(), texture_set_path_a, texture_set_path_tmp));
    ASSERT_TRUE(CopyResource(GetContentFolder(), texture_set_path_b, texture_set_path_a));
    ASSERT_TRUE(CopyResource(GetContentFolder(), texture_set_path_tmp, texture_set_path_b));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, texture_set_path_a, 0));

    // If the load truly was successful, we should have a new width/height for the internal image
    ASSERT_NE(original_width,dmGraphics::GetOriginalTextureWidth(m_GraphicsContext, resource->m_Texture->m_Texture));
    ASSERT_NE(original_height,dmGraphics::GetOriginalTextureHeight(m_GraphicsContext, resource->m_Texture->m_Texture));

    dmResource::Release(m_Factory, (void**) resource);
}

TEST_F(RenderResourceTest, TestRenderPrototypeResources)
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

    HResourceDescriptor rd_mat = dmResource::FindByHash(m_Factory, dmHashString64("/render/material_valid.materialc"));
    ASSERT_NE((void*)0, rd_mat);

    HResourceDescriptor rd_rt = dmResource::FindByHash(m_Factory, dmHashString64("/render/render_target_valid.render_targetc"));
    ASSERT_NE((void*)0, rd_rt);

    HResourceType types[] = { res_type_material, res_type_material, res_type_render_target };
    void* resources[] = { dmResource::GetResource(rd_mat), dmResource::GetResource(rd_mat), dmResource::GetResource(rd_rt) };

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

TEST_F(DataResourceTest, DataResourceContents)
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

TEST_F(DataResourceTest, PrebuiltData)
{
    dmGameSystem::DataResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/data/prebuilt.datac", (void**)&resource));
    ASSERT_NE((void*)0, resource);

    const dmGameSystemDDF::Data* ddf = dmGameSystem::GetDDFData(resource);
    ASSERT_NE((void*)0, ddf);

    ASSERT_EQ(2u, ddf->m_Tags.m_Count);
    EXPECT_STREQ("tag-one", ddf->m_Tags[0]);
    EXPECT_STREQ("tag-two", ddf->m_Tags[1]);

    EXPECT_STREQ("hello", ddf->m_Data.m_Kind.m_String);

    dmResource::Release(m_Factory, (void*)resource);
}

TEST_F(LightResourceTest, LightResourcePrototype)
{
    /////////////////////////////////
    // Test point light
    /////////////////////////////////
    dmGameSystem::LightResource* res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_point.point_light.lightc", (void**)&res));
    ASSERT_NE((void*)0, res); 

    dmRender::HLightPrototype light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);

    const dmRender::LightPrototype* proto = dmRender::GetLightPrototype(m_RenderContext, light_prototype);
    ASSERT_NE((void*)0, proto);
    ASSERT_EQ(dmRender::LIGHT_TYPE_POINT, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.5f, 0.25f, 1.0f), proto->m_Color);
    ASSERT_NEAR(2.0f, proto->m_Intensity, EPSILON);
    ASSERT_NEAR(10.0f, proto->m_Range, EPSILON);

    dmResource::Release(m_Factory, (void*)res);

    /////////////////////////////////
    // Test directional light
    /////////////////////////////////
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_directional_light.directional_light.lightc", (void**)&res));
    ASSERT_NE((void*)0, res);

    light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);
    proto = dmRender::GetLightPrototype(m_RenderContext, light_prototype);
    ASSERT_NE((void*)0, proto);
    ASSERT_EQ(dmRender::LIGHT_TYPE_DIRECTIONAL, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.0f, 0.0f, 1.0f), proto->m_Color);
    ASSERT_NEAR(3.0f, proto->m_Intensity, EPSILON);

    dmResource::Release(m_Factory, (void*)res);

    /////////////////////////////////
    // Test ambient light
    /////////////////////////////////
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_ambient_light.ambient_light.lightc", (void**)&res));
    ASSERT_NE((void*)0, res);

    light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);
    proto = dmRender::GetLightPrototype(m_RenderContext, light_prototype);
    ASSERT_NE((void*)0, proto);
    ASSERT_EQ(dmRender::LIGHT_TYPE_AMBIENT, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(0.1f, 0.2f, 0.3f, 1.0f), proto->m_Color);
    ASSERT_NEAR(5.0f, proto->m_Intensity, EPSILON);

    dmResource::Release(m_Factory, (void*)res);

    /////////////////////////////////
    // Test spot light
    /////////////////////////////////
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/light/valid_spot_light.spot_light.lightc", (void**)&res));
    ASSERT_NE((void*)0, res);

    light_prototype = dmGameSystem::GetLightPrototype(res);
    ASSERT_NE((dmRender::HLightPrototype)0, light_prototype);
    proto = dmRender::GetLightPrototype(m_RenderContext, light_prototype);
    ASSERT_NE((void*)0, proto);
    ASSERT_EQ(dmRender::LIGHT_TYPE_SPOT, proto->m_Type);
    ASSERT_VEC4(dmVMath::Vector4(0.2f, 0.8f, 0.1f, 1.0f), proto->m_Color);
    ASSERT_NEAR(4.0f, proto->m_Intensity, EPSILON);
    ASSERT_NEAR(20.0f, proto->m_Range, EPSILON);
    ASSERT_NEAR(15.0f * 3.14159265f / 180.0f, proto->m_InnerConeAngle, EPSILON);
    ASSERT_NEAR(30.0f * 3.14159265f / 180.0f, proto->m_OuterConeAngle, EPSILON);

    dmResource::Release(m_Factory, (void*)res);
}

TEST_F(LightResourceTest, LightComponentUpdatesLightBuffer)
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

    dmGameObject::HInstance go_ambient = Spawn(m_Factory, m_Collection, "/light/valid_ambient_light.goc", dmHashString64("/light_ambient"), 0, Point3(0.0f, 0.0f, 0.0f), rot_id, Vector3(1, 1, 1));
    dmGameObject::HInstance go_point = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64("/light_point"), 0, pos_point, rot_id, Vector3(1, 1, 1));
    dmGameObject::HInstance go_dir = Spawn(m_Factory, m_Collection, "/light/valid_directional_light.goc", dmHashString64("/light_dir"), 0, pos_dir, rot_id, Vector3(1, 1, 1));
    dmGameObject::HInstance go_spot = Spawn(m_Factory, m_Collection, "/light/valid_spot_light.goc", dmHashString64("/light_spot"), 0, pos_spot, rot_id, Vector3(1, 1, 1));
    ASSERT_NE((dmGameObject::HInstance)0, go_ambient);
    ASSERT_NE((dmGameObject::HInstance)0, go_point);
    ASSERT_NE((dmGameObject::HInstance)0, go_dir);
    ASSERT_NE((dmGameObject::HInstance)0, go_spot);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(4u, render_ctx->m_LightBufferScratch.Size());

    // Ambient lights now retain the same per-instance source data as the other
    // types, but are folded into light_info.xyz instead of the uploaded lights[].
    const dmRender::LightSTD140& L_ambient = render_ctx->m_LightBufferScratch[0];
    ASSERT_VEC4(dmVMath::Vector4(0.1f, 0.2f, 0.3f, 1.0f), L_ambient.m_Color);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_AMBIENT, 5.0f, 0.0f, 0.0f), L_ambient.m_Params);

    // Creation order matches CompLightWorld component order and light buffer indices 0..3.
    const dmRender::LightSTD140& L_point = render_ctx->m_LightBufferScratch[1];
    ASSERT_VEC3(pos_point, L_point.m_Position);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.5f, 0.25f, 1.0f), L_point.m_Color);
    ASSERT_VEC4(dmVMath::Vector4(0.0f, 0.0f, 0.0f, 10.0f), L_point.m_DirectionRange);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_POINT, 2.0f, 0.0f, 0.0f), L_point.m_Params);

    const dmRender::LightSTD140& L_dir = render_ctx->m_LightBufferScratch[2];
    ASSERT_VEC3(pos_dir, L_dir.m_Position);
    ASSERT_VEC4(dmVMath::Vector4(1.0f, 0.0f, 0.0f, 1.0f), L_dir.m_Color);
    ASSERT_VEC4(dmVMath::Vector4(0.0f, 0.0f, -1.0f, 0.0f), L_dir.m_DirectionRange);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_DIRECTIONAL, 3.0f, 0.0f, 0.0f), L_dir.m_Params);

    const dmRender::LightSTD140& L_spot = render_ctx->m_LightBufferScratch[3];
    ASSERT_VEC3(pos_spot, L_spot.m_Position);
    ASSERT_VEC4(dmVMath::Vector4(0.2f, 0.8f, 0.1f, 1.0f), L_spot.m_Color);
    ASSERT_VEC4(dmVMath::Vector4(0.0f, 0.0f, -1.0f, 20.0f), L_spot.m_DirectionRange);
    ASSERT_VEC4(dmVMath::Vector4((float) dmRender::LIGHT_TYPE_SPOT, 4.0f, 15.0f * 3.14159265f / 180.0f, 30.0f * 3.14159265f / 180.0f), L_spot.m_Params);

    const Point3 pos_point_moved(7.0f, 8.0f, 9.0f);
    dmGameObject::SetPosition(go_point, pos_point_moved);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_VEC3(pos_point_moved, render_ctx->m_LightBufferScratch[1].m_Position);
    ASSERT_VEC3(pos_dir, render_ctx->m_LightBufferScratch[2].m_Position);
    ASSERT_VEC3(pos_spot, render_ctx->m_LightBufferScratch[3].m_Position);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LightResourceTest, AmbientLightsAreCompactedIntoLightInfo)
{
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);
    ASSERT_GT(render_ctx->m_MaxLightCount, 0u);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    const uint32_t ambient_count = render_ctx->m_MaxLightCount;
    for (uint32_t i = 0; i < ambient_count; ++i)
    {
        char id_buf[32];
        dmSnPrintf(id_buf, sizeof(id_buf), "/ambient%u", i);
        dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/light/valid_ambient_light.goc", dmHashString64(id_buf), 0, Point3(0.0f, 0.0f, 0.0f), Quat(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1, 1, 1));
        ASSERT_NE((dmGameObject::HInstance)0, go);
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(ambient_count, render_ctx->m_LightBufferScratch.Size());

    dmRender::BeginFrame(m_RenderContext, 1.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);

    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/light_buffer.materialc", (void**) &material_res));
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material_res->m_Material);

    ASSERT_EQ(0u, render_ctx->m_LightBufferUploadScratch.Size());
    ASSERT_VEC3(Vector3(0.5f * ambient_count, 1.0f * ambient_count, 1.5f * ambient_count), render_ctx->m_AmbientLight);

    dmResource::Release(m_Factory, (void*) material_res);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LightResourceTest, LightComponentUsesWorldTransform)
{
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);
    ASSERT_GE(render_ctx->m_MaxLightCount, 1u);

    const Quat rot_id(0.0f, 0.0f, 0.0f, 1.0f);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    const Point3 parent_pos(100.0f, 10.0f, -5.0f);
    const Point3 local_light_pos(4.0f, 5.0f, 6.0f);

    dmGameObject::HInstance parent = Spawn(m_Factory, m_Collection, "/light/valid_ambient_light.goc", dmHashString64("/light_parent"), 0, parent_pos, rot_id, Vector3(1, 1, 1));
    dmGameObject::HInstance child_light = Spawn(m_Factory, m_Collection, "/light/valid_point_light.goc", dmHashString64("/light_parent/light_child"), 0, local_light_pos, rot_id, Vector3(1, 1, 1));
    ASSERT_NE((dmGameObject::HInstance)0, parent);
    ASSERT_NE((dmGameObject::HInstance)0, child_light);
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::SetParent(child_light, parent));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(2u, render_ctx->m_LightBufferScratch.Size());
    ASSERT_VEC3(dmGameObject::GetWorldPosition(child_light), render_ctx->m_LightBufferScratch[1].m_Position);
    ASSERT_NEAR(10.0f, render_ctx->m_LightBufferScratch[1].m_DirectionRange.getW(), EPSILON);

    dmGameObject::SetScale(parent, Vector3(2.0f, 3.0f, 4.0f));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_VEC3(dmGameObject::GetWorldPosition(child_light), render_ctx->m_LightBufferScratch[1].m_Position);
    ASSERT_NEAR(20.0f, render_ctx->m_LightBufferScratch[1].m_DirectionRange.getW(), EPSILON);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LightResourceTest, ReloadLightResourceTest)
{
    const char* valid_light_a = "/light/valid_point.point_light.lightc";
    const char* valid_light_b = "/light/valid_directional_light.directional_light.lightc";
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

    dmRender::HLightPrototype valid_light_prototype_a = dmGameSystem::GetLightPrototype(resource);
    const dmRender::LightPrototype* valid_light_prototype_data_a = dmRender::GetLightPrototype(m_RenderContext, valid_light_prototype_a);
    ASSERT_NE((void*)0, valid_light_prototype_data_a);
    ASSERT_VEC4(dmVMath::Vector4(1.0, 0.5, 0.25, 1.0), valid_light_prototype_data_a->m_Color);
    ASSERT_NEAR(2.0, valid_light_prototype_data_a->m_Intensity, EPSILON);
    ASSERT_NEAR(10.0, valid_light_prototype_data_a->m_Range, EPSILON);

    ASSERT_TRUE(CopyResource(GetContentFolder(), valid_light_a, tmp_path));
    ASSERT_TRUE(CopyResource(GetContentFolder(), valid_light_b, valid_light_a));
    ASSERT_TRUE(CopyResource(GetContentFolder(), tmp_path, valid_light_b));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, valid_light_a, 0));

    // A reload will keep the same prototype handle.
    dmRender::HLightPrototype valid_light_prototype_b = dmGameSystem::GetLightPrototype(resource);
    ASSERT_EQ(valid_light_prototype_a, valid_light_prototype_b);
    const dmRender::LightPrototype* valid_light_prototype_data_b = dmRender::GetLightPrototype(m_RenderContext, valid_light_prototype_b);
    ASSERT_NE((void*)0, valid_light_prototype_data_b);

    ASSERT_VEC4(dmVMath::Vector4(1.0, 0.0, 0.0, 1.0), valid_light_prototype_data_b->m_Color);
    ASSERT_NEAR(3.0, valid_light_prototype_data_b->m_Intensity, EPSILON);

    dmResource::Release(m_Factory, (void**) resource);

    ASSERT_TRUE(CopyResource(GetContentFolder(), valid_light_a, tmp_path));
    ASSERT_TRUE(CopyResource(GetContentFolder(), valid_light_b, valid_light_a));
    ASSERT_TRUE(CopyResource(GetContentFolder(), tmp_path, valid_light_b));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

bool UpdateAndWaitUntilDone(
    dmGameSystem::ScriptLibContext&    scriptlibcontext,
    dmGameObject::HCollection          collection,
    dmGameObject::UpdateContext*       update_context,
    bool                               ignore_script_update_fail,
    const char*                        tests_done_key,
    uint32_t                           timeout_seconds)
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
        dmGameSystem::UpdateScriptLibs(scriptlibcontext);
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

TEST_F(ResourceFolderTest, TestCreateTextureFromScript)
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
    ASSERT_NE(0, go);

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

// Verify that resource.create_texture_async() keeps its callback and upload buffer alive after
// the coroutine that created the request has finished and has been garbage collected.
TEST_F(ResourceFolderTest, TestCreateTextureAsyncFromCoroutine)
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

    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    null_context->m_UseAsyncTextureLoad   = 1;

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/create_texture_async_from_coroutine.goc", dmHashString64("/create_texture_async_from_coroutine"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(scriptlibcontext, m_Collection, &m_UpdateContext, false, "tests_done"));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceFolderTest, TestCreateSoundDataFromScript)
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
    ASSERT_NE(0, go);

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

TEST_F(ResourceFolderTest, TestResourceScriptBuffer)
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
    ASSERT_NE(0, go);

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceFolderTest, TestResourceScriptRenderTarget)
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
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceFolderTest, TestResourceScriptAtlas)
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
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(ResourceFolderTest, TestSetTextureFromScript)
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
    ASSERT_NE(0, go);

    dmGameSystem::TextureSetResource* texture_set_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/resource/tile_valid.t.texturesetc", (void**) &texture_set_res));

    dmGraphics::HTexture backing_texture = texture_set_res->m_Texture->m_Texture;
    ASSERT_EQ(dmGraphics::GetTextureWidth(m_GraphicsContext, backing_texture), 128);
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

    bool exists = CopyResource(GetContentFolder(), p.m_InvalidResource, tmp_name);
    ASSERT_TRUE(CopyResource(GetContentFolder(), p.m_ValidResource, p.m_InvalidResource));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, p.m_InvalidResource, &resource));

    if (exists)
        ASSERT_TRUE(CopyResource(GetContentFolder(), tmp_name, p.m_InvalidResource));
    else
        ASSERT_TRUE(UnlinkResource(GetContentFolder(), p.m_InvalidResource));
    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, p.m_InvalidResource, 0));

    dmResource::Release(m_Factory, resource);

    UnlinkResource(GetContentFolder(), tmp_name);
}

TEST_P(ComponentTest, Test)
{
    const char* go_name = GetParam();
    dmGameObjectDDF::PrototypeDesc* go_ddf;
    char path[128];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s/%s", GetContentFolder(), go_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, (void**)&go_ddf));
    ASSERT_LT(0u, go_ddf->m_Components.m_Count);
    const char* component_name = go_ddf->m_Components[0].m_Component;

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE(0, go);

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
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s/%s", GetContentFolder(), go_name);
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::LoadMessageFromFile(path, dmGameObjectDDF::PrototypeDesc::m_DDFDescriptor, (void**)&go_ddf));
    ASSERT_LT(0u, go_ddf->m_Components.m_Count);
    const char* component_name = go_ddf->m_Components[0].m_Component;
    const char* temp_name = "tmp";

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_NE(0, go);

    ASSERT_TRUE(CopyResource(GetContentFolder(), component_name, temp_name));
    ASSERT_TRUE(UnlinkResource(GetContentFolder(), component_name));

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

    ASSERT_TRUE(CopyResource(GetContentFolder(), temp_name, component_name));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmDDF::FreeMessage(go_ddf);
}

TEST_F(CameraComponentTest, CameraTest)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/camera/camera_info.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Test that tries to reload shaders with errors in them.
TEST_F(MaterialComponentTest, ReloadInvalidMaterial)
{
    const char path_material[] = "/material/valid.materialc";
    void* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_material, &resource));

    char path[1024];
    dmTestUtil::MakeHostPathf(path, sizeof(path), "build/src/gamesys/test/%s%s", GetContentFolder(), path_material);

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
TEST_F(CollectionProxyComponentTest, ConsumeInputInCollectionProxy)
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
    ASSERT_NE(0, go_consume_yes);

    dmGameObject::HInstance go_consume_no = Spawn(m_Factory, m_Collection, path_consume_no, hash_go_consume_no, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_consume_no);

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

TEST_F(CollectionProxyComponentTest, CollectionProxySetCollectionLoadInitialize)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    const char* go_path = "/collection_proxy/set_collection_single_root.goc";
    dmhash_t go_hash = dmHashString64("/go");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

TEST_F(CollectionProxyComponentTest, AmbientLightAccumulatesAcrossCollectionProxy)
{
    dmRender::RenderContext* render_ctx = (dmRender::RenderContext*) m_RenderContext;
    ASSERT_NE((void*)0, render_ctx);

    dmGameSystem::MaterialResource* material_res = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/material/light_buffer.materialc", (void**) &material_res));
    ASSERT_NE((void*)0, material_res);

    dmGameObject::HInstance proxy_go = Spawn(m_Factory, m_Collection, "/collection_proxy/ambient_light_root.goc", dmHashString64("/proxy"), 0,
                                              Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, proxy_go);

    CollectionProxyComponentRef proxy = GetCollectionProxyComponentRef(proxy_go, dmHashString64("collectionproxy"));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyLoad(proxy.m_World, proxy.m_Component, 0, 0));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyInitialize(proxy.m_World, proxy.m_Component));
    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyEnable(proxy.m_World, proxy.m_Component));

    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    dmRender::BeginFrame(m_RenderContext, 1.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material_res->m_Material);

    // The enabled proxy renders its child collection, which submits the ambient
    // and point instances before the light buffer snapshot is compacted.
    ASSERT_VEC3(Vector3(0.5f, 1.0f, 1.5f), render_ctx->m_AmbientLight);
    ASSERT_EQ(1u, render_ctx->m_LightBufferUploadScratch.Size());

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyDisable(proxy.m_World, proxy.m_Component));
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    dmRender::BeginFrame(m_RenderContext, 2.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material_res->m_Material);

    // The light instance still owns its latest data, but the disabled child is
    // not rendered and therefore does not submit the instance for this frame.
    ASSERT_VEC3(Vector3(0.0f, 0.0f, 0.0f), render_ctx->m_AmbientLight);
    ASSERT_EQ(0u, render_ctx->m_LightBufferUploadScratch.Size());

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameSystem::CompCollectionProxyEnable(proxy.m_World, proxy.m_Component));
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    dmRender::BeginFrame(m_RenderContext, 3.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material_res->m_Material);

    ASSERT_VEC3(Vector3(0.5f, 1.0f, 1.5f), render_ctx->m_AmbientLight);
    ASSERT_EQ(1u, render_ctx->m_LightBufferUploadScratch.Size());

    dmGameObject::Delete(m_Collection, proxy_go, true);
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    dmRender::BeginFrame(m_RenderContext, 4.0f, m_UpdateContext.m_DT);
    dmRender::RenderListBegin(m_RenderContext);
    ASSERT_TRUE(dmGameObject::Render(m_Collection));
    dmRender::RenderListEnd(m_RenderContext);
    dmRender::ApplyMaterialProgramLightBuffers(m_RenderContext, material_res->m_Material);

    ASSERT_VEC3(Vector3(0.0f, 0.0f, 0.0f), render_ctx->m_AmbientLight);
    ASSERT_EQ(0u, render_ctx->m_LightBufferUploadScratch.Size());

    dmResource::Release(m_Factory, (void*) material_res);
}

TEST_F(CollectionProxyComponentTest, ReleaseDynamicResourceFromAnotherCollection)
{
    // The proxy collection creates the resource, while this parent collection
    // releases it. Unloading the proxy must not leave stale resource bookkeeping.
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/collection_proxy/release_dynamic_resource_root.goc", dmHashString64("/go"), 0,
                                       Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    bool proxy_unloaded = false;
    for (uint32_t i = 0; i < 64 && !proxy_unloaded; ++i)
    {
        UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

        lua_getglobal(L, "issue_13002_proxy_unloaded");
        proxy_unloaded = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }

    ASSERT_TRUE(proxy_unloaded);
}

TEST_F(CollectionProxyComponentTest, CollectionProxyScriptLoadApi)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    const char* go_path = "/collection_proxy/script_load_api.goc";
    dmhash_t go_hash = dmHashString64("/go");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    bool callback1_ready = false;
    bool callback2_error = false;
    for (uint32_t i = 0; i < 64; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);

        lua_getglobal(L, "cp_script_load1_ready");
        callback1_ready = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
        
        lua_getglobal(L, "cp_script_load2_error");
        callback2_error = lua_toboolean(L, -1) != 0;
        lua_pop(L, 1);
    }

    ASSERT_TRUE(callback1_ready);
    ASSERT_TRUE(callback2_error);

    lua_getglobal(L, "cp_script_load_legacy_posted");
    ASSERT_TRUE(lua_toboolean(L, -1) != 0);
    lua_pop(L, 1);

    lua_getglobal(L, "cp_script_load1_error");
    ASSERT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "cp_script_load1_loading_count");
    ASSERT_GE((int)lua_tointeger(L, -1), 1);
    lua_pop(L, 1);

    lua_getglobal(L, "cp_script_load1_last_progress");
    ASSERT_NEAR(1.0f, (float)lua_tonumber(L, -1), 0.01f);
    lua_pop(L, 1);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);
}

// Uses script_load_cancel_requester.script
TEST_F(CollectionProxyComponentTest, CollectionProxyScriptLoadDeleteProxyWhileLoading)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameObject::HInstance proxy_go = Spawn(m_Factory, m_Collection, "/collection_proxy/script_load_cancel_proxy.goc", dmHashString64("/proxy"));
    ASSERT_NE(0, proxy_go);

    dmGameObject::HInstance requester_go = Spawn(m_Factory, m_Collection, "/collection_proxy/script_load_cancel_requester.goc", dmHashString64("/requester"));
    ASSERT_NE(0, requester_go);

    // Let the requester script start collectionproxy.load() against the proxy object.
    // The requester must stay alive after the proxy is deleted, otherwise script instance
    // cleanup would hide a leaked callback reference owned by the requester.
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    // assert that the init function of script_load_cancel_requester.script
    // has run successfully
    lua_getglobal(L, "cp_script_load_cancel_started");
    ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);

    // assert that the proxy started loading
    lua_getglobal(L, "cp_script_load_cancel_loading");
    ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);

    dmGameObject::Delete(m_Collection, proxy_go, true);
    // Flush the delete before another update can complete the preloader normally.
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGameObject::PostUpdate(m_Register);

    for (uint32_t i = 0; i < 4; ++i)
    {
        UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);
    }

    lua_gc(L, LUA_GCCOLLECT, 0);
    lua_gc(L, LUA_GCCOLLECT, 0);

    // The script stores a weak reference to the callback. If the engine still
    // holds m_AsyncLoadAndInitCallbackRef, the callback value remain reachable
    // after GC.
    lua_getglobal(L, "cp_script_load_cancel_refs");
    ASSERT_TRUE(lua_istable(L, -1));
    lua_getfield(L, -1, "callback");
    ASSERT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);

    // assert that the proxy did not finish loading
    lua_getglobal(L, "cp_script_load_cancel_ready");
    ASSERT_FALSE(lua_toboolean(L, -1));
    lua_pop(L, 1);

    // assert that no error occurred
    lua_getglobal(L, "cp_script_load_cancel_error");
    ASSERT_TRUE(lua_isnil(L, -1));
    lua_pop(L, 1);

    dmGameObject::Delete(m_Collection, requester_go, true);
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);
}

TEST_F(CollectionProxyComponentTest, CollectionProxySetCollectionRecursiveLoadInitialize)
{
    const char* go_path = "/collection_proxy/set_collection_cpp_cycle_proxy.goc";
    dmhash_t go_hash = dmHashString64("/go");
    dmhash_t proxy_component_hash = dmHashString64("collectionproxy");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, go_hash, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    dmLogInfo("collectionproxy cpp cycle root spawned");
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    CollectionProxyComponentRef root_proxy = GetCollectionProxyComponentRef(go, proxy_component_hash);
    dmLogInfo("collectionproxy cpp cycle root set_collection /collection_proxy/set_collection_cpp_cycle_level1.collectionc");
    ConfigureCollectionProxy(root_proxy, "/collection_proxy/set_collection_cpp_cycle_level1.collectionc");
    UpdateAndPostUpdateCollection(m_Collection, &m_UpdateContext, m_Register);

    dmGameObject::HCollection level1_collection = GetCollectionByName(m_Register, "set_collection_cpp_cycle_level1");
    dmLogInfo("collectionproxy cpp cycle level1 loaded and initialized");
    dmGameObject::HInstance level1_go = dmGameObject::GetInstanceFromIdentifier(level1_collection, dmHashString64("/go"));
    ASSERT_NE(0, level1_go);
    CollectionProxyComponentRef level1_proxy = GetCollectionProxyComponentRef(level1_go, proxy_component_hash);
    dmLogInfo("collectionproxy cpp cycle level1 set_collection /collection_proxy/set_collection_cpp_cycle_level2.collectionc");
    ConfigureCollectionProxy(level1_proxy, "/collection_proxy/set_collection_cpp_cycle_level2.collectionc");
    UpdateAndPostUpdateCollection(level1_collection, &m_UpdateContext, m_Register);

    dmGameObject::HCollection level2_collection = GetCollectionByName(m_Register, "set_collection_cpp_cycle_level2");
    dmLogInfo("collectionproxy cpp cycle level2 loaded and initialized");
    dmGameObject::HInstance level2_go = dmGameObject::GetInstanceFromIdentifier(level2_collection, dmHashString64("/go"));
    ASSERT_NE(0, level2_go);
    CollectionProxyComponentRef level2_proxy = GetCollectionProxyComponentRef(level2_go, proxy_component_hash);
    dmLogInfo("collectionproxy cpp cycle level2 set_collection /collection_proxy/set_collection_cpp_cycle_level3.collectionc");
    ConfigureCollectionProxy(level2_proxy, "/collection_proxy/set_collection_cpp_cycle_level3.collectionc");
    UpdateAndPostUpdateCollection(level2_collection, &m_UpdateContext, m_Register);

    dmGameObject::HCollection level3_collection = GetCollectionByName(m_Register, "set_collection_cpp_cycle_level3");
    dmLogInfo("collectionproxy cpp cycle level3 loaded and initialized");
    dmGameObject::HInstance level3_go = dmGameObject::GetInstanceFromIdentifier(level3_collection, dmHashString64("/go"));
    ASSERT_NE(0, level3_go);
    CollectionProxyComponentRef level3_proxy = GetCollectionProxyComponentRef(level3_go, proxy_component_hash);

    // Establish the back-edge in C++ without loading it.
    // Loading/enabling this final edge is what drives the recursion scenario.
    dmLogInfo("collectionproxy cpp cycle level3 set_collection /collection_proxy/set_collection_cpp_cycle_level1.collectionc");
    // Try to create a cyclic graph
    ConfigureCollectionProxy(level3_proxy, "/collection_proxy/set_collection_cpp_cycle_level1.collectionc", dmGameObject::RESULT_ALREADY_REGISTERED);

    ASSERT_NE(0, GetCollectionByName(m_Register, "set_collection_cpp_cycle_level1"));
    ASSERT_NE(0, GetCollectionByName(m_Register, "set_collection_cpp_cycle_level2"));
    ASSERT_NE(0, GetCollectionByName(m_Register, "set_collection_cpp_cycle_level3"));

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
    ASSERT_EQ(0, go);
}

void GetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t* out_val) {
    dmGameObject::PropertyDesc desc;
    dmGameObject::PropertyOptions opt;
    dmGameObject::PropertyResult r = dmGameObject::GetProperty(instance, comp_name, prop_name, opt, desc);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, r);
    dmGameObject::PropertyType type = desc.m_Variant.m_Type;
    ASSERT_TRUE(dmGameObject::PROPERTY_TYPE_HASH == type);
    *out_val = desc.m_Variant.m_Hash;
}

dmGameObject::PropertyResult SetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val) {
    dmGameObject::PropertyVar prop_var(in_val);
    dmGameObject::PropertyOptions opt;
    return dmGameObject::SetProperty(instance, comp_name, prop_name, opt, prop_var);
}

dmhash_t GetHashProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, const dmGameObject::PropertyOptions* options)
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

dmGameObject::PropertyResult SetHashProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val, const dmGameObject::PropertyOptions* options)
{
    dmGameObject::PropertyVar prop_var(in_val);
    dmGameObject::PropertyOptions default_options;
    return dmGameObject::SetProperty(instance, comp_name, prop_name, options ? *options : default_options, prop_var);
}

void RenderCollection(dmRender::HRenderContext render_context, dmGameObject::HCollection collection)
{
    dmRender::RenderListBegin(render_context);
    dmGameObject::Render(collection);
    dmRender::RenderListEnd(render_context);
    dmRender::DrawRenderList(render_context, 0x0, 0x0, 0x0, dmRender::SORT_BACK_TO_FRONT);
    dmRender::ClearRenderObjects(render_context);
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

const char* valid_meshset_resources[] = {"/meshset/valid_gltf.meshsetc", "/meshset/valid_gltf.skeletonc", "/meshset/valid_gltf_generated_0.animationsetc"};
INSTANTIATE_TEST_CASE_P(MeshSet, ResourceTest, jc_test_values_in(valid_meshset_resources));

ResourceFailParams invalid_mesh_resources[] =
{
    {"/meshset/valid_gltf.meshsetc", "/meshset/missing.meshsetc"},
    {"/meshset/valid_gltf.skeletonc", "/meshset/missing.skeletonc"},
    {"/meshset/valid_gltf_generated_0.animationsetc", "/meshset/missing.animationsetc"},
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
    "/light/valid_point.point_light.lightc",
    "/light/valid_directional_light.directional_light.lightc",
    "/light/valid_spot_light.spot_light.lightc",
    "/light/valid_ambient_light.ambient_light.lightc"
};
INSTANTIATE_TEST_CASE_P(Light, ResourceTest, jc_test_values_in(valid_light_resources));

ResourceFailParams invalid_light_resources[] =
{
    {"/light/valid_point.point_light.lightc", "/light/invalid_point_missing_range.point_light.lightc"},
    {"/light/valid_directional_light.directional_light.lightc", "/light/invalid_directional_missing_intensity.directional_light.lightc"},
    {"/light/valid_spot_light.spot_light.lightc", "/light/invalid_spot_missing_outer_cone_angle.spot_light.lightc"},
    {"/light/valid_ambient_light.ambient_light.lightc", "/light/invalid_ambient_missing_intensity.ambient_light.lightc"}
};
INSTANTIATE_TEST_CASE_P(Light, ResourceFailTest, jc_test_values_in(invalid_light_resources));

const char* valid_light_gos[] = {
    "/light/valid_point_light.goc",
    "/light/valid_directional_light.goc",
    "/light/valid_spot_light.goc",
    "/light/valid_ambient_light.goc"
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
    {"image", "/resource/tileset.t.texturesetc", "", "/resource/res_getset_prop.goc", "sprite", 0},
    {"texture0", "/resource/tile_mario_tileset.texturec", "/not_found.texturec", "/resource/res_getset_prop.goc", "model", 0},
    {"texture1", "/resource/tile_mario_tileset.texturec", "/not_found.texturec", "/resource/res_getset_prop.goc", "model", 0},
    {"tile_source", "/resource/tileset.t.texturesetc", "", "/resource/res_getset_prop.goc", "tilemap", 0},
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
