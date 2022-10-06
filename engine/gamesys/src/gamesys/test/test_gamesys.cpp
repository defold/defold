// Copyright 2020-2022 The Defold Foundation
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

#include "../../../../graphics/src/graphics_private.h"
#include "../../../../resource/src/resource_private.h"

#include "gamesys/resources/res_textureset.h"

#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/path.h>
#include <dlib/sys.h>

#include <ddf/ddf.h>
#include <gameobject/gameobject_ddf.h>
#include <gamesys/gamesys_ddf.h>
#include <gamesys/sprite_ddf.h>
#include "../components/comp_label.h"

#include <dmsdk/gamesys/render_constants.h>

using namespace dmVMath;

namespace dmGameSystem
{
    void DumpResourceRefs(dmGameObject::HCollection collection);
}

// Reloading these resources needs an update to clear any dirty data and get to a good state.
static const char* update_after_reload[] = {"/tile/valid.tilemapc", "/tile/valid_tilegrid_collisionobject.goc"};

#if defined(__NX__)
    #define MOUNTFS "host:/"
#else
    #define MOUNTFS ""
#endif

const char* ROOT = MOUNTFS "build/src/gamesys/test";

bool CopyResource(const char* src, const char* dst)
{
    char src_path[128];
    dmSnPrintf(src_path, sizeof(src_path), "%s/%s", ROOT, src);
    FILE* src_f = fopen(src_path, "rb");
    if (src_f == 0x0)
        return false;
    char dst_path[128];
    dmSnPrintf(dst_path, sizeof(dst_path), "%s/%s", ROOT, dst);
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
    dmSnPrintf(path, sizeof(path), "%s/%s", ROOT, name);
    return dmSys::Unlink(path) == 0;
}

static dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id, uint8_t* property_buffer, uint32_t property_buffer_size, const Point3& position, const Quat& rotation, const Vector3& scale)
{
    dmGameObject::HPrototype prototype = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&prototype) == dmResource::RESULT_OK) {
        dmGameObject::HInstance result = dmGameObject::Spawn(collection, prototype, prototype_name, id, property_buffer, property_buffer_size, position, rotation, scale);
        dmResource::Release(factory, prototype);
        return result;
    }
    return 0x0;
}

static dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id)
{
    return Spawn(factory, collection, prototype_name, id, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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

    uint32_t original_width  = dmGraphics::GetOriginalTextureWidth(resource->m_Texture);
    uint32_t original_height = dmGraphics::GetOriginalTextureHeight(resource->m_Texture);

    // Swap compiled resources to simulate an atlas update
    ASSERT_TRUE(CopyResource(texture_set_path_a, texture_set_path_tmp));
    ASSERT_TRUE(CopyResource(texture_set_path_b, texture_set_path_a));
    ASSERT_TRUE(CopyResource(texture_set_path_tmp, texture_set_path_b));

    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, texture_set_path_a, 0));

    // If the load truly was successful, we should have a new width/height for the internal image
    ASSERT_NE(original_width,dmGraphics::GetOriginalTextureWidth(resource->m_Texture));
    ASSERT_NE(original_height,dmGraphics::GetOriginalTextureHeight(resource->m_Texture));

    dmResource::Release(m_Factory, (void**) resource);
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
    dmSnPrintf(path, sizeof(path), "%s/%s", ROOT, go_name);
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
    dmSnPrintf(path, sizeof(path), "%s/%s", ROOT, go_name);
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

// Test that tries to reload shaders with errors in them.
TEST_F(ComponentTest, ReloadInvalidMaterial)
{
    const char path_material[] = "/material/valid.materialc";
    const char path_frag[] = "/fragment_program/valid.fpc";
    const char path_vert[] = "/vertex_program/valid.vpc";
    void* resource;
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_material, &resource));

    // Modify resource with simulated syntax error
    dmGraphics::SetForceVertexReloadFail(true);

    // Reload, validate fail
    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path_vert, 0));

    // Modify resource with correction
    dmGraphics::SetForceVertexReloadFail(false);

    // Reload, validate success
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path_vert, 0));

    // Same as above but for fragment shader
    dmGraphics::SetForceFragmentReloadFail(true);
    ASSERT_NE(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path_frag, 0));
    dmGraphics::SetForceFragmentReloadFail(false);
    ASSERT_EQ(dmResource::RESULT_OK, dmResource::ReloadResource(m_Factory, path_frag, 0));

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

    dmGameObject::HInstance go_consume_yes = Spawn(m_Factory, m_Collection, path_consume_yes, hash_go_consume_yes, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_consume_yes);

    dmGameObject::HInstance go_consume_no = Spawn(m_Factory, m_Collection, path_consume_no, hash_go_consume_no, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_consume_no);

    // Iteration 1: Handle proxy enable and input acquire messages from input_consume_no.script
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

TEST_P(ComponentFailTest, Test)
{
    const char* go_name = GetParam();

    dmGameObject::HInstance go = dmGameObject::New(m_Collection, go_name);
    ASSERT_EQ((void*)0, go);
}

static void DeleteInstance(dmGameObject::HCollection collection, dmGameObject::HInstance instance) {
    dmGameObject::UpdateContext ctx;
    dmGameObject::Update(collection, &ctx);
    dmGameObject::Delete(collection, instance, false);
    dmGameObject::PostUpdate(collection);
}

static void GetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t* out_val) {
    dmGameObject::PropertyDesc desc;
    dmGameObject::PropertyOptions opt;
    opt.m_Index = 0;
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(instance, comp_name, prop_name, opt, desc));
    dmGameObject::PropertyType type = desc.m_Variant.m_Type;
    ASSERT_TRUE(dmGameObject::PROPERTY_TYPE_HASH == type);
    *out_val = desc.m_Variant.m_Hash;
}

static dmGameObject::PropertyResult SetResourceProperty(dmGameObject::HInstance instance, dmhash_t comp_name, dmhash_t prop_name, dmhash_t in_val) {
    dmGameObject::PropertyVar prop_var(in_val);
    dmGameObject::PropertyOptions opt;
    opt.m_Index = 0;
    return dmGameObject::SetProperty(instance, comp_name, prop_name, opt, prop_var);
}

TEST_F(SoundTest, UpdateSoundResource)
{
    // import 'resource' lua api among others
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
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

    dmResource::SResourceDescriptor* descp = dmResource::FindByHash(m_Factory, soundata_hash);
    dmLogInfo("Original size: %d", descp->m_ResourceSize);
    ASSERT_EQ(42270+16, descp->m_ResourceSize);  // valid.wav. Size returned is always +16 from size of wav: sound_data->m_Size + sizeof(SoundData) from sound_null.cpp;

    // Update sound component with custom buffer from lua. See set_sound.script:update()
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Check the size of the updated resource

    descp = dmResource::FindByHash(m_Factory, soundata_hash);
    dmLogInfo("New size: %d", descp->m_ResourceSize);
    ASSERT_EQ(98510+16, descp->m_ResourceSize);  // replacement.wav. Size returned is always +16 from size of wav: sound_data->m_Size + sizeof(SoundData) from sound_null.cpp;

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
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* go_path = "/sound/luacallback.goc";

    // Create gameobject
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"));
    ASSERT_NE((void*)0, go);

    // Update sound component with custom buffer from lua. See set_sound.script:update()
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    // Allow for one more update for messages to go through
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    // release GO
    DeleteInstance(m_Collection, go);

    // release lua api deps
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

// Test that go.delete() does not influence other sprite animations in progress
TEST_F(SpriteAnimTest, GoDeletion)
{
    // Spawn 3 dumy game objects with one sprite in each
    dmGameObject::HInstance go1 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go1"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go2 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go2"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    dmGameObject::HInstance go3 = Spawn(m_Factory, m_Collection, "/sprite/valid_sprite.goc", dmHashString64("/go3"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go1);
    ASSERT_NE((void*)0, go2);
    ASSERT_NE((void*)0, go3);

    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go_animater = Spawn(m_Factory, m_Collection, "/sprite/sprite_property_anim.goc", dmHashString64("/go_animater"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
TEST_F(SpriteAnimTest, FlipbookAnim)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/sprite_flipbook_anim.goc", dmHashString64("/go"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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

// Test that animation done event reaches callback
TEST_F(ParticleFxTest, PlayAnim)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // Spawn one go with a script that will initiate animations on the above sprites
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/particlefx/particlefx_play.goc", dmHashString64("/go"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    lua_State* L = scriptlibcontext.m_LuaState;

    bool tests_done = false;
    int max_iter = 100;
    while (!tests_done && --max_iter > 0)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    if (max_iter <= 0)
    {
        dmLogError("The playback didn't finish");
    }

    ASSERT_TRUE(tests_done);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

static float GetFloatProperty(dmGameObject::HInstance go, dmhash_t component_id, dmhash_t property_id)
{
    dmGameObject::PropertyDesc property_desc;
    dmGameObject::PropertyOptions property_opt;
    property_opt.m_Index = 0;
    dmGameObject::GetProperty(go, component_id, property_id, property_opt, property_desc);
    return property_desc.m_Variant.m_Number;
}


TEST_F(CursorTest, GuiFlipbookCursor)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmhash_t go_id = dmHashString64("/go");
    dmhash_t gui_comp_id = dmHashString64("gui");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_flipbook_cursor.goc", go_id, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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

// Tests the animation done message/callback
TEST_F(GuiTest, GuiFlipbookAnim)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmhash_t go_id = dmHashString64("/go");
    dmhash_t gui_comp_id = dmHashString64("gui");
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/gui/gui_flipbook_anim.goc", go_id, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0x0, go);

    dmMessage::URL msg_url;
    dmMessage::ResetURL(&msg_url);
    msg_url.m_Socket = dmGameObject::GetMessageSocket(m_Collection);
    msg_url.m_Path = go_id;
    msg_url.m_Fragment = gui_comp_id;

    m_UpdateContext.m_DT = 1.0f;

    bool tests_done = false;
    int max_iter = 100;
    while (!tests_done && --max_iter > 0)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // continue test?
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    if (max_iter <= 0)
    {
        dmLogError("The playback didn't finish");
    }
    ASSERT_LT(0, max_iter);

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
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/sprite/cursor.goc", go_id, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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

TEST_F(WindowTest, MouseLock)
{
    dmHID::NewContextParams hid_params = {};
    dmHID::HContext hid_context = dmHID::NewContext(hid_params);
    dmHID::Init(hid_context);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory    = m_Factory;
    scriptlibcontext.m_Register   = m_Register;
    scriptlibcontext.m_LuaState   = dmScript::GetLuaState(m_ScriptContext);
    scriptlibcontext.m_HidContext = hid_context;

    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/window/mouse_lock.goc", dmHashString64("/mouse_lock"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_FALSE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // cleanup
    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    ASSERT_TRUE(dmGameObject::Init(m_Collection));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
    ASSERT_TRUE(dmGameObject::Final(m_Collection));

    dmGameSystem::FinalizeScriptLibs(scriptlibcontext);

    dmHID::DeleteContext(hid_context);
}

TEST_F(WindowTest, Events)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/window/window_events.goc", dmHashString64("/window_events"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
    dmHashEnableReverseHash(true);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const FactoryTestParams& param = GetParam();

    // Conditional preload. This is essentially testing async loading vs sync loading of parent collection
    // This only affects non-dynamic factories.
    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetTime() + 30*10e6;
        while (dmTime::GetTime() < stop_time)
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
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
            dmhash_t last_object_id = i == 0 ? dmHashString64("/instance1") : dmHashString64("/instance0"); // stacked index list in dynamic spawning
            for(;;)
            {
                if(dmGameObject::GetInstanceFromIdentifier(m_Collection, last_object_id) != 0x0)
                    break;
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
        // recreate resources without factoy.load having been called (sync load on demand)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(3, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));

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
    dmHashEnableReverseHash(true);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);
    const CollectionFactoryTestParams& param = GetParam();

    // Conditional preload. This is essentially testing async loading vs sync loading of parent collection
    // This only affects non-dynamic collection factories.
    dmResource::HPreloader go_pr = 0;
    if(param.m_IsPreloaded)
    {
        go_pr = dmResource::NewPreloader(m_Factory, param.m_GOPath);
        dmResource::Result r;
        uint64_t stop_time = dmTime::GetTime() + 30*10e6;
        while (dmTime::GetTime() < stop_time)
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
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, param.m_GOPath, go_hash, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- step 1 ---
        // update until instances are created through test script (collectionfactory.load and create)
        // 1) load factory resource using collectionfactory.load
        // 2) create 4 instances (two collectionfactory.create calls with a collection prototype that containes 2 references to gameobjects)
        // Do this twice in order to ensure load/unload can be called multiple times, with and without deleting created objects
        for(uint32_t i = 0; i < 2; ++i)
        {
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

            // --- step 2 ---
            // call collectionfactory.unload, derefencing 2 factory references.
            // first iteration will delete gameobjects created with factories, second will keep
            ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
            ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
            dmGameObject::PostUpdate(m_Register);
            ASSERT_EQ(i*0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
            ASSERT_EQ(i*4, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
            ASSERT_EQ(i*1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));
        }

        // --- step 3 ---
        // call collectionfactory.unload again, which is ok by design (no operation)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(4, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- step 4 ---
        // delete resources created by collectionfactory.create calls. All resource should be released
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- step 5 ---
        // recreate resources without collectionfactoy.load having been called (sync load on demand)
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(4, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

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
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));
    }
    else
    {
        // validate that resources from collection factory is loaded with the parent collection.
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(2, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- step 1 ---
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

        // --- step 2 ---
        // call collectionfactory.unload which is a no-operation for non-dynamic factories
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(8, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // Delete the root go and update so deferred deletes will be executed.
        dmGameObject::Delete(m_Collection, go, true);
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));
    }

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
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Make the render list that will be used later.
    dmRender::RenderListBegin(m_RenderContext);
    dmGameObject::Render(m_Collection);

    dmRender::RenderListEnd(m_RenderContext);
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0);

    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_EQ(expected_draw_count, dmGraphics::GetDrawCount());
    dmGraphics::Flip(m_GraphicsContext);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

/* GUI Box Render */

void AssertVertexEqual(const dmGameSystem::BoxVertex& lhs, const dmGameSystem::BoxVertex& rhs)
{
    static const float test_epsilon = 0.000001f;
    EXPECT_NEAR(lhs.m_Position[0], rhs.m_Position[0], test_epsilon);
    EXPECT_NEAR(lhs.m_Position[1], rhs.m_Position[1], test_epsilon);
    EXPECT_NEAR(lhs.m_UV[0], rhs.m_UV[0], test_epsilon);
    EXPECT_NEAR(lhs.m_UV[1], rhs.m_UV[1], test_epsilon);
}

TEST_P(BoxRenderTest, BoxRender)
{
    const BoxRenderParams& p = GetParam();
    const char* go_path = p.m_GOPath;

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    // Spawn the game object with the script we want to call
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, go_path, dmHashString64("/go"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0, 0x0);

    ASSERT_EQ(world->m_ClientVertexBuffer.Size(), (uint32_t)p.m_ExpectedVerticesCount);

    for (int i = 0; i < p.m_ExpectedVerticesCount; i++)
    {
        AssertVertexEqual(world->m_ClientVertexBuffer[i], p.m_ExpectedVertices[p.m_ExpectedIndices[i]]);
    }

    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    dmGraphics::Flip(m_GraphicsContext);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

/* Gamepad connected */

TEST_F(GamepadConnectedTest, TestGamepadConnectedInputEvent)
{
    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/input/connected_event_test.goc", dmHashString64("/gamepad_connected"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = L;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_sleepy_go = "/collision_object/sleepy_base.goc";
    dmhash_t hash_base_go = dmHashString64("/base-go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance base_go = Spawn(m_Factory, m_Collection, path_sleepy_go, hash_base_go, 0, 0, Point3(50, -10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, base_go);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body_go = "/collision_object/body.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (10,10)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body1_go, 0, 0, Point3(10,10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (50,10)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body2_go, 0, 0, Point3(50,10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
}

// Test case for collision-object properties
TEST_F(CollisionObject2DTest, PropertiesTest)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = L;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_go = "/collision_object/properties.goc";
    dmhash_t hash_go = dmHashString64("/go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance properties_go = Spawn(m_Factory, m_Collection, path_go, hash_go, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
}

TEST_P(GroupAndMask2DTest, GroupAndMaskTest )
{
    const GroupAndMaskParams& params = GetParam();
    
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = L;
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
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body2_go, hash_body2_go, 0, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);
    
    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body1_go = "/collision_object/groupmask_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
}

TEST_P(GroupAndMask3DTest, GroupAndMaskTest )
{
    const GroupAndMaskParams& params = GetParam();
    
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = L;
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
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body2_go, hash_body2_go, 0, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);
    
    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body1_go = "/collision_object/groupmask_body1.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body1_go, hash_body1_go, 0, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = L;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    // two dynamic 'body' objects will get spawned and placed apart
    const char* path_body_go = "/collision_object/body.goc";
    dmhash_t hash_body1_go = dmHashString64("/body1-go");
    // place this body standing on the base with its center at (5,5)
    dmGameObject::HInstance body1_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body1_go, 0, 0, Point3(5,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body1_go);
    dmhash_t hash_body2_go = dmHashString64("/body2-go");
    // place this body standing on the base with its center at (20,5)
    dmGameObject::HInstance body2_go = Spawn(m_Factory, m_Collection, path_body_go, hash_body2_go, 0, 0, Point3(30,5, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body2_go);

    // a 'base' gameobject works as the base for other dynamic objects to stand on
    const char* path_sleepy_go = "/collision_object/velocity_threshold_base.goc";
    dmhash_t hash_base_go = dmHashString64("/base-go");
    // place the base object so that the upper level of base is at Y = 0
    dmGameObject::HInstance base_go = Spawn(m_Factory, m_Collection, path_sleepy_go, hash_base_go, 0, 0, Point3(50, -10, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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
}


/* Physics joints */
TEST_F(ComponentTest, JointTest)
{
    /* Setup:
    ** joint_test_a
    ** - [collisionobject] collision_object/joint_test_sphere.collisionobject
    ** - [script] collision_object/joint_test.script
    ** joint_test_b
    ** - [collisionobject] collision_object/joint_test_sphere.collisionobject
    */

    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmGameSystem::ScriptLibContext scriptlibcontext;
    scriptlibcontext.m_Factory = m_Factory;
    scriptlibcontext.m_Register = m_Register;
    scriptlibcontext.m_LuaState = L;
    dmGameSystem::InitializeScriptLibs(scriptlibcontext);

    const char* path_joint_test_a = "/collision_object/joint_test_a.goc";
    const char* path_joint_test_b = "/collision_object/joint_test_b.goc";

    dmhash_t hash_go_joint_test_a = dmHashString64("/joint_test_a");
    dmhash_t hash_go_joint_test_b = dmHashString64("/joint_test_b");

    dmGameObject::HInstance go_b = Spawn(m_Factory, m_Collection, path_joint_test_b, hash_go_joint_test_b, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_b);

    dmGameObject::HInstance go_a = Spawn(m_Factory, m_Collection, path_joint_test_a, hash_go_joint_test_a, 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_a);

    // Iteration 1: Handle proxy enable and input acquire messages from input_consume_no.script
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
                                                  "/collision_object/embedded_shapes.collisionobjectc" };

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
    /*"/convex_shape/capsule.convexshapec",*/ // Temporarily disabling capsule since we are more interested in 2D atm
    "/convex_shape/hull.convexshapec",
    "/convex_shape/sphere.convexshapec",
};
INSTANTIATE_TEST_CASE_P(ConvexShape, ResourceTest, jc_test_values_in(valid_cs_resources));

ResourceFailParams invalid_cs_resources[] =
{
    {"/convex_shape/box.convexshapec", "/convex_shape/invalid_box.convexshapec"},
    {"/convex_shape/capsule.convexshapec", "/convex_shape/invalid_capsule.convexshapec"},
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

/* Fragment Program */

const char* valid_fp_resources[] = {"/fragment_program/valid.fpc"};
INSTANTIATE_TEST_CASE_P(FragmentProgram, ResourceTest, jc_test_values_in(valid_fp_resources));

ResourceFailParams invalid_fp_resources[] =
{
    {"/fragment_program/valid.fpc", "/fragment_program/missing.fpc"},
};
INSTANTIATE_TEST_CASE_P(FragmentProgram, ResourceFailTest, jc_test_values_in(invalid_fp_resources));

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

/* Light */

const char* valid_light_resources[] = {"/light/valid.lightc"};
INSTANTIATE_TEST_CASE_P(Light, ResourceTest, jc_test_values_in(valid_light_resources));

ResourceFailParams invalid_light_resources[] =
{
    {"/light/valid.lightc", "/light/missing.lightc"},
};
INSTANTIATE_TEST_CASE_P(Light, ResourceFailTest, jc_test_values_in(invalid_light_resources));

const char* valid_light_gos[] = {"/light/valid_light.goc"};
INSTANTIATE_TEST_CASE_P(Light, ComponentTest, jc_test_values_in(valid_light_gos));

const char* invalid_light_gos[] = {"/light/invalid_light.goc"};
INSTANTIATE_TEST_CASE_P(Light, ComponentFailTest, jc_test_values_in(invalid_light_gos));

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

const char* valid_meshset_resources[] = {"/meshset/valid.meshsetc", "/meshset/valid.skeletonc", "/meshset/valid.animationsetc"};
INSTANTIATE_TEST_CASE_P(MeshSet, ResourceTest, jc_test_values_in(valid_meshset_resources));

ResourceFailParams invalid_mesh_resources[] =
{
    {"/meshset/valid.meshsetc", "/meshset/missing.meshsetc"},
    {"/meshset/valid.skeletonc", "/meshset/missing.skeletonc"},
    {"/meshset/valid.animationsetc", "/meshset/missing.animationsetc"},
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

/* Vertex Program */

const char* valid_vp_resources[] = {"/vertex_program/valid.vpc"};
INSTANTIATE_TEST_CASE_P(VertexProgram, ResourceTest, jc_test_values_in(valid_vp_resources));

ResourceFailParams invalid_vp_resources[] =
{
    {"/vertex_program/valid.vpc", "/vertex_program/missing.vpc"},
};
INSTANTIATE_TEST_CASE_P(VertexProgram, ResourceFailTest, jc_test_values_in(invalid_vp_resources));

/* Label */

void AssertPointEquals(const Vector4& p, float x, float y)
{
    static const float test_epsilon = 0.000001f;
    EXPECT_NEAR(p.getX(), x, test_epsilon);
    EXPECT_NEAR(p.getY(), y, test_epsilon);
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
    "/model/invalid_vertexspace.modelc",
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
    {"/factory/dynamic_factory_test.goc", true, true},
    {"/factory/dynamic_factory_test.goc", true, false},
    {"/factory/factory_test.goc", false, true},
    {"/factory/factory_test.goc", false, false},
};
INSTANTIATE_TEST_CASE_P(Factory, FactoryTest, jc_test_values_in(factory_testparams));

/* Validate default and dynamic collection factories */

CollectionFactoryTestParams collection_factory_testparams [] =
{
    {"/collection_factory/dynamic_collectionfactory_test.goc", true, true},
    {"/collection_factory/dynamic_collectionfactory_test.goc", true, false},
    {"/collection_factory/collectionfactory_test.goc", false, true},
    {"/collection_factory/collectionfactory_test.goc", false, false},
};
INSTANTIATE_TEST_CASE_P(CollectionFactory, CollectionFactoryTest, jc_test_values_in(collection_factory_testparams));

/* Validate draw count for different GOs */

DrawCountParams draw_count_params[] =
{
    {"/gui/draw_count_test.goc", 1},
    {"/gui/draw_count_test2.goc", 1},
};
INSTANTIATE_TEST_CASE_P(DrawCount, DrawCountTest, jc_test_values_in(draw_count_params));

/* Validate gui box rendering for different GOs. */

BoxRenderParams box_render_params[] =
{
    // 9-slice params: on | Use geometries: 8 | Flip uv: off | Texture: tilesource animation
    {
        "/gui/render_box_test1.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -16.000000, 0.0, 0.0), 0.031250, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -14.000000, 0.0, 0.0), 0.031250, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -14.000000, 0.0, 0.0), 0.000000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, -16.000000, 0.0, 0.0), 0.468750, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, -14.000000, 0.0, 0.0), 0.468750, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -14.000000, 0.0, 0.0), 0.500000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 14.000000, 0.0, 0.0), 0.031250, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 14.000000, 0.0, 0.0), 0.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, 14.000000, 0.0, 0.0), 0.468750, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 14.000000, 0.0, 0.0), 0.500000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 16.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, 16.000000, 0.0, 0.0), 0.468750, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        54,
        {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2, 4, 6, 7, 4, 7, 5, 3, 2, 8, 3, 8, 9, 2, 5, 10, 2, 10, 8, 5, 7, 11, 5, 11, 10, 9, 8, 12, 9, 12, 13, 8, 10, 14, 8, 14, 12, 10, 11, 15, 10, 15, 14}
    },
    // 9-slice params: off | Use geometries: 8 | Flip uv: off | Texture: tilesource animation
    {
        "/gui/render_box_test2.goc",
        {
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        18,
        {0, 1, 2, 0, 2, 2, 0, 2, 3, 0, 3, 3, 0, 3, 3, 0, 3, 3}
    },
    // 9-slice params: off | Use geometries: off | Flip uv: off | Texture: tilesource animation
    {
        "/gui/render_box_test3.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        6,
        {0, 1, 2, 0, 2, 3}
    },
    // 9-slice params: off | Use geometries: off | Flip uv: u | Texture: tilesource animation
    {
        "/gui/render_box_test4.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        6,
        {0, 1, 2, 0, 2, 3}
    },
    // 9-slice params: off | Use geometries: off | Flip uv: uv | Texture: tilesource animation
    {
        "/gui/render_box_test5.goc",
        {
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        6,
        {0, 1, 2, 0, 2, 3}
    },
    // 9-slice params: on | Use geometries: 8 | Flip uv: uv | Texture: tilesource animation
    {
        "/gui/render_box_test6.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-16.000000, -16.000000, 0.0, 0.0), 0.500000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -16.000000, 0.0, 0.0), 0.468750, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, -14.000000, 0.0, 0.0), 0.468750, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, -14.000000, 0.0, 0.0), 0.500000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, -16.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, -14.000000, 0.0, 0.0), 0.031250, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -16.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, -14.000000, 0.0, 0.0), 0.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 14.000000, 0.0, 0.0), 0.468750, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 14.000000, 0.0, 0.0), 0.500000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, 14.000000, 0.0, 0.0), 0.031250, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 14.000000, 0.0, 0.0), 0.000000, 0.531250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-14.000000, 16.000000, 0.0, 0.0), 0.468750, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-16.000000, 16.000000, 0.0, 0.0), 0.500000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(14.000000, 16.000000, 0.0, 0.0), 0.031250, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(16.000000, 16.000000, 0.0, 0.0), 0.000000, 0.500000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        54,
        {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2, 4, 6, 7, 4, 7, 5, 3, 2, 8, 3, 8, 9, 2, 5, 10, 2, 10, 8, 5, 7, 11, 5, 11, 10, 9, 8, 12, 9, 12, 13, 8, 10, 14, 8, 14, 12, 10, 11, 15, 10, 15, 14}
    },
    // 9-slice params: on | Use geometries: na | Flip uv: na | Texture: none
    {
        "/gui/render_box_test7.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-1.000000, -1.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(1.000000, -1.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-1.000000, 1.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(1.000000, 1.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        6,
        {0, 1, 3, 0, 3, 2}
    },
    // 9-slice params: off | Use geometries: na | Flip uv: na | Texture: script
    {
        "/gui/render_box_test8.goc",
        {
            dmGameSystem::BoxVertex(Vector4(68.000000, 68.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(132.000000, 68.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(68.000000, 132.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(132.000000, 132.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        6,
        {0, 1, 3, 0, 3, 2}
    },
    // 9-slice params: on | Use geometries: na | Flip uv: na | Texture: script
    {
        "/gui/render_box_test9.goc",
        {
            dmGameSystem::BoxVertex(Vector4(68.000000, 68.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(70.000000, 68.000000, 0.0, 0.0), 0.031250, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(70.000000, 70.000000, 0.0, 0.0), 0.031250, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(68.000000, 70.000000, 0.0, 0.0), 0.000000, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(130.000000, 68.000000, 0.0, 0.0), 0.968750, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(130.000000, 70.000000, 0.0, 0.0), 0.968750, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(132.000000, 68.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(132.000000, 70.000000, 0.0, 0.0), 1.000000, 0.031250, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(70.000000, 130.000000, 0.0, 0.0), 0.031250, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(68.000000, 130.000000, 0.0, 0.0), 0.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(130.000000, 130.000000, 0.0, 0.0), 0.968750, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(132.000000, 130.000000, 0.0, 0.0), 1.000000, 0.968750, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(70.000000, 132.000000, 0.0, 0.0), 0.031250, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(68.000000, 132.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(130.000000, 132.000000, 0.0, 0.0), 0.968750, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(132.000000, 132.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        54,
        {0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2, 4, 6, 7, 4, 7, 5, 3, 2, 8, 3, 8, 9, 2, 5, 10, 2, 10, 8, 5, 7, 11, 5, 11, 10, 9, 8, 12, 9, 12, 13, 8, 10, 14, 8, 14, 12, 10, 11, 15, 10, 15, 14}
    },
    // 9-slice params: off | Use geometries: na | Flip uv: na | Texture: none
    {
        "/gui/render_box_test10.goc",
        {
            dmGameSystem::BoxVertex(Vector4(-1.000000, -1.000000, 0.0, 0.0), 0.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(1.000000, -1.000000, 0.0, 0.0), 1.000000, 0.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(-1.000000, 1.000000, 0.0, 0.0), 0.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0)),
            dmGameSystem::BoxVertex(Vector4(1.000000, 1.000000, 0.0, 0.0), 1.000000, 1.000000, Vector4(1.0, 1.0, 1.0, 1.0))
        },
        6,
        {0, 1, 3, 0, 3, 2}
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


static bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

TEST_F(ScriptBufferTest, PushCheckBuffer)
{
    int top = lua_gettop(L);
    dmScript::LuaHBuffer luabuf(m_Buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    dmScript::LuaHBuffer* buffer_ptr = dmScript::CheckBuffer(L, -1);
    ASSERT_NE((void*)0x0, buffer_ptr);
    ASSERT_EQ(m_Buffer, buffer_ptr->m_Buffer);
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
#if !defined(__SANITIZE_ADDRESS__)
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

TEST_F(RenderConstantsTest, SetGetConstant)
{
    dmhash_t name_hash1 = dmHashString64("user_var1");
    dmhash_t name_hash2 = dmHashString64("user_var2");

    dmRender::HMaterial material = 0;
    {
        const char path_material[] = "/material/valid.materialc";
        ASSERT_EQ(dmResource::RESULT_OK, dmResource::Get(m_Factory, path_material, (void**)&material));
        ASSERT_NE((void*)0, material);

        dmRender::HConstant rconstant;
        ASSERT_TRUE(dmRender::GetMaterialProgramConstant(material, name_hash1, rconstant));
        ASSERT_TRUE(dmRender::GetMaterialProgramConstant(material, name_hash2, rconstant));
    }

    dmGameSystem::HComponentRenderConstants constants = dmGameSystem::CreateRenderConstants();

    dmRender::HConstant constant = 0;
    bool result = dmGameSystem::GetRenderConstant(constants, name_hash1, &constant);
    ASSERT_FALSE(result);
    ASSERT_EQ(0, constant);

    // Setting property value
    dmGameObject::PropertyVar var1(dmVMath::Vector4(1,2,3,4));
    dmGameSystem::SetRenderConstant(constants, material, name_hash1, 0, 0, var1); // stores the previous value

    result = dmGameSystem::GetRenderConstant(constants, name_hash1, &constant);
    ASSERT_TRUE(result);
    ASSERT_NE((void*)0, constant);
    ASSERT_EQ(name_hash1, constant->m_NameHash);

    // Issue in 1.2.183: We reallocated the array, thus invalidating the previous pointer
    dmGameObject::PropertyVar var2(dmVMath::Vector4(5,6,7,8));
    dmGameSystem::SetRenderConstant(constants, material, name_hash2, 0, 0, var2);
    // Make sure it's still valid and doesn't trigger an ASAN issue
    ASSERT_EQ(name_hash1, constant->m_NameHash);

    ASSERT_NE(0, dmGameSystem::ClearRenderConstant(constants, name_hash1)); // removed
    ASSERT_EQ(0, dmGameSystem::ClearRenderConstant(constants, name_hash1)); // not removed
    ASSERT_NE(0, dmGameSystem::ClearRenderConstant(constants, name_hash2));
    ASSERT_EQ(0, dmGameSystem::ClearRenderConstant(constants, name_hash2));

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


int main(int argc, char **argv)
{
    dmHashEnableReverseHash(true);
    // Enable message descriptor translation when sending messages
    dmDDF::RegisterAllTypes();

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
