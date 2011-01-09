#include "gamesys.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>

#include "resources/res_collision_object.h"
#include "resources/res_convex_shape.h"
#include "resources/res_emitter.h"
#include "resources/res_texture.h"
#include "resources/res_vertex_program.h"
#include "resources/res_fragment_program.h"
#include "resources/res_image_font.h"
#include "resources/res_font.h"
#include "resources/res_model.h"
#include "resources/res_mesh.h"
#include "resources/res_material.h"
#include "resources/res_gui.h"
#include "resources/res_sound_data.h"
#include "resources/res_camera.h"
#include "resources/res_input_binding.h"
#include "resources/res_gamepad_map.h"
#include "resources/res_spawn_point.h"
#include "resources/res_light.h"
#include "resources/res_render_script.h"
#include "resources/res_render_prototype.h"

#include "components/comp_collision_object.h"
#include "components/comp_emitter.h"
#include "components/comp_model.h"
#include "components/comp_gui.h"
#include "components/comp_sound.h"
#include "components/comp_camera.h"
#include "components/comp_spawn_point.h"
#include "components/comp_light.h"

#include "camera_ddf.h"
#include "physics_ddf.h"

namespace dmGameSystem
{
    void RegisterDDFTypes()
    {
        dmGameObject::RegisterDDFType(dmCameraDDF::AddCameraTarget::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmCameraDDF::SetCamera::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::RayCastRequest::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::RayCastResponse::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::VelocityRequest::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::VelocityResponse::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmGameSystemDDF::SpawnObject::m_DDFDescriptor);
    }

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory, dmRender::HRenderContext render_context)
    {
        dmResource::FactoryResult e;

#define REGISTER_RESOURCE_TYPE(extension, context, create_func, destroy_func, recreate_func)\
    e = dmResource::RegisterType(factory, extension, context, create_func, destroy_func, recreate_func);\
    if( e != dmResource::FACTORY_RESULT_OK )\
    {\
        dmLogFatal("Unable to register resource type: %s", extension);\
        return e;\
    }\

        REGISTER_RESOURCE_TYPE("collisionobjectc", 0, ResCollisionObjectCreate, ResCollisionObjectDestroy, 0);
        REGISTER_RESOURCE_TYPE("convexshapec", 0, ResConvexShapeCreate, ResConvexShapeDestroy, 0);
        REGISTER_RESOURCE_TYPE("emitterc", 0, ResEmitterCreate, ResEmitterDestroy, ResEmitterRecreate);
        REGISTER_RESOURCE_TYPE("texturec", 0, ResTextureCreate, ResTextureDestroy, 0);
        REGISTER_RESOURCE_TYPE("vpc", 0, ResVertexProgramCreate, ResVertexProgramDestroy, ResVertexProgramRecreate);
        REGISTER_RESOURCE_TYPE("fpc", 0, ResFragmentProgramCreate, ResFragmentProgramDestroy, ResFragmentProgramRecreate);
        REGISTER_RESOURCE_TYPE("imagefontc", 0, ResImageFontCreate, ResImageFontDestroy, 0);
        REGISTER_RESOURCE_TYPE("fontc", 0, ResFontCreate, ResFontDestroy, 0);
        REGISTER_RESOURCE_TYPE("modelc", 0, ResCreateModel, ResDestroyModel, ResRecreateModel);
        REGISTER_RESOURCE_TYPE("meshc", 0, ResCreateMesh, ResDestroyMesh, ResRecreateMesh);
        REGISTER_RESOURCE_TYPE("materialc", 0, ResMaterialCreate, ResMaterialDestroy, ResMaterialRecreate);
        REGISTER_RESOURCE_TYPE("guic", 0, ResCreateSceneDesc, ResDestroySceneDesc, 0);
        REGISTER_RESOURCE_TYPE("gui_scriptc", 0, ResCreateGuiScript, ResDestroyGuiScript, 0);
        REGISTER_RESOURCE_TYPE("wavc", 0, ResSoundDataCreate, ResSoundDataDestroy, 0);
        REGISTER_RESOURCE_TYPE("camerac", 0, ResCameraCreate, ResCameraDestroy, ResCameraRecreate);
        REGISTER_RESOURCE_TYPE("input_bindingc", 0, ResInputBindingCreate, ResInputBindingDestroy, ResInputBindingRecreate);
        REGISTER_RESOURCE_TYPE("gamepadsc", 0, ResGamepadMapCreate, ResGamepadMapDestroy, ResGamepadMapRecreate);
        REGISTER_RESOURCE_TYPE("spawnpointc", 0, ResSpawnPointCreate, ResSpawnPointDestroy, 0);
        REGISTER_RESOURCE_TYPE("lightc", 0, ResLightCreate, ResLightDestroy, 0);
        REGISTER_RESOURCE_TYPE("render_scriptc", render_context, ResRenderScriptCreate, ResRenderScriptDestroy, ResRenderScriptRecreate);
        REGISTER_RESOURCE_TYPE("renderc", render_context, ResRenderPrototypeCreate, ResRenderPrototypeDestroy, ResRenderPrototypeRecreate);

#undef REGISTER_RESOURCE_TYPE

        return e;
    }

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                dmGameObject::HRegister regist,
                                                dmRender::RenderContext* render_context,
                                                PhysicsContext* physics_context,
                                                EmitterContext* emitter_context)
    {
        dmGameObject::RegisterDDFType(dmPhysicsDDF::ApplyForceMessage::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::CollisionMessage::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmPhysicsDDF::ContactPointMessage::m_DDFDescriptor);

        uint32_t type;
        dmGameObject::ComponentType component_type;
        dmResource::FactoryResult factory_result;
        dmGameObject::Result go_result;

#define REGISTER_COMPONENT_TYPE(extension, context, new_world_func, delete_world_func, create_func, init_func, destroy_func, update_func, on_message_func, on_input_func)\
    factory_result = dmResource::GetTypeFromExtension(factory, extension, &type);\
    if (factory_result != dmResource::FACTORY_RESULT_OK)\
    {\
        dmLogWarning("Unable to get resource type for '%s' (%d)", extension, factory_result);\
        return dmGameObject::RESULT_UNKNOWN_ERROR;\
    }\
    component_type = dmGameObject::ComponentType();\
    component_type.m_Name = extension;\
    component_type.m_ResourceType = type;\
    component_type.m_Context = context;\
    component_type.m_NewWorldFunction = new_world_func;\
    component_type.m_DeleteWorldFunction = delete_world_func;\
    component_type.m_CreateFunction = create_func;\
    component_type.m_InitFunction = init_func;\
    component_type.m_DestroyFunction = destroy_func;\
    component_type.m_UpdateFunction = update_func;\
    component_type.m_OnMessageFunction = on_message_func;\
    component_type.m_OnInputFunction = on_input_func;\
    component_type.m_InstanceHasUserData = (uint32_t)true;\
    go_result = dmGameObject::RegisterComponentType(regist, component_type);\
    if (go_result != dmGameObject::RESULT_OK)\
        return go_result;

        REGISTER_COMPONENT_TYPE("camerac", render_context,
                &CompCameraNewWorld, &CompCameraDeleteWorld,
                &CompCameraCreate, 0, &CompCameraDestroy,
                &CompCameraUpdate, &CompCameraOnMessage, 0);

        REGISTER_COMPONENT_TYPE("collisionobjectc", physics_context,
                &CompCollisionObjectNewWorld, &CompCollisionObjectDeleteWorld,
                &CompCollisionObjectCreate, &CompCollisionObjectInit, &CompCollisionObjectDestroy,
                &CompCollisionObjectUpdate, &CompCollisionObjectOnMessage, 0);

        REGISTER_COMPONENT_TYPE("wavc", 0x0,
                CompSoundNewWorld, CompSoundDeleteWorld,
                CompSoundCreate, 0, CompSoundDestroy,
                CompSoundUpdate, CompSoundOnMessage, 0);

        REGISTER_COMPONENT_TYPE("modelc", render_context,
                CompModelNewWorld, CompModelDeleteWorld,
                CompModelCreate, 0, CompModelDestroy,
                CompModelUpdate, CompModelOnMessage, 0);

        REGISTER_COMPONENT_TYPE("emitterc", emitter_context,
                &CompEmitterNewWorld, &CompEmitterDeleteWorld,
                &CompEmitterCreate, 0, &CompEmitterDestroy,
                &CompEmitterUpdate, &CompEmitterOnMessage, 0);

        REGISTER_COMPONENT_TYPE("guic", render_context,
                CompGuiNewWorld, CompGuiDeleteWorld,
                CompGuiCreate, CompGuiInit, CompGuiDestroy,
                CompGuiUpdate, CompGuiOnMessage, CompGuiOnInput);

        REGISTER_COMPONENT_TYPE("spawnpointc", render_context,
                CompSpawnPointNewWorld, CompSpawnPointDeleteWorld,
                CompSpawnPointCreate, 0, CompSpawnPointDestroy,
                CompSpawnPointUpdate, CompSpawnPointOnMessage, 0);

        REGISTER_COMPONENT_TYPE("lightc", render_context,
                CompLightNewWorld, CompLightDeleteWorld,
                CompLightCreate, 0, CompLightDestroy,
                CompLightUpdate, CompLightOnMessage, 0);

        #undef REGISTER_COMPONENT_TYPE

        return go_result;
    }

    static void RayCastCallback(const dmPhysics::RayCastResponse& response, const dmPhysics::RayCastRequest& request)
    {
        if (response.m_Hit)
        {
            dmGameObject::HCollection collection = (dmGameObject::HCollection)request.m_UserData;

            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, request.m_UserId);
            dmPhysicsDDF::RayCastResponse ddf;
            ddf.m_Fraction = response.m_Fraction;
            ddf.m_GameObjectId = dmGameObject::GetIdentifier((dmGameObject::HInstance)response.m_CollisionObjectUserData);
            ddf.m_Group = response.m_CollisionObjectGroup;
            ddf.m_Position = response.m_Position;
            ddf.m_Normal = response.m_Normal;
            dmGameObject::PostDDFMessageTo(instance, 0x0, dmPhysicsDDF::RayCastResponse::m_DDFDescriptor, &ddf);
        }
    }

    void RequestRayCast(dmGameObject::HCollection collection, dmGameObject::HInstance instance, const Vectormath::Aos::Point3& from, const Vectormath::Aos::Point3& to, uint32_t mask)
    {
        // Request ray cast
        dmPhysics::RayCastRequest request;
        request.m_From = from;
        request.m_To = to;
        request.m_IgnoredUserData = instance;
        request.m_Mask = mask;
        request.m_UserId = dmGameObject::GetIdentifier(instance);
        request.m_UserData = (void*)collection;
        request.m_Callback = &RayCastCallback;

        uint32_t type;
        dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(dmGameObject::GetFactory(collection), "collisionobjectc", &type);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for '%s' (%d)", "collisionobjectc", fact_result);
        }
        void* world = dmGameObject::FindWorld(collection, type);
        if (world != 0x0)
        {
            dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
            dmPhysics::RequestRayCast(physics_world, request);
        }
    }

}
