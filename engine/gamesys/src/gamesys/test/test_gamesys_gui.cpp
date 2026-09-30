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

using namespace dmVMath;

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

static dmGameSystem::LabelComponent* GetLabelComponent(dmGameObject::HInstance instance, dmhash_t component_id)
{
    uint32_t component_type = 0;
    dmGameObject::HComponent component = 0;
    dmGameObject::HComponentWorld world = 0;
    EXPECT_EQ(dmGameObject::RESULT_OK, dmGameObject::GetComponent(instance, component_id, &component_type, &component, &world));
    EXPECT_NE((void*)0, component);
    return (dmGameSystem::LabelComponent*) component;
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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);
}

// Tests the animation done message/callback
TEST_F(GuiTest, GuiFlipbookAnim)
{
    dmhash_t go_id = dmHashString64("/go");
    dmhash_t gui_comp_id = dmHashString64("gui");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_flipbook_anim.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/gui/atlas_valid.a.texturesetc", (void**) &valid_atlas));
    ASSERT_TRUE(valid_atlas != 0x0);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/gui/texture_valid_png.texturec", (void**) &valid_texture));
    ASSERT_TRUE(valid_atlas != 0x0);

    dmGraphics::HTexture valid_atlas_th = valid_atlas->m_Texture->m_Texture;
    dmGraphics::HTexture valid_texture_th = valid_texture->m_Texture;

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/texture_resources_texture_resources.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/gui/texture_setter_override_b.a.texturesetc", (void**)&expected_atlas));
    ASSERT_NE((void*)0x0, expected_atlas);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/texture_setter_override.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/gui/texture_setter_override_a.a.texturesetc", (void**)&expected_atlas));
    ASSERT_NE((void*)0x0, expected_atlas);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/texture_reload_override.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

// Verifies that atlas resources assigned through gui.set(msg.url(), "textures", ...)
// can be replaced and then explicitly removed. This guards the component-owned
// resource references so script-side resource.release() can fully destroy the
// old and current runtime atlases.
TEST_F(GuiTest, GuiSetNilRemovesRuntimeTextureMapping)
{
    const char* atlas_a_path = "/gui/set_texture_nil_a.texturesetc";
    const char* atlas_b_path = "/gui/set_texture_nil_b.texturesetc";
    const dmhash_t atlas_a_hash = dmHashString64(atlas_a_path);
    const dmhash_t atlas_b_hash = dmHashString64(atlas_b_path);
    const dmhash_t texture_a_hash = dmHashString64("/gui/set_texture_nil_a.texturec");
    const dmhash_t texture_b_hash = dmHashString64("/gui/set_texture_nil_b.texturec");

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_set_texture_nil.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    uint32_t component_type_index        = dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("guic"));
    dmGameSystem::GuiWorld* gui_world    = (dmGameSystem::GuiWorld*) dmGameObject::GetWorld(m_Collection, component_type_index);
    dmGameSystem::GuiComponent* gui_comp = gui_world->m_Components[0];
    dmGui::HNode box = dmGui::GetNodeById(gui_comp->m_Scene, "box");
    ASSERT_NE(0, box);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, atlas_a_hash));

    dmGameSystem::TextureSetResource* atlas_a = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, atlas_a_path, (void**)&atlas_a));
    ASSERT_NE((void*)0x0, atlas_a);
    ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, atlas_a_hash));

    dmGui::NodeTextureType texture_type;
    dmGui::HTextureSource texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box, &texture_type);
    ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, texture_type);
    ASSERT_EQ((dmGui::HTextureSource) atlas_a, texture_source);

    dmResource::Release(m_Factory, atlas_a);
    ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, atlas_a_hash));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, atlas_a_hash));
    ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, atlas_b_hash));

    dmGameSystem::TextureSetResource* atlas_b = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, atlas_b_path, (void**)&atlas_b));
    ASSERT_NE((void*)0x0, atlas_b);
    ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, atlas_b_hash));

    texture_source = dmGui::GetNodeTexture(gui_comp->m_Scene, box, &texture_type);
    ASSERT_EQ(dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, texture_type);
    ASSERT_EQ((dmGui::HTextureSource) atlas_b, texture_source);

    dmResource::Release(m_Factory, atlas_b);
    ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, atlas_b_hash));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    lua_State* L = m_Scriptlibcontext.m_LuaState;
    lua_getglobal(L, "gui_set_texture_nil_done");
    bool done = lua_toboolean(L, -1);
    lua_pop(L, 1);
    ASSERT_TRUE(done);

    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, atlas_a_hash));
    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, atlas_b_hash));
    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, texture_a_hash));
    ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, texture_b_hash));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, AsyncTextureAutoSize)
{
    dmGraphics::NullContext* null_context = (dmGraphics::NullContext*) m_GraphicsContext;
    null_context->m_UseAsyncTextureLoad = 1;

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/async_texture_auto_size.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(UpdateAndWaitUntilDone(m_Scriptlibcontext, m_Collection, &m_UpdateContext, false, "tests_done"));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Tests creating and deleting dynamic textures
TEST_F(GuiTest, MaxDynamictextures)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_max_dynamic_textures.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
TEST_F(GuiResourceTest, ScriptSetFonts)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/goscript.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

TEST_F(FontTest, GlyphBankRecreateKeepsFontHandle)
{
    dmGameSystem::FontResource* font_1;
    dmGameSystem::FontResource* font_2;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/glyph_bank_test_1.fontc", (void**)&font_1));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/glyph_bank_test_2.fontc", (void**)&font_2));

    HFont    hfont_1 = dmGameSystem::GetFont(font_1->m_GlyphBankResource);
    HFont    hfont_2 = dmGameSystem::GetFont(font_2->m_GlyphBankResource);
    uint32_t glyph_index_1 = FontGetGlyphIndex(hfont_1, 'A');
    uint32_t glyph_index_2 = FontGetGlyphIndex(hfont_2, 'A');
    ASSERT_NE(0U, glyph_index_1);
    ASSERT_NE(0U, glyph_index_2);

    FontGlyphOptions options = {};
    options.m_GenerateImage = true;
    FontGlyph original_glyph;
    FontGlyph replacement_glyph;
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(hfont_1, glyph_index_1, &options, &original_glyph));
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(hfont_2, glyph_index_2, &options, &replacement_glyph));

    const char* replacement_path = font_2->m_DDF->m_GlyphBank;
    char        replacement_host_path[256];
    dmTestUtil::MakeHostPathf(replacement_host_path, sizeof(replacement_host_path), "build/src/gamesys/test/%s%s", GetContentFolder(), replacement_path);
    uint32_t replacement_size = 0;
    uint8_t* replacement_data = dmTestUtil::ReadFile(replacement_host_path, &replacement_size);
    ASSERT_NE((uint8_t*)0, replacement_data);

    const char*         target_path = font_1->m_DDF->m_GlyphBank;
    HResourceDescriptor descriptor = dmResource::FindByHash(m_Factory, dmHashString64(target_path));
    ASSERT_NE((HResourceDescriptor)0, descriptor);
    HResourceType resource_type;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::GetTypeFromExtension(m_Factory, "glyph_bankc", &resource_type));

    dmResource::ResourceRecreateParams params = {};
    params.m_Factory = m_Factory;
    params.m_Type = resource_type;
    params.m_FilenameHash = dmHashString64(target_path);
    params.m_Filename = target_path;
    params.m_Buffer = replacement_data;
    params.m_BufferSize = replacement_size;
    params.m_FileSize = replacement_size;
    params.m_Resource = descriptor;
    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResGlyphBankRecreate(&params));
    dmMemory::AlignedFree(replacement_data);

    FontGlyph recreated_glyph;
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(hfont_1, FontGetGlyphIndex(hfont_1, 'A'), &options, &recreated_glyph));
    ASSERT_NE(original_glyph.m_Bitmap.m_Data, recreated_glyph.m_Bitmap.m_Data);
    ASSERT_EQ(replacement_glyph.m_Bitmap.m_DataSize, recreated_glyph.m_Bitmap.m_DataSize);
    ASSERT_EQ(0, memcmp(replacement_glyph.m_Bitmap.m_Data, recreated_glyph.m_Bitmap.m_Data, recreated_glyph.m_Bitmap.m_DataSize));

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

struct DynamicFontJobCallbackState
{
    uint32_t                    m_CallbackCount;
    int                         m_Result;
    char                        m_ErrMsg[128];
    dmResource::HFactory        m_Factory;
    const char*                 m_Path;
    uint32_t                    m_ReloadCount;
    dmResource::Result          m_ReloadResult;
};

static void DynamicFontJobCallback(void* ctx, int result, const char* errmsg)
{
    DynamicFontJobCallbackState* state = (DynamicFontJobCallbackState*)ctx;
    state->m_CallbackCount++;
    state->m_Result = result;
    dmStrlCpy(state->m_ErrMsg, errmsg ? errmsg : "", sizeof(state->m_ErrMsg));
}

static void DynamicFontJobReloadCallback(void* ctx, int result, const char* errmsg)
{
    DynamicFontJobCallback(ctx, result, errmsg);

    DynamicFontJobCallbackState* state = (DynamicFontJobCallbackState*)ctx;
    if (state->m_ReloadCount == 0)
    {
        state->m_ReloadCount++;
        state->m_ReloadResult = dmResource::ReloadResource(state->m_Factory, state->m_Path, 0);
    }
}

static bool WaitForDynamicFontJobCallbacks(HJobContext job_context, DynamicFontJobCallbackState* state, uint32_t callback_count)
{
    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;
    while (state->m_CallbackCount < callback_count && dmTime::GetMonotonicTime() < stop_time)
    {
        JobSystemUpdate(job_context, 0);
        dmTime::Sleep(1000);
    }
    return state->m_CallbackCount >= callback_count;
}

TEST_F(FontTest, DynamicFontPrewarmedGlyphsFitCacheRows)
{
    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_negative_ascent.fontc", (void**)&font));
    dmRender::HFontMap font_map = dmGameSystem::ResFontGetHandle(font);
    HFont hfont = FontCollectionGetFont(dmRender::GetFontCollection(font_map), 0);
    dmRender::UpdateCacheTexture(font_map);

    bool used_next_row = false;
    for (uint32_t codepoint = '!'; codepoint <= '~'; ++codepoint)
    {
        FontGlyph* glyph = 0;
        ASSERT_EQ(FONT_RESULT_OK, GetGlyph(font_map, hfont, codepoint, &glyph));
        ASSERT_NE((FontGlyph*)0, glyph);
        if (codepoint == '_')
            ASSERT_EQ(-3.0f, glyph->m_Ascent);
        if (glyph->m_Bitmap.m_Height == 0)
            continue;

        int32_t offset_y = font_map->m_CacheCellMaxAscent - (int32_t)glyph->m_Ascent;
        ASSERT_GE(offset_y, 0);
        ASSERT_LE(offset_y + glyph->m_Bitmap.m_Height, font_map->m_CacheCellHeight);
        ASSERT_LE(glyph->m_Bitmap.m_Width, font_map->m_CacheCellWidth);
        uint64_t key = dmRender::MakeGlyphIndexKey(hfont, glyph->m_GlyphIndex);
        dmRender::CacheGlyph* cached = dmRender::AddGlyphToCache(font_map, 1, key, glyph, offset_y);
        ASSERT_NE((dmRender::CacheGlyph*)0, cached);
        used_next_row |= cached->m_Y > 0;
    }
    ASSERT_TRUE(used_next_row);
    dmResource::Release(m_Factory, font);
}

TEST_F(FontTest, DynamicFontFirstBitmapHasNegativeAscent)
{
    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_negative_ascent_empty.fontc", (void**)&font));
    dmRender::HFontMap font_map = dmGameSystem::ResFontGetHandle(font);
    HFont hfont = FontCollectionGetFont(dmRender::GetFontCollection(font_map), 0);
    ASSERT_EQ(0u, font_map->m_Glyphs.Size());
    ASSERT_EQ(0, font_map->m_CacheCellHeight);
    ASSERT_EQ(0, font_map->m_CacheCellCount);
    ASSERT_EQ((dmRender::CacheGlyph*)0, font_map->m_Cache);

    // A space has layout metrics but no bitmap, so it must not establish the cache baseline.
    DynamicFontJobCallbackState space_callback_state = {};
    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontPrewarmText(font, " ", DynamicFontJobCallback, &space_callback_state));
    ASSERT_TRUE(WaitForDynamicFontJobCallbacks(m_JobContext, &space_callback_state, 1));
    ASSERT_EQ(1, space_callback_state.m_Result);
    FontGlyph* space = 0;
    ASSERT_EQ(FONT_RESULT_OK, GetGlyph(font_map, hfont, ' ', &space));
    ASSERT_NE((FontGlyph*)0, space);
    ASSERT_EQ(0, space->m_Bitmap.m_Height);
    ASSERT_EQ(0, font_map->m_CacheCellHeight);
    uint64_t space_key = dmRender::MakeGlyphIndexKey(hfont, space->m_GlyphIndex);
    ASSERT_EQ((dmRender::CacheGlyph*)0, dmRender::AddGlyphToCache(font_map, 0, space_key, space, 0));

    const char* texts[] = {"_", "I", "\xc3\x85"};
    const uint32_t codepoints[] = {'_', 'I', 0xc5};
    FontGlyph* glyphs[3];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(texts); ++i)
    {
        DynamicFontJobCallbackState callback_state = {};
        ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontPrewarmText(font, texts[i], DynamicFontJobCallback, &callback_state));
        ASSERT_TRUE(WaitForDynamicFontJobCallbacks(m_JobContext, &callback_state, 1));
        ASSERT_EQ(1, callback_state.m_Result);
        ASSERT_EQ(FONT_RESULT_OK, GetGlyph(font_map, hfont, codepoints[i], &glyphs[i]));
        ASSERT_NE((FontGlyph*)0, glyphs[i]);
        if (i == 0)
        {
            ASSERT_EQ(-3.0f, glyphs[i]->m_Ascent);
            ASSERT_EQ(-3, font_map->m_CacheCellMaxAscent);
            ASSERT_EQ(19, font_map->m_CacheCellHeight);
        }

        dmRender::UpdateCacheTexture(font_map);
        // Raising the baseline for a new glyph must still leave room for earlier glyphs.
        for (uint32_t j = 0; j <= i; ++j)
        {
            FontGlyph* glyph = glyphs[j];
            int32_t offset_y = font_map->m_CacheCellMaxAscent - (int32_t)glyph->m_Ascent;
            ASSERT_GE(offset_y, 0);
            ASSERT_LE(offset_y + glyph->m_Bitmap.m_Height, font_map->m_CacheCellHeight);
            uint64_t key = dmRender::MakeGlyphIndexKey(hfont, glyph->m_GlyphIndex);
            ASSERT_NE((dmRender::CacheGlyph*)0, dmRender::AddGlyphToCache(font_map, i + 1, key, glyph, offset_y));
        }
    }
    dmResource::Release(m_Factory, font);
}

static int32_t ProcessBlockingFontJob(HJobContext job_context, HJob job, void* user_context, void* user_data)
{
    (void)job_context;
    (void)job;
    int32_atomic_t* started = (int32_atomic_t*)user_context;
    int32_atomic_t* allow_finish = (int32_atomic_t*)user_data;
    dmAtomicStore32(started, 1);

    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;
    while (!dmAtomicGet32(allow_finish) && dmTime::GetMonotonicTime() < stop_time)
    {
        dmTime::Sleep(1000);
    }
    return 1;
}

// A completed prewarm request must not use callback/self references from a new
// script instance that reused the destroyed instance's Lua context-table slot.
TEST_F(FontTest, PrewarmTextRejectsCallbackAfterScriptInstanceReuse)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", (void**)&font));
    ASSERT_NE((void*)0, font);

    // Occupy the only worker thread so both prewarm requests remain pending
    // until the two script instances have been created and destroyed/reused.
    int32_t blocker_started = 0;
    int32_t blocker_allow_finish = 0;
    Job blocker = {};
    blocker.m_Process = ProcessBlockingFontJob;
    blocker.m_Context = &blocker_started;
    blocker.m_Data = &blocker_allow_finish;

    HJob blocker_job = JobSystemCreateJob(m_JobContext, &blocker);
    ASSERT_NE((HJob)0, blocker_job);
    ASSERT_EQ(JOBSYSTEM_RESULT_OK, JobSystemPushJob(m_JobContext, blocker_job));

    uint64_t blocker_stop_time = dmTime::GetMonotonicTime() + 500000;
    while (!dmAtomicGet32(&blocker_started) && dmTime::GetMonotonicTime() < blocker_stop_time)
    {
        dmTime::Sleep(1000);
    }
    ASSERT_EQ(1, dmAtomicGet32(&blocker_started));

    // The first instance starts a prewarm request. Its Lua callback remembers
    // this instance's context-table reference plus callback/self indices.
    dmGameObject::HInstance first = Spawn(m_Factory, m_Collection, "/font/prewarm_callback_instance_reuse.goc", dmHashString64("/first"), 0,
                                          Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((dmGameObject::HInstance)0, first);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Destroy the first instance before its prewarm job completes. The Lua
    // registry can now reuse its released context-table reference.
    dmGameObject::Delete(m_Collection, first, false);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // The replacement instance deterministically reuses that registry slot and
    // creates another callback with the same table-local callback/self indices.
    // SetupCallback() alone cannot distinguish the old and new instances.
    dmGameObject::HInstance replacement = Spawn(m_Factory, m_Collection, "/font/prewarm_callback_instance_reuse.goc", dmHashString64("/replacement"), 0,
                                                Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((dmGameObject::HInstance)0, replacement);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Let the blocker and both font jobs finish, then dispatch their callbacks
    // on this thread through JobSystemUpdate().
    dmAtomicStore32(&blocker_allow_finish, 1);

    uint64_t stop_time = dmTime::GetMonotonicTime() + 500000;
    while (!font->m_PendingJobs.Empty() && dmTime::GetMonotonicTime() < stop_time)
    {
        JobSystemUpdate(m_JobContext, 0);
        dmTime::Sleep(1000);
    }

    // IsCallbackValid() must reject the stale callback by unique script id.
    // Without that check, the first completion resolves through the replacement
    // context table and invokes its callback, making this count two instead of one.
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    ASSERT_TRUE(font->m_PendingJobs.Empty());

    lua_getglobal(L, "prewarm_stale_callback_count");
    ASSERT_EQ(0, lua_tointeger(L, -1));
    lua_pop(L, 1);

    lua_getglobal(L, "prewarm_replacement_callback_count");
    ASSERT_EQ(1, lua_tointeger(L, -1));
    lua_pop(L, 1);

    dmResource::Release(m_Factory, font);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Reloading a dynamic font with pending work must cancel the old jobs and
// suppress callbacks after the old font state has been torn down.
TEST_F(FontTest, ReloadCancelsPendingDynamicFontJobs)
{
    const char path_font[] = "/font/dyn_glyph_bank_test_1.fontc";
    dmGameSystem::FontResource* font = 0;
    DynamicFontJobCallbackState callback_state = {0, -1, {0}};

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font, (void**) &font));
    ASSERT_NE((void*)0, font);
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontPrewarmText(font, "Reload pending jobs", DynamicFontJobCallback, &callback_state));
    ASSERT_GT(font->m_PendingJobs.Size(), 0u);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path_font, 0));
    ASSERT_EQ(0u, font->m_PendingJobs.Size());
    ASSERT_EQ(0u, callback_state.m_CallbackCount);

    JobSystemUpdate(m_JobContext, 0);
    ASSERT_EQ(0u, callback_state.m_CallbackCount);

    dmResource::Release(m_Factory, font);
}

// Releasing a dynamic font with pending work must cancel the jobs and suppress
// callbacks that would otherwise touch destroyed font job data.
TEST_F(FontTest, ReleaseCancelsPendingDynamicFontJobs)
{
    const char path_font[] = "/font/dyn_glyph_bank_test_1.fontc";
    dmGameSystem::FontResource* font = 0;
    DynamicFontJobCallbackState callback_state = {0, -1, {0}};

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font, (void**) &font));
    ASSERT_NE((void*)0, font);
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontPrewarmText(font, "Release pending jobs", DynamicFontJobCallback, &callback_state));
    ASSERT_GT(font->m_PendingJobs.Size(), 0u);

    dmResource::Release(m_Factory, font);
    ASSERT_EQ(0u, callback_state.m_CallbackCount);

    JobSystemUpdate(m_JobContext, 0);
    ASSERT_EQ(0u, callback_state.m_CallbackCount);
}

// A successful prewarm callback may reload the same font because job ownership
// is destroyed before invoking the user callback.
TEST_F(FontTest, DynamicFontPrewarmCallbackCanReloadSameFont)
{
    const char path_font[] = "/font/dyn_glyph_bank_test_1.fontc";
    dmGameSystem::FontResource* font = 0;
    DynamicFontJobCallbackState callback_state = {0, -1, {0}};

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font, (void**) &font));
    ASSERT_NE((void*)0, font);
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    callback_state.m_Factory = m_Factory;
    callback_state.m_Path = path_font;
    callback_state.m_ReloadResult = dmResource::RESULT_NOT_LOADED;

    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontPrewarmText(font, "Callback reloads same font", DynamicFontJobReloadCallback, &callback_state));
    ASSERT_GT(font->m_PendingJobs.Size(), 0u);

    ASSERT_TRUE(WaitForDynamicFontJobCallbacks(m_JobContext, &callback_state, 1));
    ASSERT_EQ(1u, callback_state.m_CallbackCount);
    ASSERT_EQ(1, callback_state.m_Result);
    ASSERT_STREQ("", callback_state.m_ErrMsg);
    ASSERT_EQ(1u, callback_state.m_ReloadCount);
    ASSERT_EQ(dmResource::RESULT_OK, callback_state.m_ReloadResult);

    dmResource::Release(m_Factory, font);
}

// While a font resource is being destroyed, new dynamic glyph jobs must be
// rejected from both explicit prewarm and cache-miss paths.
TEST_F(FontTest, DestroyingDynamicFontRejectsNewJobs)
{
    const char path_font[] = "/font/dyn_glyph_bank_test_1.fontc";
    dmGameSystem::FontResource* font = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_font, (void**) &font));
    ASSERT_NE((void*)0, font);
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    font->m_Destroying = 1;
    ASSERT_EQ(dmResource::RESULT_INVAL, dmGameSystem::ResFontPrewarmText(font, "Rejected while destroying", 0, 0));
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    dmRender::HFontMap font_map = dmGameSystem::ResFontGetHandle(font);
    HFontCollection font_collection = dmRender::GetFontCollection(font_map);
    HFont hfont = FontCollectionGetFont(font_collection, 0);

    FontGlyph* glyph = 0;
    ASSERT_EQ(FONT_RESULT_ERROR, GetGlyph(font_map, hfont, 'A', &glyph));
    ASSERT_EQ((FontGlyph*)0, glyph);
    ASSERT_EQ(0u, font->m_PendingJobs.Size());

    font->m_Destroying = 0;
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

TEST_F(FontTest, CompiledFontStyleTable)
{
    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/authored_styles.fontc", (void**)&font));
    ASSERT_EQ(2u, font->m_DDF->m_Styles.m_Count);
    HFontCollection        collection = dmGameSystem::ResFontGetFontCollection(font);
    const TextRenderStyle* defaults = FontCollectionGetNamedStyle(collection, dmHashString64("default"));
    ASSERT_NE((const TextRenderStyle*)0, defaults);
    ASSERT_EQ(1.375f, defaults->m_OutlineWidth);
    ASSERT_EQ(0.3725f, defaults->m_OutlineAlpha);
    ASSERT_EQ(0.6235f, defaults->m_ShadowAlpha);
    ASSERT_EQ(-1.625f, defaults->m_ShadowY);
    ASSERT_EQ(0u, defaults->m_Flags & (TEXT_RENDER_STYLE_OUTLINE_COLOR | TEXT_RENDER_STYLE_SHADOW_COLOR));
    ASSERT_EQ((const TextRenderStyle*)0, FontCollectionGetNamedStyle(collection, dmHashString64("link")));
    dmhash_t                        notice = dmHashString64("notice");
    const TextNamedStyleDecoration* decoration = FontCollectionGetNamedStyleDecoration(collection, notice);
    ASSERT_NE((const TextNamedStyleDecoration*)0, decoration);
    ASSERT_EQ((uint8_t)TEXT_RESOLVED_DECORATION_UNDERLINE, decoration->m_Flags);
    uint32_t          count = 0;
    const TextEffect* effects = FontCollectionGetNamedStyleEffects(collection, notice, &count);
    ASSERT_EQ(2u, count);
    ASSERT_EQ((uint16_t)TEXT_EFFECT_WAVE, effects[0].m_Type);
    ASSERT_EQ((uint16_t)TEXT_EFFECT_SHAKE, effects[1].m_Type);
    dmResource::Release(m_Factory, font);
}

TEST_F(FontTest, EmptyCompiledFontStyleTable)
{
    const char* path = "/font/authored_styles.fontc";
    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path, (void**)&font));

    dmRenderDDF::FontMap replacement = *font->m_DDF;
    replacement.m_Styles.m_Data = 0;
    replacement.m_Styles.m_Count = 0;
    dmArray<uint8_t> buffer;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&replacement, dmRenderDDF::FontMap::m_DDFDescriptor, buffer));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, dmHashString64(path), buffer.Begin(), buffer.Size()));

    HFontCollection collection = dmGameSystem::ResFontGetFontCollection(font);
    const char* names[] = { "default", "link", "link:hover", "link:active", "notice" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(names); ++i)
    {
        dmhash_t name = dmHashString64(names[i]);
        ASSERT_EQ((const TextRenderStyle*)0, FontCollectionGetNamedStyle(collection, name));
        ASSERT_EQ((const TextNamedStyleDecoration*)0, FontCollectionGetNamedStyleDecoration(collection, name));
    }

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path, 0));
    collection = dmGameSystem::ResFontGetFontCollection(font);
    ASSERT_NE((const TextRenderStyle*)0, FontCollectionGetNamedStyle(collection, dmHashString64("default")));
    ASSERT_NE((const TextNamedStyleDecoration*)0, FontCollectionGetNamedStyleDecoration(collection, dmHashString64("notice")));
    dmResource::Release(m_Factory, font);
}

TEST_F(FontTest, ScriptSetNamedFontStyle)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory         = m_Factory;
    scriptlibcontext.m_Register        = m_Register;
    scriptlibcontext.m_LuaState        = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_GraphicsContext = m_GraphicsContext;
    scriptlibcontext.m_ScriptContext   = m_ScriptContext;
    scriptlibcontext.m_JobContext      = m_JobContext;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    dmGameSystem::FontResource* font = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", (void**)&font));

    HFontCollection collection = dmGameSystem::ResFontGetFontCollection(font);
    const TextNamedStyleDecoration* link_decoration = FontCollectionGetNamedStyleDecoration(collection, dmHashString64("link"));
    ASSERT_NE((const TextNamedStyleDecoration*)0, link_decoration);
    ASSERT_EQ((uint8_t)TEXT_RESOLVED_DECORATION_UNDERLINE, link_decoration->m_Flags);

    lua_State* L = scriptlibcontext.m_LuaState;
    ASSERT_TRUE(RunString(L, "font.set_style('/font/dyn_glyph_bank_test_1.fontc', 'link', '<color=#336699CC>')"));
    link_decoration = FontCollectionGetNamedStyleDecoration(collection, dmHashString64("link"));
    ASSERT_NE((const TextNamedStyleDecoration*)0, link_decoration);
    ASSERT_EQ((uint8_t)TEXT_RESOLVED_DECORATION_UNDERLINE, link_decoration->m_Flags);
    ASSERT_TRUE(RunString(L, "font.set_style('/font/dyn_glyph_bank_test_1.fontc', 'link:hover', '<color=#336699CC><outline size=2><shadow x=-1 blur=3>')"));

    const TextRenderStyle* style = FontCollectionGetNamedStyle(collection, dmHashString64("link:hover"));
    ASSERT_NE((const TextRenderStyle*)0, style);
    ASSERT_EQ(TEXT_RENDER_STYLE_FACE_COLOR | TEXT_RENDER_STYLE_OUTLINE_WIDTH | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_BLUR, style->m_Flags);
    ASSERT_NEAR(0.2f, style->m_FaceColor[0], 0.0001f);
    ASSERT_NEAR(0.8f, style->m_FaceColor[3], 0.0001f);
    ASSERT_EQ(2.0f, style->m_OutlineWidth);
    ASSERT_EQ(-1.0f, style->m_ShadowX);
    ASSERT_EQ(3.0f, style->m_ShadowBlur);

    dmLogInfo("Expected errors ->");
    ASSERT_FALSE(RunString(L, "font.set_style('/font/dyn_glyph_bank_test_1.fontc', 'bad', '<shadow blur=-1>')"));
    ASSERT_FALSE(RunString(L, "font.set_style('/font/dyn_glyph_bank_test_1.fontc', 'bad', '<color=#ffffff></color>')"));
    dmLogInfo("<- End of expected errors.");

    dmResource::Release(m_Factory, font);
    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);
}

TEST_F(FontTest, OpenTypeResource)
{
    const char* otf_path = "/font/SourceCodePro-Regular.otf";
    uint32_t data_size = 0;
    uint8_t* data = dmTestUtil::ReadHostFile("src/gamesys/test/font/SourceCodePro-Regular.otf", &data_size);
    ASSERT_NE((uint8_t*)0, data);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::AddFile(m_Factory, otf_path, data_size, data));

    dmGameSystem::TTFResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, otf_path, (void**)&resource));
    ASSERT_NE((dmGameSystem::TTFResource*)0, resource);

    HFont font = dmGameSystem::GetFont(resource);
    uint32_t glyph_index = FontGetGlyphIndex(font, 'A');
    ASSERT_NE(0u, glyph_index);
    FontGlyphOptions options;
    options.m_Scale = FontGetScaleFromSize(font, 32);
    options.m_GenerateImage = true;
    FontGlyph glyph;
    ASSERT_EQ(FONT_RESULT_OK, FontGetGlyphByIndex(font, glyph_index, &options, &glyph));
    ASSERT_NE((uint8_t*)0, glyph.m_Bitmap.m_Data);
    ASSERT_EQ(FONT_RESULT_OK, FontFreeGlyph(font, &glyph));

    dmGameSystem::FontResource* font_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/font/dyn_glyph_bank_test_1.fontc", (void**)&font_resource));
    dmRender::HFontMap font_map = dmGameSystem::ResFontGetHandle(font_resource);
    HFontCollection font_collection = dmRender::GetFontCollection(font_map);
    uint32_t font_count = FontCollectionGetFontCount(font_collection);
    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontAddFontByPath(m_Factory, font_resource, otf_path));
    ASSERT_EQ(font_count + 1, FontCollectionGetFontCount(font_collection));
    ASSERT_EQ(dmResource::RESULT_OK, dmGameSystem::ResFontRemoveFont(m_Factory, font_resource, dmHashString64(otf_path)));
    ASSERT_EQ(font_count, FontCollectionGetFontCount(font_collection));
    dmResource::Release(m_Factory, font_resource);

    dmResource::Release(m_Factory, resource);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::RemoveFile(m_Factory, otf_path));
    dmMemory::AlignedFree(data);
}

// Test that a GUI with mixed nodes (box + multiple text nodes) produces a single font
// dispatch for all text (not one per text node), and that text render order is preserved.
TEST_F(GuiComponentTest, GuiTextSingleFlushAndOrder)
{
    const char* go_path = "/gui/gui_text_flush_test.goc";
    const uint32_t num_text_nodes = 5;

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);
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
    ASSERT_EQ(dmHashString64("/gui/font_valid.ttf"), GetTextLayoutGlyphFontPathHash(dynamic_font_resource, initial.m_TextLayout));

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
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/gui/font_dyn_glyph_bank_test_1.fontc", 0));
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
    ASSERT_NE(0, go);
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

TEST_F(GuiTest, GuiSelectedBaseStyle)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    dmGui::HScene scene = GetGuiComponent(m_Collection)->m_Scene;
    dmGui::HNode  node = dmGui::GetNodeById(scene, "text");
    ASSERT_EQ(dmHashString64("default"), dmGui::GetNodeTextStyle(scene, node));
    dmGui::SetNodeText(scene, node, "A<color=#ff0000>B</color>");
    dmGui::SetNodeTextStyle(scene, node, dmHashString64("link"));
    HTextLayout layout = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection).m_TextLayout;
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(dmHashString64("link"), layout->m_BaseStyleName);
    ASSERT_GT(TextLayoutGetDecorationCount(layout), 0u);
    const float         color[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextGlyphRenderData data;
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[1], color, &data);
    ASSERT_EQ(1.0f, data.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(0.0f, data.m_FaceColors.m_BottomLeft[1]);
    dmGui::SetNodeTextStyle(scene, node, 0);
    layout = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection).m_TextLayout;
    ASSERT_EQ((dmhash_t)0, layout->m_BaseStyleName);
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[1], color, &data);
    ASSERT_EQ(1.0f, data.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(0.0f, data.m_FaceColors.m_BottomLeft[1]);
    dmGui::SetNodeTextStyle(scene, node, dmHashString64("missing"));
    layout = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection).m_TextLayout;
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));

    // A replacement font can remove a selected name, then restore it later.
    dmGui::SetNodeTextStyle(scene, node, dmHashString64("link"));
    dmGameSystem::FontResource* font = (dmGameSystem::FontResource*)dmGui::GetNodeFont(scene, node);
    dmRenderDDF::FontMap        replacement = *font->m_DDF;
    replacement.m_Styles.m_Count = 1;
    dmArray<uint8_t> buffer;
    ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&replacement, dmRenderDDF::FontMap::m_DDFDescriptor, buffer));
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, dmHashString64("/gui/font_dyn_glyph_bank_test_1.fontc"), buffer.Begin(), buffer.Size()));
    layout = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection).m_TextLayout;
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(dmHashString64("link"), layout->m_BaseStyleName);
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));
    ASSERT_EQ(layout, PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection).m_TextLayout);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/gui/font_dyn_glyph_bank_test_1.fontc", 0));
    layout = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection).m_TextLayout;
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_GT(TextLayoutGetDecorationCount(layout), 0u);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiPreparedRichTextLayout)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);

    dmGui::HScene scene = gui_component->m_Scene;
    dmGui::HNode  node = dmGui::GetNodeById(scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);

    dmGui::SetNodeText(scene, node, "Plain <color=#ff8040>orange</color> and <size=24>large</size>.");
    GuiTextSubmitResult rich_text = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(1u, rich_text.m_TextEntryCount);
    ASSERT_NE((HTextLayout)0, rich_text.m_TextLayout);
    ASSERT_EQ(0u, rich_text.m_TextBufferSize);
    ASSERT_EQ(23u, TextLayoutGetGlyphCount(rich_text.m_TextLayout));
    ASSERT_TRUE(((TextLayout*)rich_text.m_TextLayout)->m_UseRichText);
    const TextGlyph* glyphs = TextLayoutGetGlyphs(rich_text.m_TextLayout);
    ASSERT_NE(glyphs[0].m_MarkupSpanIndex, glyphs[6].m_MarkupSpanIndex);
    ASSERT_GT(glyphs[17].m_RenderScale, glyphs[0].m_RenderScale);

    const char malformed_source[] = "<color=#ff8040>A</size>";
    dmGui::SetNodeText(scene, node, malformed_source);
    GuiTextSubmitResult malformed_text = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(1u, malformed_text.m_TextEntryCount);
    ASSERT_NE((HTextLayout)0, malformed_text.m_TextLayout);
    ASSERT_EQ(0u, malformed_text.m_TextBufferSize);
    ASSERT_EQ(sizeof(malformed_source) - 1, TextLayoutGetGlyphCount(malformed_text.m_TextLayout));
    ASSERT_TRUE(((TextLayout*)malformed_text.m_TextLayout)->m_UseRichText);
    const TextGlyph* malformed_glyphs = TextLayoutGetGlyphs(malformed_text.m_TextLayout);
    ASSERT_EQ((uint32_t)'<', malformed_glyphs[0].m_Codepoint);
    ASSERT_EQ((uint32_t)'>', malformed_glyphs[TextLayoutGetGlyphCount(malformed_text.m_TextLayout) - 1].m_Codepoint);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiLayoutObjectsAreCurrentOnDemand)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_layout_objects_on_demand.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    bool tests_done = false;
    // Rendering would populate the cache and hide failures in the on-demand script path.
    WaitForTestsDone(10, false, &tests_done);
    ASSERT_TRUE(tests_done);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiRichTextLinkInteraction)
{
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    dmGui::SetDefaultResolution(m_GuiContext, 640, 480);
    dmGui::SetPhysicalResolution(m_GuiContext, 640, 480);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);
    dmGui::HNode node = dmGui::GetNodeById(gui_component->m_Scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);
    dmGui::SetNodeText(gui_component->m_Scene, node, "<link id=docs src=https://www.defold.com>Link</link>");

    GuiTextSubmitResult initial = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, initial.m_TextLayout);
    ASSERT_EQ(1u, TextLayoutGetObjectCount(initial.m_TextLayout));
    ASSERT_EQ(1u, TextLayoutGetDecorationCount(initial.m_TextLayout));

    TextGlyphRenderData before = {};
    TextLayoutGetGlyphRenderData(initial.m_TextLayout, TextLayoutGetGlyphs(initial.m_TextLayout)[0], white, &before);

    dmGameObject::AcquireInputFocus(m_Collection, go);
    dmGameObject::InputAction input_action = {};
    input_action.m_PositionSet = 1;
    input_action.m_X = 307.0f;
    input_action.m_Y = 240.0f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    TextGlyphRenderData hovered = {};
    TextLayoutGetGlyphRenderData(initial.m_TextLayout, TextLayoutGetGlyphs(initial.m_TextLayout)[0], white, &hovered);
    ASSERT_NE(before.m_FaceColors.m_BottomLeft[0], hovered.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(initial.m_TextLayout));
    ASSERT_NEAR(0.25f, dmGui::GetNodeProperty(gui_component->m_Scene, node, dmGui::PROPERTY_COLOR).getX(), 0.0001f);

    for (float x = 307.0f; x <= 333.0f; x += 0.25f)
    {
        input_action.m_X = x;
        ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
        ASSERT_EQ(initial.m_TextLayout, gui_component->m_HoveredLayoutObject.m_Layout);
        ASSERT_EQ(0u, TextLayoutGetDecorationCount(initial.m_TextLayout));
    }

    input_action.m_X = 500.0f;
    input_action.m_Y = 400.0f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    TextGlyphRenderData unhovered = {};
    TextLayoutGetGlyphRenderData(initial.m_TextLayout, TextLayoutGetGlyphs(initial.m_TextLayout)[0], white, &unhovered);
    ASSERT_NEAR(before.m_FaceColors.m_BottomLeft[0], unhovered.m_FaceColors.m_BottomLeft[0], 0.0001f);
    ASSERT_EQ(1u, TextLayoutGetDecorationCount(initial.m_TextLayout));
    ASSERT_NEAR(0.5f, dmGui::GetNodeProperty(gui_component->m_Scene, node, dmGui::PROPERTY_COLOR).getX(), 0.0001f);

    input_action.m_X = 307.0f;
    input_action.m_Y = 240.0f;
    input_action.m_Pressed = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(initial.m_TextLayout));
    input_action.m_Pressed = 0;
    input_action.m_Released = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_STREQ("clicked", dmGui::GetNodeText(gui_component->m_Scene, node));

    const char sprite_markup[] = "<sprite id=icon src=/icon.png width=32px height=32px/>";
    dmGui::SetNodeText(gui_component->m_Scene, node, sprite_markup);
    GuiTextSubmitResult sprite = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, sprite.m_TextLayout);
    ASSERT_EQ(1u, TextLayoutGetObjectCount(sprite.m_TextLayout));
    ASSERT_EQ(dmHashString64("sprite"), TextLayoutGetObjects(sprite.m_TextLayout)[0].m_Tag);

    input_action = {};
    input_action.m_PositionSet = 1;
    input_action.m_X = 320.0f;
    input_action.m_Y = 240.0f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_NEAR(0.5f, dmGui::GetNodeProperty(gui_component->m_Scene, node, dmGui::PROPERTY_COLOR).getX(), 0.0001f);

    input_action.m_Pressed = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    input_action.m_Pressed = 0;
    input_action.m_Released = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_STREQ(sprite_markup, dmGui::GetNodeText(gui_component->m_Scene, node));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiRichTextLinkTargetFontReload)
{
    dmGui::SetDefaultResolution(m_GuiContext, 640, 480);
    dmGui::SetPhysicalResolution(m_GuiContext, 640, 480);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);
    dmGui::HNode node = dmGui::GetNodeById(gui_component->m_Scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);
    dmGui::SetNodeText(gui_component->m_Scene, node, "<link id=docs src=https://www.defold.com>Link</link>");

    GuiTextSubmitResult initial = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, initial.m_TextLayout);

    dmGameSystem::FontResource* font_resource = (dmGameSystem::FontResource*)dmGui::GetNodeFont(gui_component->m_Scene, node);
    ASSERT_NE((void*)0, font_resource);
    const uint32_t font_version = dmGameSystem::ResFontGetVersion(font_resource);

    dmGameObject::AcquireInputFocus(m_Collection, go);
    dmGameObject::InputAction input_action = {};
    input_action.m_PositionSet = 1;
    input_action.m_X = 307.0f;
    input_action.m_Y = 240.0f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_EQ(initial.m_TextLayout, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_EQ(font_resource, gui_component->m_HoveredLayoutObject.m_FontResource);
    ASSERT_EQ(font_version, gui_component->m_HoveredLayoutObject.m_FontVersion);

    input_action.m_Pressed = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_EQ(initial.m_TextLayout, gui_component->m_PressedLayoutObject.m_Layout);
    ASSERT_EQ(font_version, gui_component->m_PressedLayoutObject.m_FontVersion);

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, "/gui/font_dyn_glyph_bank_test_1.fontc", 0));
    ASSERT_EQ(font_version + 1, dmGameSystem::ResFontGetVersion(font_resource));
    ASSERT_EQ(initial.m_TextLayout, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_EQ(initial.m_TextLayout, gui_component->m_PressedLayoutObject.m_Layout);

    input_action.m_X = 500.0f;
    input_action.m_Y = 400.0f;
    input_action.m_Pressed = 0;
    input_action.m_Released = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_EQ((HTextLayout)0, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_EQ((HTextLayout)0, gui_component->m_PressedLayoutObject.m_Layout);
    ASSERT_NEAR(0.5f, dmGui::GetNodeProperty(gui_component->m_Scene, node, dmGui::PROPERTY_COLOR).getX(), 0.0001f);

    input_action.m_X = 307.0f;
    input_action.m_Y = 240.0f;
    input_action.m_Released = 0;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_NE((HTextLayout)0, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_NE(initial.m_TextLayout, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_EQ(font_resource, gui_component->m_HoveredLayoutObject.m_FontResource);
    ASSERT_EQ(font_version + 1, gui_component->m_HoveredLayoutObject.m_FontVersion);
    ASSERT_NEAR(0.25f, dmGui::GetNodeProperty(gui_component->m_Scene, node, dmGui::PROPERTY_COLOR).getX(), 0.0001f);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiRichTextLinkInteractionUsesRenderLayerOrder)
{
    dmGui::SetDefaultResolution(m_GuiContext, 640, 480);
    dmGui::SetPhysicalResolution(m_GuiContext, 640, 480);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);
    dmGui::HScene scene = gui_component->m_Scene;
    dmGui::HNode  front = dmGui::GetNodeById(scene, "text");
    ASSERT_NE((dmGui::HNode)0, front);
    const char* front_text = "<link id=docs src=https://www.defold.com role=front>Link</link>";
    const char* back_text  = "<link id=docs src=https://www.defold.com role=back>Link</link>";
    dmGui::SetNodeText(scene, front, front_text);

    dmGui::HNode back = 0;
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::CloneNode(scene, front, &back));
    ASSERT_NE((dmGui::HNode)0, back);
    dmGui::SetNodeText(scene, back, back_text);

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::AddLayer(scene, "front"));
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::SetNodeLayer(scene, front, "front"));
    dmGui::MoveNodeBelow(scene, front, back);
    ASSERT_GT(dmGui::GetNodeLayerIndex(scene, front), dmGui::GetNodeLayerIndex(scene, back));

    GuiTextSubmitResult prepared = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_EQ(2u, prepared.m_TextEntryCount);

    dmGameObject::AcquireInputFocus(m_Collection, go);
    dmGameObject::InputAction input_action = {};
    input_action.m_PositionSet = 1;
    input_action.m_X = 307.0f;
    input_action.m_Y = 240.0f;

    dmGui::SetNodeVisible(scene, front, false);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_NE((HTextLayout)0, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_STREQ(back_text, TextLayoutGetObjectSource(gui_component->m_HoveredLayoutObject.m_Layout));

    dmGui::SetNodeVisible(scene, front, true);
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_NE((HTextLayout)0, gui_component->m_HoveredLayoutObject.m_Layout);
    ASSERT_STREQ(front_text, TextLayoutGetObjectSource(gui_component->m_HoveredLayoutObject.m_Layout));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiRichTextAnimationAdvances)
{
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0, gui_component);
    dmGui::HNode node = dmGui::GetNodeById(gui_component->m_Scene, "text");
    ASSERT_NE((dmGui::HNode)0, node);
    dmGui::SetNodeText(gui_component->m_Scene, node, "<wave amplitude=4 hz=1 fit=span>Wave</wave>");

    GuiTextSubmitResult initial = PrepareGuiAndGetTextLayout(m_RenderContext, m_Collection);
    ASSERT_NE((HTextLayout)0, initial.m_TextLayout);
    TextGlyphRenderData before = {};
    TextLayoutGetGlyphRenderData(initial.m_TextLayout, TextLayoutGetGlyphs(initial.m_TextLayout)[0], white, &before);

    m_UpdateContext.m_DT = 0.25f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    TextGlyphRenderData after = {};
    TextLayoutGetGlyphRenderData(initial.m_TextLayout, TextLayoutGetGlyphs(initial.m_TextLayout)[0], white, &after);
    ASSERT_NE(before.m_OffsetY, after.m_OffsetY);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiPreparedTextLayoutDestroyedBeforeDraw)
{
    // The GUI node can clear its cached prepared layout after queueing text but
    // before DrawRenderList. The queued render entry still needs its own ref.
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_text_layout_cache.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
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

TEST_F(LabelComponentTest, LabelTextProperty)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");
    const dmhash_t text_id = dmHashString64("text");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::PropertyOptions options;
    dmGameObject::PropertyDesc desc;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, label_id, text_id, options, desc));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_TEXT, desc.m_Variant.m_Type);
    ASSERT_STREQ("Label", desc.m_Variant.m_Text);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, text_id, options, dmGameObject::PropertyVar("Runtime label text")));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, label_id, text_id, options, desc));
    ASSERT_STREQ("Runtime label text", desc.m_Variant.m_Text);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH, dmGameObject::SetProperty(go, label_id, text_id, options, dmGameObject::PropertyVar(1.0f)));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, label_id, text_id, options, desc));
    ASSERT_STREQ("Runtime label text", desc.m_Variant.m_Text);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, text_id, options, dmGameObject::PropertyVar("")));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, label_id, text_id, options, desc));
    ASSERT_STREQ("", desc.m_Variant.m_Text);
}

TEST_F(LabelComponentTest, LabelUserDataSurvivesPoolCompaction)
{
    const dmhash_t victim_go_id = dmHashString64("/victim");
    const dmhash_t target_go_id = dmHashString64("/target");
    const dmhash_t label_id = dmHashString64("label");
    const dmhash_t text_id = dmHashString64("text");
    const char* link_text = "<link id=docs>Link</link>";
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance victim = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", victim_go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, victim);
    dmGameObject::HInstance target = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", target_go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, target);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* target_before = GetLabelComponent(target, label_id);
    ASSERT_NE((void*)0, target_before);

    DeleteInstance(m_Collection, victim);

    dmGameSystem::LabelComponent* target_after = GetLabelComponent(target, label_id);
    ASSERT_NE((void*)0, target_after);
    ASSERT_NE(target_before, target_after);

    dmGameObject::PropertyOptions options;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(target, label_id, text_id, options, dmGameObject::PropertyVar(link_text)));

    dmGameObject::PropertyDesc desc;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(target, label_id, text_id, options, desc));
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_TEXT, desc.m_Variant.m_Type);
    ASSERT_STREQ(link_text, desc.m_Variant.m_Text);

    HTextLayout layout = dmGameSystem::CompLabelGetTextLayout(target_after);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(1u, TextLayoutGetObjectCount(layout));
    ASSERT_EQ(1u, TextLayoutGetDecorationCount(layout));

    TextGlyphRenderData before = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &before);

    dmGameObject::AcquireInputFocus(m_Collection, target);
    dmGameObject::InputAction input_action = {};
    input_action.m_PositionSet = 1;
    input_action.m_X = -13.0f;
    input_action.m_Y = 82.96361f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    TextGlyphRenderData hovered = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &hovered);
    ASSERT_NE(before.m_FaceColors.m_BottomLeft[0], hovered.m_FaceColors.m_BottomLeft[0]);

    DeleteInstance(m_Collection, target);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LabelComponentTest, LabelSelectedBaseStyles)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/styled_labels.goc", dmHashString64("/go"));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    const char*    components[] = { "default", "plain", "notice" };
    const dmhash_t styles[] = { dmHashString64("default"), 0, dmHashString64("notice") };
    const float    white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    for (uint32_t i = 0; i < 3; ++i)
    {
        dmGameSystem::LabelComponent* component = GetLabelComponent(go, dmHashString64(components[i]));
        ASSERT_NE((void*)0, component);
        HTextLayout layout = dmGameSystem::CompLabelGetTextLayout(component);
        ASSERT_NE((HTextLayout)0, layout);
        ASSERT_EQ(styles[i], layout->m_BaseStyleName);
        ASSERT_EQ(i == 2, TextLayoutGetDecorationCount(layout) > 0);
        TextGlyphRenderData data;
        TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[1], white, &data);
        ASSERT_EQ(1.0f, data.m_FaceColors.m_BottomLeft[0]);
        ASSERT_EQ(0.0f, data.m_FaceColors.m_BottomLeft[1]);
        ASSERT_EQ(i == 0, (data.m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) != 0);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LabelComponentTest, LabelStyleHashReload)
{
    const char*                  path = "/label/valid.labelc";
    dmGameSystem::LabelResource* resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path, (void**)&resource));
    ASSERT_EQ(dmHashString64("default"), resource->m_DDF->m_StyleHash);

    const dmhash_t hashes[] = { dmHashString64("link"), 0 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(hashes); ++i)
    {
        dmGameSystemDDF::LabelDesc replacement = *resource->m_DDF;
        replacement.m_StyleHash = hashes[i];
        dmArray<uint8_t> buffer;
        ASSERT_EQ(dmDDF::RESULT_OK, dmDDF::SaveMessageToArray(&replacement, dmGameSystemDDF::LabelDesc::m_DDFDescriptor, buffer));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::SetResource(m_Factory, dmHashString64(path), buffer.Begin(), buffer.Size()));
        ASSERT_EQ(hashes[i], resource->m_DDF->m_StyleHash);
    }

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path, 0));
    ASSERT_EQ(dmHashString64("default"), resource->m_DDF->m_StyleHash);
    dmResource::Release(m_Factory, resource);
}

TEST_F(LabelComponentTest, LabelPreparedTextLayoutInvalidation)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* label_component = GetLabelComponent(go, label_id);
    ASSERT_NE((void*)0, label_component);

    dmRender::TextMetrics initial_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, initial_metrics);
    ASSERT_GT(initial_metrics.m_Width, 0.0f);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

    dmGameObject::PropertyOptions options;
    const char malformed_text[] = "<color=#ff8040>A</size>";
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, dmHashString64("text"), options, dmGameObject::PropertyVar(malformed_text)));
    HTextLayout malformed_layout = dmGameSystem::CompLabelGetTextLayout(label_component);
    ASSERT_NE((HTextLayout)0, malformed_layout);
    ASSERT_EQ(sizeof(malformed_text) - 1, TextLayoutGetGlyphCount(malformed_layout));
    ASSERT_TRUE(((TextLayout*)malformed_layout)->m_UseRichText);

    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, label_id, dmHashString64("text"), options, dmGameObject::PropertyVar("Label Label Label")));

    dmRender::TextMetrics text_metrics = {};
    dmGameSystem::CompLabelGetTextMetrics(label_component, text_metrics);
    ASSERT_GT(text_metrics.m_Width, initial_metrics.m_Width);
    ASSERT_NE((HTextLayout)0, RenderLabelAndGetTextLayout(m_RenderContext, m_Collection));

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
    ASSERT_NE(0, font_go);
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

TEST_F(LabelComponentTest, LabelRichTextAnimationAdvances)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    PostLabelSetText(m_Collection, go_id, label_id, "<wave amplitude=4 hz=1 fit=span>Wave</wave>", (uintptr_t)go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* label_component = GetLabelComponent(go, label_id);
    ASSERT_NE((void*)0, label_component);

    HTextLayout layout = dmGameSystem::CompLabelGetTextLayout(label_component);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_GT(TextLayoutGetGlyphCount(layout), 0u);

    TextGlyphRenderData before = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &before);

    m_UpdateContext.m_DT = 0.25f;
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    TextGlyphRenderData after = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &after);
    ASSERT_NE(before.m_OffsetY, after.m_OffsetY);

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LabelComponentTest, LabelRichTextLinkHover)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");
    const float    white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    PostLabelSetText(m_Collection, go_id, label_id, "<link id=docs>Link</link>", (uintptr_t)go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* label_component = GetLabelComponent(go, label_id);
    ASSERT_NE((void*)0, label_component);
    HTextLayout layout = dmGameSystem::CompLabelGetTextLayout(label_component);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(1u, TextLayoutGetObjectCount(layout));
    ASSERT_EQ(1u, TextLayoutGetDecorationCount(layout));

    TextGlyphRenderData before = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &before);

    dmGameObject::AcquireInputFocus(m_Collection, go);
    dmGameObject::InputAction input_action = {};
    input_action.m_PositionSet = 1;
    input_action.m_X = -13.0f;
    input_action.m_Y = 82.96361f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    TextGlyphRenderData hovered = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &hovered);
    ASSERT_NE(before.m_FaceColors.m_BottomLeft[0], hovered.m_FaceColors.m_BottomLeft[0]);
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));

    for (float x = -13.0f; x <= 13.0f; x += 0.25f)
    {
        input_action.m_X = x;
        ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
        ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));
    }

    input_action.m_X = 500.0f;
    input_action.m_Y = 400.0f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    TextGlyphRenderData unhovered = {};
    TextLayoutGetGlyphRenderData(layout, TextLayoutGetGlyphs(layout)[0], white, &unhovered);
    ASSERT_NEAR(before.m_FaceColors.m_BottomLeft[0], unhovered.m_FaceColors.m_BottomLeft[0], 0.0001f);
    ASSERT_EQ(1u, TextLayoutGetDecorationCount(layout));

    input_action.m_X = -13.0f;
    input_action.m_Y = 82.96361f;
    input_action.m_Pressed = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    ASSERT_EQ(0u, TextLayoutGetDecorationCount(layout));

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LabelComponentTest, LegacyRichTextLinkWrappedSpriteInteraction)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/link_hover.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    PostLabelSetText(m_Collection, go_id, label_id, "<link id=icon src=/icon.png><sprite width=32px height=32px/></link>", (uintptr_t)go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameSystem::LabelComponent* label_component = GetLabelComponent(go, label_id);
    ASSERT_NE((void*)0, label_component);
    HTextLayout layout = dmGameSystem::CompLabelGetTextLayout(label_component);
    ASSERT_NE((HTextLayout)0, layout);
    ASSERT_EQ(TEXT_LAYOUT_TYPE_LEGACY, FontCollectionGetLayoutType(layout->m_FontCollection));
    ASSERT_EQ(2u, TextLayoutGetObjectCount(layout));
    ASSERT_EQ(dmHashString64("link"), TextLayoutGetObjects(layout)[0].m_Tag);
    ASSERT_EQ(dmHashString64("sprite"), TextLayoutGetObjects(layout)[1].m_Tag);

    float layout_width;
    float layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    const TextLine& line = TextLayoutGetLines(layout)[0];

    dmGameObject::AcquireInputFocus(m_Collection, go);
    dmGameObject::InputAction input_action = {};
    input_action.m_PositionSet = 1;
    const TextLayoutObject& sprite = TextLayoutGetObjects(layout)[1];
    input_action.m_X = 0.5f - line.m_Width * 0.5f + sprite.m_Width * 0.5f;
    input_action.m_Y = (1.0f - layout_height) * 0.5f + line.m_Baseline + sprite.m_Height * 0.3f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    input_action.m_Pressed = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));
    input_action.m_Pressed = 0;
    input_action.m_Released = 1;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    input_action.m_Released = 0;
    input_action.m_X = 500.0f;
    input_action.m_Y = 400.0f;
    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(m_Collection, &input_action, 1));

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_NEAR(123.0f, dmGameObject::GetPosition(go).getX(), 0.0001f);

    DeleteInstance(m_Collection, go);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(LabelComponentTest, LabelPreparedTextLayoutDestroyedBeforeDraw)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t label_id = dmHashString64("label");

    // Labels hit the same lifetime issue as GUI text: mutating the component
    // after queueing must not free the prepared layout out from under render.
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/label/valid_label.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

TEST_F(LabelComponentTest, LabelPreparedTextLayoutFallbackMutation)
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
    ASSERT_NE(0, go);
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
    ASSERT_NE(0, go);

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
        {0, 2, 1, 0, 2, 2, 0, 3, 2, 0, 3, 3, 0, 3, 3, 0, 3, 3}
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
        {0, 2, 1, 0, 3, 2}
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
        {0, 2, 1, 0, 3, 2}
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
        {0, 2, 1, 0, 3, 2}
    },
    // 9-slice params: asymmetric | Use geometries: 8 | Flip uv: uv | Texture: tilesource animation
    {
        "/gui/render_box_test6.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-12.000000, -16.000000, 0.0, 0.0), 0.437500, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-12.000000, -13.000000, 0.0, 0.0), 0.437500, 0.953125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -13.000000, 0.0, 0.0), 0.500000, 0.953125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, -16.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, -13.000000, 0.0, 0.0), 0.031250, 0.953125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, -13.000000, 0.0, 0.0), 0.000000, 0.953125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-12.000000, 11.000000, 0.0, 0.0), 0.437500, 0.578125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 11.000000, 0.0, 0.0), 0.500000, 0.578125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(14.000000, 11.000000, 0.0, 0.0), 0.031250, 0.578125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(16.000000, 11.000000, 0.0, 0.0), 0.000000, 0.578125, Vector4(1.0, 1.0, 1.0, 1.0), 0),
            dmGameSystem::BoxVertex(Vector4(-12.000000, 16.000000, 0.0, 0.0), 0.437500, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0), 0),
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
        {0, 2, 1, 0, 3, 2}
    }
};
INSTANTIATE_TEST_CASE_P(BoxRender, BoxRenderTest, jc_test_values_in(box_render_params));

/* Sprite cursor property */
#define F1T3 1.0f/3.0f
#define F2T3 2.0f/3.0f
const CursorTestParams cursor_properties[] = {

    // Playback none should apply the initial cursor and keep it unchanged,
    // regardless of the animation frame rate.
    {"anim_none_0",     0.5f, 1.0f, {0.5f, 0.5f}, 2},
    {"anim_none_60",    0.5f, 1.0f, {0.5f, 0.5f}, 2},

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
    {"anim_once_back",    0.25f, 1.0f, {0.75f, 0.5f, 0.25f, 0.0f, 0.0f}, 5},
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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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

TEST_F(GuiTest, GuiCustomPropertiesFromDDF)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/valid_gui.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0x0, gui_component);

    dmGui::HNode node = dmGui::GetNodeById(gui_component->m_Scene, "custom_props");
    ASSERT_NE(dmGui::INVALID_HANDLE, node);

    dmGui::CustomProperty property = {};
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_string"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_STRING, property.m_Type);
    ASSERT_STREQ("component", property.m_String);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(GuiTest, GuiCustomPropertiesFromLayoutDDF)
{
    dmRender::HDisplayProfiles display_profiles = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/display_profiles/gui_layout_no_auto.display_profilesc", (void**)&display_profiles));
    dmGui::SetDisplayProfiles(m_GuiContext, display_profiles);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/custom_properties_layout.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    dmGameSystem::GuiComponent* gui_component = GetGuiComponent(m_Collection);
    ASSERT_NE((void*)0x0, gui_component);

    dmGui::HNode node = dmGui::GetNodeById(gui_component->m_Scene, "custom_props_layout");
    ASSERT_NE(dmGui::INVALID_HANDLE, node);

    dmGui::CustomProperty property = {};
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_string"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_STRING, property.m_Type);
    ASSERT_STREQ("default", property.m_String);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_number"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_NUMBER, property.m_Type);
    ASSERT_NEAR(1.0f, property.m_Number, EPSILON);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_boolean"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_BOOLEAN, property.m_Type);
    ASSERT_TRUE(property.m_Boolean);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_vector3"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_VECTOR3, property.m_Type);
    ASSERT_VEC3(Vector3(1.0f, 2.0f, 3.0f), property.m_Vector3);

    gui_component->m_Scene->m_ApplyLayoutCallback(gui_component->m_Scene, dmHashString64("Landscape"));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_string"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_STRING, property.m_Type);
    ASSERT_STREQ("landscape", property.m_String);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_number"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_NUMBER, property.m_Type);
    ASSERT_NEAR(2.0f, property.m_Number, EPSILON);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_boolean"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_BOOLEAN, property.m_Type);
    ASSERT_FALSE(property.m_Boolean);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_vector3"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_VECTOR3, property.m_Type);
    ASSERT_VEC3(Vector3(4.0f, 5.0f, 6.0f), property.m_Vector3);

    gui_component->m_Scene->m_ApplyLayoutCallback(gui_component->m_Scene, dmHashString64("Portrait"));

    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_string"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_STRING, property.m_Type);
    ASSERT_STREQ("default", property.m_String);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_number"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_NUMBER, property.m_Type);
    ASSERT_NEAR(1.0f, property.m_Number, EPSILON);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_boolean"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_BOOLEAN, property.m_Type);
    ASSERT_TRUE(property.m_Boolean);
    ASSERT_EQ(dmGui::RESULT_OK, dmGui::GetNodeCustomProperty(gui_component->m_Scene, node, dmHashString64("test_custom_vector3"), &property));
    ASSERT_EQ(dmGui::CUSTOM_PROPERTY_TYPE_VECTOR3, property.m_Type);
    ASSERT_VEC3(Vector3(1.0f, 2.0f, 3.0f), property.m_Vector3);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGui::SetDisplayProfiles(m_GuiContext, 0);
    dmResource::Release(m_Factory, display_profiles);
}
