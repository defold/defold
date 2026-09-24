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

class GamesysErrorLogCapture;
static void CaptureGamesysErrorLog(LogSeverity severity, const char* domain, const char* formatted_string);
static GamesysErrorLogCapture* g_GamesysErrorLogCapture = 0;

static bool WaitForGamesysErrorLogCapture()
{
    uint64_t stop_time = dmTime::GetMonotonicTime() + 5000000;
    while (dmLog::GetPendingLogCount() != 0 && dmTime::GetMonotonicTime() < stop_time)
    {
        dmTime::Sleep(1000);
    }
    return dmLog::GetPendingLogCount() == 0;
}

class GamesysErrorLogCapture
{
public:
    GamesysErrorLogCapture()
    {
        assert(g_GamesysErrorLogCapture == 0);
        g_GamesysErrorLogCapture = this;
        dmLogRegisterListener(CaptureGamesysErrorLog);
        WaitForGamesysErrorLogCapture();
        m_Output.SetSize(0);
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
        return WaitForGamesysErrorLogCapture() && m_Output.Size() == 0;
    }

    bool Contains(const char* needle)
    {
        WaitForGamesysErrorLogCapture();
        return strstr(Output(), needle) != 0;
    }

    const char* Output()
    {
        WaitForGamesysErrorLogCapture();
        if (m_Output.Size() == 0)
        {
            return "";
        }
        if (m_Output[m_Output.Size() - 1] != '\0')
        {
            m_Output.OffsetCapacity(1);
            m_Output.Push('\0');
        }
        return m_Output.Begin();
    }

private:
    dmArray<char> m_Output;
};

static bool GamesysErrorLogCaptureEmpty(GamesysErrorLogCapture& log_capture)
{
    bool empty = log_capture.Empty();
    if (!empty)
    {
        printf("Unexpected GAMESYS error log output:\n%s", log_capture.Output());
    }
    return empty;
}

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

static void PostSpriteFlip(dmGameObject::HCollection collection, dmhash_t go_id, dmhash_t component_id, bool horizontal, bool vertical)
{
    dmMessage::URL msg_url;
    dmMessage::ResetURL(&msg_url);
    msg_url.m_Socket = dmGameObject::GetMessageSocket(collection);
    msg_url.m_Path = go_id;
    msg_url.m_Fragment = component_id;

    dmGameSystemDDF::SetFlipHorizontal horizontal_msg;
    horizontal_msg.m_Flip = horizontal;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::PostDDF(&horizontal_msg, &msg_url, &msg_url, 0, 0, 0));

    dmGameSystemDDF::SetFlipVertical vertical_msg;
    vertical_msg.m_Flip = vertical;
    ASSERT_EQ(dmMessage::RESULT_OK, dmMessage::PostDDF(&vertical_msg, &msg_url, &msg_url, 0, 0, 0));
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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
        ASSERT_NE(0, go);

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

TEST_F(ResourceComponentTest, ModelTexturePropertyAllTextureSlots)
{
    const char* go_path = "/resource/res_getset_prop.goc";
    const char* tex_path = "/resource/texture_valid_png.texturec";
    dmhash_t tex_hash = dmHashString64(tex_path);
    dmhash_t comp_name = dmHashString64("model");

    void* tex_res = 0x0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, tex_path, &tex_res));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE(0, go);

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

// A sprite with a sampler-free material should update and render without a texture set.
TEST_F(SpriteTest, TexturelessMaterial)
{
    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t sprite_id = dmHashString64("sprite");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/textureless.goc", go_id);
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    PostSpritePlayAnimation(m_Collection, go_id, sprite_id, dmHashString64("anim"), 0.0f, 1.0f);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::PropertyOptions options;
    dmGameObject::PropertyVar cursor(0.5);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, sprite_id, dmHashString64("cursor"), options, cursor));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    RenderCollection(m_RenderContext, m_Collection);
}

// Test that go.delete() does not influence other sprite animations in progress
TEST_F(SpriteTest, GoDeletion)
{
    // Spawn 3 dumy game objects with one sprite in each
    dmGameObject::HInstance go1 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go1"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go2 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go2"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go3 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go3"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go1);
    ASSERT_NE(0, go2);
    ASSERT_NE(0, go3);

    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go_animater = Spawn(m_Factory, m_Collection, "/sprite/sprite_property_anim.goc", dmHashString64("/go_animater"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_animater);

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
    ASSERT_NE(0, go);

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
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/frame_count_sprite_frame_count.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    WaitForTestsDone(100, false, 0);

    dmGameObject::Delete(m_Collection, go, true);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
}

TEST_F(SpriteTest, GetSetSliceProperty)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/sprite_slice9.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, Slice9FlipGeometry)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    const dmhash_t go_id = dmHashString64("/go");
    const dmhash_t sprite_id = dmHashString64("sprite");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/sprite_slice9.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    // Run script init before replacing the slice value used by its property test.
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::PropertyOptions options;
    dmGameObject::PropertyVar slice(Vector4(10.0f, 20.0f, 30.0f, 40.0f));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, sprite_id, dmHashString64("slice"), options, slice));
    PostSpriteFlip(m_Collection, go_id, sprite_id, true, true);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    RenderCollection(m_RenderContext, m_Collection);

    dmGameSystem::MaterialResource* material_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/sprite/sprite.materialc", (void**)&material_resource));
    dmGraphics::HVertexDeclaration vertex_declaration = dmRender::GetVertexDeclaration(material_resource->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    const uint32_t vertex_stride = dmGraphics::GetVertexDeclarationStride(vertex_declaration);
    const uint32_t position_offset = dmGraphics::GetVertexStreamOffset(vertex_declaration, dmHashString64("position"));
    ASSERT_NE(dmGraphics::INVALID_STREAM_OFFSET, position_offset);

    void* sprite_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
    ASSERT_NE((void*)0, sprite_world);
    dmRender::BufferedRenderBuffer* vertex_buffer = 0;
    dmRender::BufferedRenderBuffer* index_buffer = 0;
    dmGameSystem::GetSpriteWorldRenderBuffers(sprite_world, &vertex_buffer, &index_buffer);
    ASSERT_NE((void*)0, vertex_buffer);
    ASSERT_EQ(1u, vertex_buffer->m_Buffers.Size());

    const uint32_t vertex_count = 16;
    dmGraphics::HVertexBuffer vertex_buffer_handle = vertex_buffer->m_Buffers[0];
    ASSERT_EQ(vertex_count * vertex_stride, dmGraphics::GetVertexBufferSize(vertex_buffer_handle));
    const char* vertex_data = ((dmGraphics::VertexBuffer*)vertex_buffer_handle)->m_Buffer;

    // Original margins are left=10, top=20, right=30, bottom=40. Both
    // flips move the right/bottom margins to the left/top respectively.
    const float expected_x[4] = {-128.0f, -98.0f, 118.0f, 128.0f};
    const float expected_y[4] = {-128.0f, -108.0f, 88.0f, 128.0f};
    for (uint32_t i = 0; i < vertex_count; ++i)
    {
        const char* position = vertex_data + i * vertex_stride + position_offset;
        ASSERT_NEAR(expected_x[i % 4], ReadUnalignedFloat(position), EPSILON);
        ASSERT_NEAR(expected_y[i / 4], ReadUnalignedFloat(position + sizeof(float)), EPSILON);
    }

    dmResource::Release(m_Factory, material_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

/*
 * Intent:
 * Verify that sprite vertex staging memory is grown using the active render
 * material's vertex stride before vertex data is generated.
 *
 * Setup:
 * Create a sprite whose component material has a scalar custom attribute,
 * then draw it through a render script that overrides the material with one
 * where the same attribute is a mat4. Confirm that the staging buffer was
 * initially sized from the smaller component-material stride.
 *
 * Expected results:
 * Rendering grows the staging buffer to fit four vertices using the larger
 * override-material stride, and the complete vertex data is uploaded without
 * writing beyond the allocated staging memory.
 */
TEST_F(SpriteTest, RenderScriptMaterialOverrideGrowsVertexBuffer)
{
    dmHashEnableReverseHash(true);

    dmRender::RenderContext* render_context_ptr = (dmRender::RenderContext*) m_RenderContext;
    render_context_ptr->m_MultiBufferingRequired = 1;

    dmGameSystem::MaterialResource* mat4_material_resource = 0;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/sprite/attribute_stride_mat4.materialc", (void**) &mat4_material_resource));

    const char* render_script_source =
        "function init(self)\n"
        "    self.predicate = render.predicate({\"tile\"})\n"
        "end\n"
        "function update(self)\n"
        "    render.enable_material(\"attribute_stride_mat4\")\n"
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
    lua_source.m_Filename = "sprite-attribute-stride-render-script";

    dmRender::HRenderScript render_script = dmRender::NewRenderScript(m_RenderContext, &lua_source);
    ASSERT_NE((dmRender::HRenderScript) 0, render_script);
    dmRender::HRenderScriptInstance render_script_instance = dmRender::NewRenderScriptInstance(m_RenderContext, render_script);
    ASSERT_NE((dmRender::HRenderScriptInstance) 0, render_script_instance);
    dmRender::AddRenderScriptInstanceRenderResource(render_script_instance, "attribute_stride_mat4", (uint64_t) mat4_material_resource->m_Material, dmRender::RENDER_RESOURCE_TYPE_MATERIAL);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::InitRenderScriptInstance(render_script_instance));

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/attribute_stride.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::DispatchRenderScriptInstance(render_script_instance));
    dmRender::RenderListEnd(m_RenderContext);

    void* sprite_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("spritec")));
    ASSERT_NE((void*) 0, sprite_world);

    const uint32_t vertex_count = 4;
    dmGraphics::HVertexDeclaration mat4_vertex_declaration = dmRender::GetVertexDeclaration(mat4_material_resource->m_Material, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX);
    const uint32_t mat4_vertex_stride = dmGraphics::GetVertexDeclarationStride(mat4_vertex_declaration);
    const uint32_t required_capacity = vertex_count * mat4_vertex_stride;
    ASSERT_LT(dmGameSystem::GetSpriteWorldVertexBufferCapacity(sprite_world), required_capacity);

    dmGraphics::BeginFrame(m_GraphicsContext);
    ASSERT_EQ(dmRender::RENDER_SCRIPT_RESULT_OK, dmRender::UpdateRenderScriptInstance(render_script_instance, m_UpdateContext.m_DT));
    ASSERT_EQ(required_capacity, dmGameSystem::GetSpriteWorldVertexBufferCapacity(sprite_world));

    dmRender::BufferedRenderBuffer* vertex_buffer = 0;
    dmRender::BufferedRenderBuffer* index_buffer = 0;
    dmGameSystem::GetSpriteWorldRenderBuffers(sprite_world, &vertex_buffer, &index_buffer);
    ASSERT_EQ(1u, vertex_buffer->m_Buffers.Size());
    ASSERT_EQ(required_capacity, dmGraphics::GetVertexBufferSize(vertex_buffer->m_Buffers[0]));

    dmRender::DeleteRenderScriptInstance(render_script_instance);
    dmRender::DeleteRenderScript(m_RenderContext, render_script);
    dmResource::Release(m_Factory, mat4_material_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, GetSetImagesByHash)
{
    void* atlas=0;
    dmResource::Result res = dmResource::Get(m_Factory, "/sprite/atlas_valid_64x64.a.texturesetc", &atlas);
    ASSERT_EQ(dmResource::RESULT_OK, res);

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/image_get_set_image_by_hash.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
    dmhash_t atlas_b = dmHashString64("/sprite/atlas_valid_64x64.a.texturesetc");
    dmhash_t animation_b = dmHashString64("atlas_texture_valid_png");
    dmGameSystem::TextureSetResource* atlas_resource = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/sprite/atlas_valid_64x64.a.texturesetc", (void**) &atlas_resource));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/image_get_set_image_by_hash_noscript.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

        ASSERT_TRUE(GamesysErrorLogCaptureEmpty(log_capture));
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
    ASSERT_NE(0, go);
    ASSERT_EQ(shared_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

    {
        GamesysErrorLogCapture log_capture;

        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, SetHashProperty(go, sprite_comp_id, image_prop_id, new_image_id));
        ASSERT_EQ(shared_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));

        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        RenderCollection(m_RenderContext, m_Collection);

        ASSERT_EQ(shared_animation_id, GetHashProperty(go, sprite_comp_id, animation_prop_id));
        ASSERT_TRUE(GamesysErrorLogCaptureEmpty(log_capture));
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
    ASSERT_NE(0, go);

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

        ASSERT_TRUE(GamesysErrorLogCaptureEmpty(log_capture));
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
    dmhash_t new_image_id = dmHashString64("/sprite/image_stale_animation_id_in_range.a.texturesetc");
    dmGameSystem::TextureSetResource* image_resource = 0;

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/sprite/image_stale_animation_id_in_range.a.texturesetc", (void**) &image_resource));
    ASSERT_TRUE(image_resource->m_TextureSet->m_Animations.m_Count > 6);
    ASSERT_NE(dmGameSystemDDF::SPRITE_TRIM_MODE_OFF, image_resource->m_TextureSet->m_Geometries[0].m_TrimMode);
    ASSERT_EQ((uint32_t*)0, image_resource->m_AnimationIds.Get(old_animation_id));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/cursor.goc", go_id, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
        ASSERT_TRUE(GamesysErrorLogCaptureEmpty(log_capture));
    }

    dmResource::Release(m_Factory, image_resource);
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(SpriteTest, ScaleAffectsWorldSize)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

    bool tests_done = false;
    WaitForTestsDone(100, true, &tests_done);

    if (!tests_done)
    {
        dmLogError("The playback didn't finish");
    }
    ASSERT_TRUE(tests_done);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

// Verify that particlefx.play() can capture and invoke an emitter callback when called from a coroutine.
// The callback must be read from the coroutine's Lua stack; reading it from the main thread's stack
// causes coroutine.resume() to fail with an unrelated value as its error object.
TEST_F(ParticleFxTest, PlayAnimFromCoroutine)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/particlefx/particlefx_play_from_coroutine.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmResource::Release(m_Factory, material);
}

TEST_F(ParticleFxTest, PlayWithOverridesAfterComponentStorageGrowth)
{
    dmGameObject::DeleteCollections(m_Register);

    dmGameObjectDDF::ComponenTypeDesc component_types[2] = {};
    component_types[0].m_NameHash = dmHashString64("goc");
    component_types[0].m_MaxCount = 32;
    component_types[1].m_NameHash = dmHashString64("particlefxc");
    component_types[1].m_MaxCount = 1;

    dmGameObjectDDF::CollectionDesc collection_desc = {};
    collection_desc.m_Name = "particlefx_low_component_capacity";
    collection_desc.m_ComponentTypes.m_Data = component_types;
    collection_desc.m_ComponentTypes.m_Count = sizeof(component_types) / sizeof(component_types[0]);

    m_Collection = dmGameObject::NewCollection(collection_desc.m_Name, m_Factory, m_Register, m_projectOptions.m_MaxInstances, &collection_desc);
    ASSERT_NE(dmGameObject::INVALID_COLLECTION, m_Collection);
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/particlefx/valid_particlefx.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

    dmGameObject::PropertyOptions options;
    ASSERT_TRUE(dmGameObject::AddPropertyOptionsKey(&options, dmHashString64("emitter")));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::SetProperty(go, dmHashString64("particlefx"), dmHashString64("animation"), options, dmGameObject::PropertyVar(dmHashString64("anim"))));

    dmMessage::URL receiver;
    receiver.m_Socket   = dmGameObject::GetMessageSocket(m_Collection);
    receiver.m_Path     = dmGameObject::GetIdentifier(go);
    receiver.m_Fragment = dmHashString64("particlefx");

    for (uint32_t i = 0; i < 16; ++i)
    {
        dmMessage::Post(
            0, &receiver,
            dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash,
            (uintptr_t)go,
            (uintptr_t)dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor,
            0, 0, 0);
    }

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ParticleFxTest, FrustumCullsParticleEmitters)
{
    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go_inside = Spawn(m_Factory, m_Collection, "/particlefx/valid_particlefx.goc", dmHashString64("/go_inside"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go_outside = Spawn(m_Factory, m_Collection, "/particlefx/valid_particlefx.goc", dmHashString64("/go_outside"), 0, Point3(1000, 1000, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_inside);
    ASSERT_NE(0, go_outside);

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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
            "/factory/sprite_valid.spritec",
            "/factory/tile_valid.t.texturesetc",
            "/factory/sprite_sprite.materialc",
    };
    const char* dyn_prototype_empty_resource_path[] = {
            "/factory/empty.goc",
    };
    const char* dyn_prototype_sprite_resource_path[] = {
            "/factory/dynamic_prototype_sprite.goc",
            "/factory/sprite_valid.spritec",
            "/factory/tile_valid.t.texturesetc",
            "/factory/sprite_sprite.materialc",
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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);
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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);

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
            "/collection_factory/sprite_valid.spritec", // single instance (subresource of go's)
            "/collection_factory/tile_valid.t.texturesetc", // single instance (subresource of sprite)
            "/collection_factory/sprite_sprite.materialc", // single instance (subresource of sprite)
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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);
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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);
    go = dmGameObject::GetInstanceFromIdentifier(m_Collection, go_hash);
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

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
    ASSERT_NE(0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    // test gamepad connected with device name and connected flag
    dmGameObject::InputAction input_action_connected;
    input_action_connected.m_ActionId = dmHashString64("gamepad_connected");
    input_action_connected.m_GamepadConnected = 1;
    input_action_connected.m_Count = dmStrlCpy(input_action_connected.m_Text, "null_device", sizeof(input_action_connected.m_Text));
    dmGameObject::UpdateResult res = dmGameObject::DispatchInput(m_Collection, &input_action_connected, 1);

    ASSERT_TRUE(res == dmGameObject::UpdateResult::UPDATE_RESULT_OK);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // test gamepad connected with empty device name
    dmGameObject::InputAction input_action_empty;
    input_action_empty.m_ActionId = dmHashString64("gamepad_connected_0");
    input_action_empty.m_Count = 0;
    input_action_empty.m_GamepadConnected = 1;
    res = dmGameObject::DispatchInput(m_Collection, &input_action_empty, 1);

    ASSERT_TRUE(res == dmGameObject::UpdateResult::UPDATE_RESULT_OK);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // test gamepad connected with device name and no connected flag
    dmGameObject::InputAction input_action_other;
    input_action_other.m_ActionId = dmHashString64("other_event");
    input_action_other.m_GamepadConnected = 0;
    input_action_other.m_Count = dmStrlCpy(input_action_other.m_Text, "null_device", sizeof(input_action_other.m_Text));
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
    ASSERT_NE(0, base_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body_go = "/collision_object/body.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (10,10)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body1_go, 0, Point3(10,10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, body1_go);
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (50,10)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body2_go, 0, Point3(50,10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, body2_go);


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
    ASSERT_NE(0, properties_go);

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
    ASSERT_NE(0, trigger_go);

    const char* path_body1_go = "/collision_object/eventtrigger_false_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, body1_go);

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
    ASSERT_NE(0, body2_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body1_go = "/collision_object/groupmask_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, body1_go);

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
    ASSERT_NE(0, body2_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body1_go = "/collision_object/groupmask_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, body1_go);

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
    ASSERT_NE(0, body1_go);
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (20,5)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body2_go, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, body2_go);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_sleepy_go = "/collision_object/velocity_threshold_base.goc";
    dmhash_t hash_base_go = dmHashString64("/base-go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance base_go = Spawn(m_Factory, m_Collection, path_sleepy_go, hash_base_go, 0, Point3(50, -10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, base_go);

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

TEST_F(MiscComponentTest, DispatchBuffersTest)
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
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/misc/dispatch_buffers_test_dispatch_buffers_test.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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

    // Vertex format for /misc/dispatch_buffers_test_vs_format_a.vp:
    // attribute vec4 position;
    // attribute float page_index;
    struct vs_format_a
    {
        float position[3];
        float page_index;
    };

    // Vertex format for /misc/dispatch_buffers_test_vs_format_b.vp:
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
            dmGraphics::HVertexBuffer vx_buffer_handle = vx_buffer->m_Buffers[i];
            dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer_handle;
            ASSERT_EQ(buffer_size, dmGraphics::GetVertexBufferSize(vx_buffer_handle));

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

        SET_VTX_A(model_a[0], p2[0], p2[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[1], p3[0], p3[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[2], p0[0], p0[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[3], p2[0], p2[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[4], p0[0], p0[1], 1.0f, 0.0f);
        SET_VTX_A(model_a[5], p1[0], p1[1], 1.0f, 0.0f);

        SET_VTX_B(model_b[0], p2[0], p2[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[1], p3[0], p3[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[2], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[3], p2[0], p2[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[4], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(model_b[5], p1[0], p1[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);

        for (int i = 0; i < num_draws; ++i)
        {
            // TODO: Maybe validate index buffer here as well
            dmGraphics::HVertexBuffer vx_buffer_handle = vx_buffer->m_Buffers[i];
            dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer_handle;
            ASSERT_EQ(buffer_size, dmGraphics::GetVertexBufferSize(vx_buffer_handle));

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
        SET_VTX_A(pfx_a[1], p2[0], p2[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[2], p3[0], p3[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[3], p3[0], p3[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[4], p1[0], p1[1], 1.0f, 0.0f);
        SET_VTX_A(pfx_a[5], p0[0], p0[1], 1.0f, 0.0f);

        SET_VTX_B(pfx_b[0], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[1], p2[0], p2[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[2], p3[0], p3[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[3], p3[0], p3[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[4], p1[0], p1[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);
        SET_VTX_B(pfx_b[5], p0[0], p0[1], 0.0f, 4.0f, 3.0f, 2.0f, 1.0f);

        for (int i = 0; i < num_draws; ++i)
        {
            dmGraphics::HVertexBuffer vx_buffer_handle = vx_buffer->m_Buffers[i];
            dmGraphics::VertexBuffer* gfx_vx_buffer = (dmGraphics::VertexBuffer*) vx_buffer_handle;
            ASSERT_EQ(buffer_size, dmGraphics::GetVertexBufferSize(vx_buffer_handle));

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

TEST_F(ParticleFxComponentTest, ParticleFXRenderScriptMaterialOverrideAttributeSizeMismatch)
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
    ASSERT_NE(0, go);

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

TEST_F(MiscComponentTest, DispatchBuffersInstancingTest)
{
    dmHashEnableReverseHash(true);

    dmRender::RenderContext* render_context_ptr  = (dmRender::RenderContext*) m_RenderContext;

    render_context_ptr->m_MultiBufferingRequired = 1;

    void* model_world = dmGameObject::GetWorld(m_Collection, dmGameObject::GetComponentTypeIndex(m_Collection, dmHashString64("modelc")));
    ASSERT_NE((void*) 0, model_world);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/misc/dispatch_buffers_instancing_test_dispatch_buffers_instancing_test.goc", dmHashString64("/go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go);

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
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/misc/dispatch_buffers_instancing_test_material_a.materialc", (void**) &material_a));
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, "/misc/dispatch_buffers_instancing_test_material_b.materialc", (void**) &material_b));

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

TEST_F(ComponentTest, GetSetCollisionShape)
{
    dmHashEnableReverseHash(true);

    dmGameObject::HInstance go_base = Spawn(m_Factory, m_Collection, "/collision_object/get_set_shape.goc", dmHashString64("/get_set_shape_go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_base);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(ComponentTest, GetSetErrorCollisionShape)
{
    dmHashEnableReverseHash(true);

    dmGameObject::HInstance go_base = Spawn(m_Factory, m_Collection, "/collision_object/get_set_error_shape.goc", dmHashString64("/get_set_error_shape_go"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE(0, go_base);

    ASSERT_FALSE(dmGameObject::Final(m_Collection));
}
