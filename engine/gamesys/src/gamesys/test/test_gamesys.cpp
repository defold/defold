#include "test_gamesys.h"

#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/time.h>

#include <gameobject/gameobject_ddf.h>

const char* ROOT = "build/default/src/gamesys/test";

bool CopyResource(const char* src, const char* dst)
{
    char src_path[128];
    DM_SNPRINTF(src_path, sizeof(src_path), "%s/%s", ROOT, src);
    FILE* src_f = fopen(src_path, "rb");
    if (src_f == 0x0)
        return false;
    char dst_path[128];
    DM_SNPRINTF(dst_path, sizeof(dst_path), "%s/%s", ROOT, dst);
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
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, name);
    return unlink(path) == 0;
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

    uint64_t stop_time = dmTime::GetTime() + 30*10e6;
    while (dmTime::GetTime() < stop_time)
    {
        // Simulate running at 30fps
        r = dmResource::UpdatePreloader(pr, 33*1000);
        if (r != dmResource::RESULT_PENDING)
            break;
        dmTime::Sleep(33*1000);
    }

    ASSERT_EQ(dmResource::RESULT_OK, r);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, resource_name, &resource));

    dmResource::DeletePreloader(pr);
    dmResource::Release(m_Factory, resource);
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
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, go_name);
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
    DM_SNPRINTF(path, sizeof(path), "%s/%s", ROOT, go_name);
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

TEST_P(ComponentFailTest, Test)
{
    const char* go_name = GetParam();

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_EQ((void*)0, go);
}

/* Camera */

const char* valid_camera_resources[] = {"/camera/valid.camerac"};
INSTANTIATE_TEST_CASE_P(Camera, ResourceTest, ::testing::ValuesIn(valid_camera_resources));

ResourceFailParams invalid_camera_resources[] =
{
    {"/camera/valid.camerac", "/camera/missing.camerac"},
};
INSTANTIATE_TEST_CASE_P(Camera, ResourceFailTest, ::testing::ValuesIn(invalid_camera_resources));

const char* valid_camera_gos[] = {"/camera/valid_camera.goc"};
INSTANTIATE_TEST_CASE_P(Camera, ComponentTest, ::testing::ValuesIn(valid_camera_gos));

const char* invalid_camera_gos[] = {"/camera/invalid_camera.goc"};
INSTANTIATE_TEST_CASE_P(Camera, ComponentFailTest, ::testing::ValuesIn(invalid_camera_gos));

/* Collection Proxy */

const char* valid_collection_proxy_resources[] = {"/collection_proxy/valid.collectionproxyc"};
INSTANTIATE_TEST_CASE_P(CollectionProxy, ResourceTest, ::testing::ValuesIn(valid_collection_proxy_resources));

const char* valid_collection_proxy_gos[] = {"/collection_proxy/valid_collection_proxy.goc"};
INSTANTIATE_TEST_CASE_P(CollectionProxy, ComponentTest, ::testing::ValuesIn(valid_collection_proxy_gos));

/* Collision Object */

const char* valid_collision_object_resources[] = {"/collision_object/valid.collisionobjectc",
                                                  "/collision_object/valid_tilegrid.collisionobjectc",
                                                  "/collision_object/embedded_shapes.collisionobjectc" };

INSTANTIATE_TEST_CASE_P(CollisionObject, ResourceTest, ::testing::ValuesIn(valid_collision_object_resources));

ResourceFailParams invalid_collision_object_resources[] =
{
    {"/collision_object/valid.collisionobjectc", "/collision_object/missing.collisionobjectc"},
    {"/collision_object/embedded_shapes.collisionobjectc", "/collision_object/invalid_embedded_shapes.collisionobjectc"},
};
INSTANTIATE_TEST_CASE_P(CollisionObject, ResourceFailTest, ::testing::ValuesIn(invalid_collision_object_resources));

const char* valid_collision_object_gos[] = {"/collision_object/valid_collision_object.goc", "/collision_object/valid_tilegrid.goc"};
INSTANTIATE_TEST_CASE_P(CollisionObject, ComponentTest, ::testing::ValuesIn(valid_collision_object_gos));

const char* invalid_collision_object_gos[] =
{
    "/collision_object/invalid_shape.goc"
};
INSTANTIATE_TEST_CASE_P(CollisionObject, ComponentFailTest, ::testing::ValuesIn(invalid_collision_object_gos));

/* Convex Shape */

const char* valid_cs_resources[] =
{
    "/convex_shape/box.convexshapec",
    /*"/convex_shape/capsule.convexshapec",*/ // Temporarily disabling capsule since we are more interested in 2D atm
    "/convex_shape/hull.convexshapec",
    "/convex_shape/sphere.convexshapec",
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceTest, ::testing::ValuesIn(valid_cs_resources));

ResourceFailParams invalid_cs_resources[] =
{
    {"/convex_shape/box.convexshapec", "/convex_shape/invalid_box.convexshapec"},
    {"/convex_shape/capsule.convexshapec", "/convex_shape/invalid_capsule.convexshapec"},
    {"/convex_shape/hull.convexshapec", "/convex_shape/invalid_hull.convexshapec"},
    {"/convex_shape/sphere.convexshapec", "/convex_shape/invalid_sphere.convexshapec"},
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceFailTest, ::testing::ValuesIn(invalid_cs_resources));

/* Emitter */

const char* valid_emitter_resources[] = {"/emitter/valid.emitterc"};
INSTANTIATE_TEST_CASE_P(Emitter, ResourceTest, ::testing::ValuesIn(valid_emitter_resources));

const char* valid_emitter_gos[] = {"/emitter/valid_emitter.goc"};
INSTANTIATE_TEST_CASE_P(Emitter, ComponentTest, ::testing::ValuesIn(valid_emitter_gos));

/* Font map */

const char* valid_font_resources[] = {"/font/valid_font.fontc"};
INSTANTIATE_TEST_CASE_P(FontMap, ResourceTest, ::testing::ValuesIn(valid_font_resources));

ResourceFailParams invalid_font_resources[] =
{
    {"/font/valid_font.fontc", "/font/missing.fontc"},
    {"/font/valid_font.fontc", "/font/invalid_material.fontc"},
};
INSTANTIATE_TEST_CASE_P(FontMap, ResourceFailTest, ::testing::ValuesIn(invalid_font_resources));

/* Fragment Program */

const char* valid_fp_resources[] = {"/fragment_program/valid.fpc"};
INSTANTIATE_TEST_CASE_P(FragmentProgram, ResourceTest, ::testing::ValuesIn(valid_fp_resources));

ResourceFailParams invalid_fp_resources[] =
{
    {"/fragment_program/valid.fpc", "/fragment_program/missing.fpc"},
};
INSTANTIATE_TEST_CASE_P(FragmentProgram, ResourceFailTest, ::testing::ValuesIn(invalid_fp_resources));

/* Gui Script */

const char* valid_gs_resources[] = {"/gui/valid.gui_scriptc"};
INSTANTIATE_TEST_CASE_P(GuiScript, ResourceTest, ::testing::ValuesIn(valid_gs_resources));

ResourceFailParams invalid_gs_resources[] =
{
    {"/gui/valid.gui_scriptc", "/gui/missing.gui_scriptc"},
    {"/gui/valid.gui_scriptc", "/gui/missing_module.gui_scriptc"},
};
INSTANTIATE_TEST_CASE_P(GuiScript, ResourceFailTest, ::testing::ValuesIn(invalid_gs_resources));

/* Gui */

const char* valid_gui_resources[] = {"/gui/valid.guic"};
INSTANTIATE_TEST_CASE_P(Gui, ResourceTest, ::testing::ValuesIn(valid_gui_resources));

ResourceFailParams invalid_gui_resources[] =
{
    {"/gui/valid.guic", "/gui/missing.guic"},
    {"/gui/valid.guic", "/gui/invalid_font.guic"},
};
INSTANTIATE_TEST_CASE_P(Gui, ResourceFailTest, ::testing::ValuesIn(invalid_gui_resources));

const char* valid_gui_gos[] = {"/gui/valid_gui.goc"};
INSTANTIATE_TEST_CASE_P(Gui, ComponentTest, ::testing::ValuesIn(valid_gui_gos));

const char* invalid_gui_gos[] =
{
    "/gui/invalid_font.goc"
};
INSTANTIATE_TEST_CASE_P(Gui, ComponentFailTest, ::testing::ValuesIn(invalid_gui_gos));

/* Input Binding */

const char* valid_input_resources[] = {"/input/valid.input_bindingc"};
INSTANTIATE_TEST_CASE_P(InputBinding, ResourceTest, ::testing::ValuesIn(valid_input_resources));

ResourceFailParams invalid_input_resources[] =
{
    {"/input/valid.input_bindingc", "/input/missing.input_bindingc"},
};
INSTANTIATE_TEST_CASE_P(InputBinding, ResourceFailTest, ::testing::ValuesIn(invalid_input_resources));

/* Light */

const char* valid_light_resources[] = {"/light/valid.lightc"};
INSTANTIATE_TEST_CASE_P(Light, ResourceTest, ::testing::ValuesIn(valid_light_resources));

ResourceFailParams invalid_light_resources[] =
{
    {"/light/valid.lightc", "/light/missing.lightc"},
};
INSTANTIATE_TEST_CASE_P(Light, ResourceFailTest, ::testing::ValuesIn(invalid_light_resources));

const char* valid_light_gos[] = {"/light/valid_light.goc"};
INSTANTIATE_TEST_CASE_P(Light, ComponentTest, ::testing::ValuesIn(valid_light_gos));

const char* invalid_light_gos[] = {"/light/invalid_light.goc"};
INSTANTIATE_TEST_CASE_P(Light, ComponentFailTest, ::testing::ValuesIn(invalid_light_gos));

/* Material */

const char* valid_material_resources[] = {"/material/valid.materialc"};
INSTANTIATE_TEST_CASE_P(Material, ResourceTest, ::testing::ValuesIn(valid_material_resources));

ResourceFailParams invalid_material_resources[] =
{
    {"/material/valid.materialc", "/material/missing.materialc"},
    {"/material/valid.materialc", "/material/missing_name.materialc"},
};
INSTANTIATE_TEST_CASE_P(Material, ResourceFailTest, ::testing::ValuesIn(invalid_material_resources));

/* Mesh */

const char* valid_mesh_resources[] = {"/mesh/valid.meshc"};
INSTANTIATE_TEST_CASE_P(Mesh, ResourceTest, ::testing::ValuesIn(valid_mesh_resources));

ResourceFailParams invalid_mesh_resources[] =
{
    {"/mesh/valid.meshc", "/mesh/missing.meshc"},
};
INSTANTIATE_TEST_CASE_P(Mesh, ResourceFailTest, ::testing::ValuesIn(invalid_mesh_resources));

/* Model */

const char* valid_model_resources[] = {"/model/valid.modelc"};
INSTANTIATE_TEST_CASE_P(Model, ResourceTest, ::testing::ValuesIn(valid_model_resources));

ResourceFailParams invalid_model_resources[] =
{
    {"/model/valid.modelc", "/model/missing.modelc"},
    {"/model/valid.modelc", "/model/invalid_material.modelc"},
};
INSTANTIATE_TEST_CASE_P(Model, ResourceFailTest, ::testing::ValuesIn(invalid_model_resources));

const char* valid_model_gos[] = {"/model/valid_model.goc"};
INSTANTIATE_TEST_CASE_P(Model, ComponentTest, ::testing::ValuesIn(valid_model_gos));

const char* invalid_model_gos[] = {"/model/invalid_model.goc", "/model/invalid_material.goc"};
INSTANTIATE_TEST_CASE_P(Model, ComponentFailTest, ::testing::ValuesIn(invalid_model_gos));

/* Particle FX */

const char* valid_particlefx_resources[] = {"/particlefx/valid.particlefxc"};
INSTANTIATE_TEST_CASE_P(ParticleFX, ResourceTest, ::testing::ValuesIn(valid_particlefx_resources));

ResourceFailParams invalid_particlefx_resources[] =
{
    {"/particlefx/valid.particlefxc", "/particlefx/invalid_material.particlefxc"},
};
INSTANTIATE_TEST_CASE_P(ParticleFX, ResourceFailTest, ::testing::ValuesIn(invalid_particlefx_resources));

const char* valid_particlefx_gos[] = {"/particlefx/valid_particlefx.goc"};
INSTANTIATE_TEST_CASE_P(ParticleFX, ComponentTest, ::testing::ValuesIn(valid_particlefx_gos));

const char* invalid_particlefx_gos[] =
{
    "/particlefx/invalid_material.goc",
    "/particlefx/invalid_texture.goc"
};
INSTANTIATE_TEST_CASE_P(ParticleFX, ComponentFailTest, ::testing::ValuesIn(invalid_particlefx_gos));

/* Render */

const char* valid_render_resources[] = {"/render/valid.renderc"};
INSTANTIATE_TEST_CASE_P(Render, ResourceTest, ::testing::ValuesIn(valid_render_resources));

ResourceFailParams invalid_render_resources[] =
{
    {"/render/valid.renderc", "/render/missing.renderc"},
    {"/render/valid.renderc", "/render/invalid_material.renderc"},
};
INSTANTIATE_TEST_CASE_P(Render, ResourceFailTest, ::testing::ValuesIn(invalid_render_resources));

/* Render Script */

const char* valid_rs_resources[] = {"/render_script/valid.render_scriptc"};
INSTANTIATE_TEST_CASE_P(RenderScript, ResourceTest, ::testing::ValuesIn(valid_rs_resources));

ResourceFailParams invalid_rs_resources[] =
{
    {"/render_script/valid.render_scriptc", "/render_script/missing.render_scriptc"},
};
INSTANTIATE_TEST_CASE_P(RenderScript, ResourceFailTest, ::testing::ValuesIn(invalid_rs_resources));

/* Display Profiles */

const char* valid_dp_resources[] = {"/display_profiles/valid.display_profilesc"};
INSTANTIATE_TEST_CASE_P(DisplayProfiles, ResourceTest, ::testing::ValuesIn(valid_dp_resources));

ResourceFailParams invalid_dp_resources[] =
{
    {"/display_profiles/valid.display_profilesc", "/display_profiles/missing.display_profilesc"},
};
INSTANTIATE_TEST_CASE_P(DisplayProfiles, ResourceFailTest, ::testing::ValuesIn(invalid_dp_resources));

/* Script */

const char* valid_script_resources[] = {"/script/valid.scriptc"};
INSTANTIATE_TEST_CASE_P(Script, ResourceTest, ::testing::ValuesIn(valid_script_resources));

ResourceFailParams invalid_script_resources[] =
{
    {"/script/valid.scriptc", "/script/missing.scriptc"},
};
INSTANTIATE_TEST_CASE_P(Script, ResourceFailTest, ::testing::ValuesIn(invalid_script_resources));

const char* valid_script_gos[] = {"/script/valid_script.goc"};
INSTANTIATE_TEST_CASE_P(Script, ComponentTest, ::testing::ValuesIn(valid_script_gos));

const char* invalid_script_gos[] = {"/script/missing_script.goc", "/script/invalid_script.goc"};
INSTANTIATE_TEST_CASE_P(Script, ComponentFailTest, ::testing::ValuesIn(invalid_script_gos));

/* Sound */

const char* valid_sound_resources[] = {"/sound/valid.soundc"};
INSTANTIATE_TEST_CASE_P(Sound, ResourceTest, ::testing::ValuesIn(valid_sound_resources));

ResourceFailParams invalid_sound_resources[] =
{
    {"/sound/valid.soundc", "/sound/missing.soundc"},
};
INSTANTIATE_TEST_CASE_P(Sound, ResourceFailTest, ::testing::ValuesIn(invalid_sound_resources));

const char* valid_sound_gos[] = {"/sound/valid_sound.goc"};
INSTANTIATE_TEST_CASE_P(Sound, ComponentTest, ::testing::ValuesIn(valid_sound_gos));

const char* invalid_sound_gos[] = {"/sound/invalid_sound.goc", "/sound/invalid_sound.goc"};
INSTANTIATE_TEST_CASE_P(Sound, ComponentFailTest, ::testing::ValuesIn(invalid_sound_gos));

/* Factory */

const char* valid_sp_resources[] = {"/factory/valid.factoryc"};
INSTANTIATE_TEST_CASE_P(Factory, ResourceTest, ::testing::ValuesIn(valid_sp_resources));

ResourceFailParams invalid_sp_resources[] =
{
    {"/factory/valid.factoryc", "/factory/missing.factoryc"},
};
INSTANTIATE_TEST_CASE_P(Factory, ResourceFailTest, ::testing::ValuesIn(invalid_sp_resources));

const char* valid_sp_gos[] = {"/factory/valid_factory.goc"};
INSTANTIATE_TEST_CASE_P(Factory, ComponentTest, ::testing::ValuesIn(valid_sp_gos));

const char* invalid_sp_gos[] = {"/factory/invalid_factory.goc"};
INSTANTIATE_TEST_CASE_P(Factory, ComponentFailTest, ::testing::ValuesIn(invalid_sp_gos));

/* Sprite */

const char* valid_sprite_resources[] = {"/sprite/valid.spritec"};
INSTANTIATE_TEST_CASE_P(Sprite, ResourceTest, ::testing::ValuesIn(valid_sprite_resources));

ResourceFailParams invalid_sprite_resources[] =
{
    {"/sprite/valid.spritec", "/sprite/invalid_animation.spritec"},
};
INSTANTIATE_TEST_CASE_P(Sprite, ResourceFailTest, ::testing::ValuesIn(invalid_sprite_resources));

const char* valid_sprite_gos[] = {"/sprite/valid_sprite.goc"};
INSTANTIATE_TEST_CASE_P(Sprite, ComponentTest, ::testing::ValuesIn(valid_sprite_gos));

const char* invalid_sprite_gos[] = {"/sprite/invalid_sprite.goc"};
INSTANTIATE_TEST_CASE_P(Sprite, ComponentFailTest, ::testing::ValuesIn(invalid_sprite_gos));

/* TileSet */
const char* valid_tileset_resources[] = {"/tile/valid.texturesetc"};
INSTANTIATE_TEST_CASE_P(TileSet, ResourceTest, ::testing::ValuesIn(valid_tileset_resources));

/* TileGrid */
const char* valid_tilegrid_resources[] = {"/tile/valid.tilegridc"};
INSTANTIATE_TEST_CASE_P(TileGrid, ResourceTest, ::testing::ValuesIn(valid_tilegrid_resources));

const char* valid_tileset_gos[] = {"/tile/valid_tilegrid.goc", "/tile/valid_tilegrid_collisionobject.goc"};
INSTANTIATE_TEST_CASE_P(TileSet, ComponentTest, ::testing::ValuesIn(valid_tileset_gos));

/* Texture */

const char* valid_texture_resources[] = {"/texture/valid_png.texturec", "/texture/blank_4096_png.texturec"};
INSTANTIATE_TEST_CASE_P(Texture, ResourceTest, ::testing::ValuesIn(valid_texture_resources));

ResourceFailParams invalid_texture_resources[] =
{
    {"/texture/valid_png.texturec", "/texture/missing.texturec"},
};
INSTANTIATE_TEST_CASE_P(Texture, ResourceFailTest, ::testing::ValuesIn(invalid_texture_resources));

/* Vertex Program */

const char* valid_vp_resources[] = {"/vertex_program/valid.vpc"};
INSTANTIATE_TEST_CASE_P(VertexProgram, ResourceTest, ::testing::ValuesIn(valid_vp_resources));

ResourceFailParams invalid_vp_resources[] =
{
    {"/vertex_program/valid.vpc", "/vertex_program/missing.vpc"},
};
INSTANTIATE_TEST_CASE_P(VertexProgram, ResourceFailTest, ::testing::ValuesIn(invalid_vp_resources));

/* Spine Scene */

const char* valid_spine_scene_resources[] = {"/spine/valid.spinescenec"};
INSTANTIATE_TEST_CASE_P(SpineScene, ResourceTest, ::testing::ValuesIn(valid_spine_scene_resources));

/* Spine Model */

const char* valid_spine_model_resources[] = {"/spine/valid.spinemodelc"};
INSTANTIATE_TEST_CASE_P(SpineModel, ResourceTest, ::testing::ValuesIn(valid_spine_model_resources));

const char* valid_spine_gos[] = {"/spine/valid_spine.goc"};
INSTANTIATE_TEST_CASE_P(SpineModel, ComponentTest, ::testing::ValuesIn(valid_spine_gos));

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
