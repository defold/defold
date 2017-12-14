#include "test_gamesys.h"

#include "../../../../graphics/src/graphics_private.h"
#include "../../../../resource/src/resource_private.h"

#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/path.h>

#include <gameobject/gameobject_ddf.h>

namespace dmGameSystem
{
    void DumpResourceRefs(dmGameObject::HCollection collection);
}


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

static dmGameSystem::RenderScriptPrototype* LoadRenderFile(dmResource::HFactory factory, const char* prototype_name)
{
    dmGameSystem::RenderScriptPrototype* render_script_res = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&render_script_res) == dmResource::RESULT_OK) {
        dmRender::RenderScriptResult script_result = InitRenderScriptInstance(render_script_res->m_Instance);
        if (script_result != dmRender::RENDER_SCRIPT_RESULT_OK) {
            dmLogFatal("Render script could not be initialized.");
            return 0x0;
        }
        return render_script_res;
    }

    return 0x0;
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

// Test getting texture0 properties on components.
TEST_P(TexturePropTest, GetTextureProperty)
{
    const TexturePropParams& p =  GetParam();

    dmhash_t hash_comp_1_1 = p.comp_same_1;
    dmhash_t hash_comp_1_2 = p.comp_same_2;
    dmhash_t hash_comp_2   = p.comp_different;

    dmGameObject::PropertyDesc prop_value1, prop_value2;

    // Spawn a go with three components, two with same texture and one with a unique.
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, p.go_path, dmHashString64("/go"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    // Valid property
    dmGameObject::PropertyResult prop_res = dmGameObject::GetProperty(go, hash_comp_1_1, hash_property_id, prop_value1);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, prop_res);
    ASSERT_EQ(dmGameObject::PROPERTY_TYPE_HASH, prop_value1.m_Variant.m_Type);

    // Invalid property
    prop_res = dmGameObject::GetProperty(go, hash_comp_1_1, hash_property_id_invalid, prop_value1);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_NOT_FOUND, prop_res);

    // Compare comp_1_1 and comp_1_2 which need to have the same texture.
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, hash_comp_1_1, hash_property_id, prop_value1));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, hash_comp_1_2, hash_property_id, prop_value2));
    ASSERT_EQ(prop_value1.m_Variant.m_Hash, prop_value2.m_Variant.m_Hash);

    // Compare comp_1_1 and comp_2 which don't have the same texture.
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, hash_comp_1_1, hash_property_id, prop_value1));
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, dmGameObject::GetProperty(go, hash_comp_2, hash_property_id, prop_value2));
    ASSERT_NE(prop_value1.m_Variant.m_Hash, prop_value2.m_Variant.m_Hash);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
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
    dmGameObject::HInstance go_animater = Spawn(m_Factory, m_Collection, "/sprite/sprite_anim.goc", dmHashString64("/go_animater"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
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


TEST_F(WindowEventTest, Test)
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
    char resource_path[4][DMPATH_MAX_PATH];
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

    // create resource paths for resource to test for references
    DM_SNPRINTF(resource_path[0], DMPATH_MAX_PATH, "%s/%s", ROOT, "factory/factory_resource.goc");     // instance referenced in factory protoype
    DM_SNPRINTF(resource_path[1], DMPATH_MAX_PATH, "%s/%s", ROOT, "sprite/valid.spritec");             // single instance (subresource of go)
    DM_SNPRINTF(resource_path[2], DMPATH_MAX_PATH, "%s/%s", ROOT, "tile/valid.texturesetc");           // single instance (subresource of sprite)
    DM_SNPRINTF(resource_path[3], DMPATH_MAX_PATH, "%s/%s", ROOT, "sprite/sprite.materialc");          // single instance (subresource of sprite)

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
    char resource_path[5][DMPATH_MAX_PATH];
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

    // create resource paths for resource to test for references
    DM_SNPRINTF(resource_path[0], DMPATH_MAX_PATH, "%s/%s", ROOT, "collection_factory/collectionfactory_test.collectionc"); // prototype resource (loaded in collection factory resource)
    DM_SNPRINTF(resource_path[1], DMPATH_MAX_PATH, "%s/%s", ROOT, "collection_factory/collectionfactory_resource.goc");     // two instances referenced in factory collection protoype
    DM_SNPRINTF(resource_path[2], DMPATH_MAX_PATH, "%s/%s", ROOT, "sprite/valid.spritec");                                  // single instance (subresource of go's)
    DM_SNPRINTF(resource_path[3], DMPATH_MAX_PATH, "%s/%s", ROOT, "tile/valid.texturesetc");                                // single instance (subresource of sprite)
    DM_SNPRINTF(resource_path[4], DMPATH_MAX_PATH, "%s/%s", ROOT, "sprite/sprite.materialc");                               // single instance (subresource of sprite)

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

        // verify four instances created + two references from factory collection prototype
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(6, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[2])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[3])));
        ASSERT_EQ(1, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[4])));

        // --- step 2 ---
        // call collectionfactory.unload which is a no-operation for non-dynamic factories
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        dmGameObject::PostUpdate(m_Register);
        ASSERT_EQ(0, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[0])));
        ASSERT_EQ(6, dmResource::GetRefCount(m_Factory, dmHashString64(resource_path[1])));
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

// TODO document
// TEST_F(ResourceTest, Samplers)
// {
//     dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/resource/samplers.goc", dmHashString64("/go"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
//     ASSERT_NE((void*)0, go);

//     ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
//     ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

//     ASSERT_TRUE(dmGameObject::Final(m_Collection));
// }

/* Render Targets */

TEST_P(RenderScriptTest, Test)
{
    const char* render_proto_path = GetParam();

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    dmGameSystem::RenderScriptPrototype* render_script = LoadRenderFile(m_Factory, render_proto_path);
    ASSERT_NE((void*)0x0, render_script);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));

    // Call update twice (once for enabling rt, once to cleanup)
    for (int i = 0; i < 2; i++)
    {
        dmRender::RenderListBegin(m_RenderContext);
        dmGameObject::Render(m_Collection);
        dmRender::DispatchRenderScriptInstance(render_script->m_Instance);
        dmRender::RenderListEnd(m_RenderContext);
        dmRender::UpdateRenderScriptInstance(render_script->m_Instance);

        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        dmGraphics::Flip(m_GraphicsContext);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
    dmResource::Release(m_Factory, render_script);
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
    dmRender::DrawRenderList(m_RenderContext, 0x0, 0x0);

    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    ASSERT_EQ(expected_draw_count, dmGraphics::GetDrawCount());
    dmGraphics::Flip(m_GraphicsContext);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
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

const char* valid_mesh_resources[] = {"/mesh/valid.meshsetc", "/mesh/valid.skeletonc", "/mesh/valid.animationsetc"};
INSTANTIATE_TEST_CASE_P(Mesh, ResourceTest, ::testing::ValuesIn(valid_mesh_resources));

ResourceFailParams invalid_mesh_resources[] =
{
    {"/mesh/valid.meshsetc", "/mesh/missing.meshsetc"},
    {"/mesh/valid.skeletonc", "/mesh/missing.skeletonc"},
    {"/mesh/valid.animationsetc", "/mesh/missing.animationsetc"},
};
INSTANTIATE_TEST_CASE_P(Mesh, ResourceFailTest, ::testing::ValuesIn(invalid_mesh_resources));

/* Model */

const char* valid_model_resources[] = {"/model/valid.modelc", "/model/empty_texture.modelc"};
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

/* Animationset */

const char* valid_animationset_resources[] = {"/animationset/valid.animationsetc"};
INSTANTIATE_TEST_CASE_P(AnimationSet, ResourceTest, ::testing::ValuesIn(valid_animationset_resources));

ResourceFailParams invalid_animationset_resources[] =
{
    {"/animationset/valid.animationsetc", "/animationset/missing.animationsetc"},
    {"/animationset/valid.animationsetc", "/animationset/invalid_animationset.animationsetc"},
};
INSTANTIATE_TEST_CASE_P(AnimationSet, ResourceFailTest, ::testing::ValuesIn(invalid_animationset_resources));

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

const char* valid_render_resources[] = {"/render/valid.renderc", "/render_script/render_targets.renderc"};
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


/* Collection Factory */

const char* valid_cf_resources[] = {"/collection_factory/valid.collectionfactoryc"};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ResourceTest, ::testing::ValuesIn(valid_cf_resources));

ResourceFailParams invalid_cf_resources[] =
{
    {"/collection_factory/valid.collectionfactoryc", "/collection_factory/missing.collectionfactoryc"},
};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ResourceFailTest, ::testing::ValuesIn(invalid_cf_resources));

const char* valid_cf_gos[] = {"/collection_factory/valid_collectionfactory.goc"};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ComponentTest, ::testing::ValuesIn(valid_cf_gos));

const char* invalid_cf_gos[] = {"/collection_factory/invalid_collectionfactory.goc"};
INSTANTIATE_TEST_CASE_P(CollectionFactory, ComponentFailTest, ::testing::ValuesIn(invalid_cf_gos));


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

const char* valid_spine_scene_resources[] = {"/spine/valid.rigscenec"};
INSTANTIATE_TEST_CASE_P(SpineScene, ResourceTest, ::testing::ValuesIn(valid_spine_scene_resources));

/* Spine Model */

const char* valid_spine_model_resources[] = {"/spine/valid.spinemodelc"};
INSTANTIATE_TEST_CASE_P(SpineModel, ResourceTest, ::testing::ValuesIn(valid_spine_model_resources));

const char* valid_spine_gos[] = {"/spine/valid_spine.goc"};
INSTANTIATE_TEST_CASE_P(SpineModel, ComponentTest, ::testing::ValuesIn(valid_spine_gos));

/* Label */

const char* valid_label_resources[] = {"/label/valid.labelc"};
INSTANTIATE_TEST_CASE_P(Label, ResourceTest, ::testing::ValuesIn(valid_label_resources));

const char* valid_label_gos[] = {"/label/valid_label.goc"};
INSTANTIATE_TEST_CASE_P(Label, ComponentTest, ::testing::ValuesIn(valid_label_gos));

const char* invalid_label_gos[] = {"/label/invalid_label.goc"};
INSTANTIATE_TEST_CASE_P(Label, ComponentFailTest, ::testing::ValuesIn(invalid_label_gos));

/* Get texture0 property on sprite and model */

TexturePropParams texture_prop_params[] =
{
    {"/resource/sprite.goc", dmHashString64("sprite_1_1"), dmHashString64("sprite_1_2"), dmHashString64("sprite_2")},
    {"/resource/model.goc", dmHashString64("model_1_1"), dmHashString64("model_1_2"), dmHashString64("model_2")},
};
INSTANTIATE_TEST_CASE_P(TextureProperty, TexturePropTest, ::testing::ValuesIn(texture_prop_params));

/* Validate default and dynamic gameobject factories */

FactoryTestParams factory_testparams [] =
{
    {"/factory/dynamic_factory_test.goc", true, true},
    {"/factory/dynamic_factory_test.goc", true, false},
    {"/factory/factory_test.goc", false, true},
    {"/factory/factory_test.goc", false, false},
};
INSTANTIATE_TEST_CASE_P(Factory, FactoryTest, ::testing::ValuesIn(factory_testparams));

/* Validate default and dynamic collection factories */

CollectionFactoryTestParams collection_factory_testparams [] =
{
    {"/collection_factory/dynamic_collectionfactory_test.goc", true, true},
    {"/collection_factory/dynamic_collectionfactory_test.goc", true, false},
    {"/collection_factory/collectionfactory_test.goc", false, true},
    {"/collection_factory/collectionfactory_test.goc", false, false},
};
INSTANTIATE_TEST_CASE_P(CollectionFactory, CollectionFactoryTest, ::testing::ValuesIn(collection_factory_testparams));

/* Validate draw count for different GOs */

DrawCountParams draw_count_params[] =
{
    {"/gui/draw_count_test.goc", 2},
    {"/gui/draw_count_test2.goc", 1},
};
INSTANTIATE_TEST_CASE_P(DrawCount, DrawCountTest, ::testing::ValuesIn(draw_count_params));

/* Test different render scripts using render targets and texture resources */

const char* valid_render_protos[] = {"/render_script/render_targets.renderc", "/render_script/textures.renderc"};
INSTANTIATE_TEST_CASE_P(RenderScript, RenderScriptTest, ::testing::ValuesIn(valid_render_protos));

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
