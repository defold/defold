#include "gamesys.h"

#include <dlib/log.h>

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

#include "components/comp_collision_object.h"
#include "components/comp_emitter.h"
#include "components/comp_model.h"
#include "components/comp_gui.h"
#include "components/comp_sound.h"
#include "components/comp_camera.h"

#include "camera_ddf.h"
#include "physics_ddf.h"

namespace dmGameSystem
{
    void RegisterDDFTypes()
    {
        dmGameObject::RegisterDDFType(dmCameraDDF::AddCameraTarget::m_DDFDescriptor);
        dmGameObject::RegisterDDFType(dmCameraDDF::SetCamera::m_DDFDescriptor);
    }

    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory)
    {
        dmResource::FactoryResult e;

#define REGISTER_RESOURCE_TYPE(extension, create_func, destroy_func, recreate_func)\
    e = dmResource::RegisterType(factory, extension, 0, create_func, destroy_func, recreate_func);\
    if( e != dmResource::FACTORY_RESULT_OK )\
    {\
        dmLogFatal("Unable to register resource type: %s", extension);\
        return e;\
    }\

        REGISTER_RESOURCE_TYPE("collisionobject", ResCollisionObjectCreate, ResCollisionObjectDestroy, 0);
        REGISTER_RESOURCE_TYPE("convexshape", ResConvexShapeCreate, ResConvexShapeDestroy, 0);
        REGISTER_RESOURCE_TYPE("emitterc", ResEmitterCreate, ResEmitterDestroy, ResEmitterRecreate);
        REGISTER_RESOURCE_TYPE("texture", ResTextureCreate, ResTextureDestroy, 0);
        REGISTER_RESOURCE_TYPE("arbvp", ResVertexProgramCreate, ResVertexProgramDestroy, 0);
        REGISTER_RESOURCE_TYPE("arbfp", ResFragmentProgramCreate, ResFragmentProgramDestroy, 0);
        REGISTER_RESOURCE_TYPE("imagefont", ResImageFontCreate, ResImageFontDestroy, 0);
        REGISTER_RESOURCE_TYPE("font", ResFontCreate, ResFontDestroy, 0);
        REGISTER_RESOURCE_TYPE("modelc", ResCreateModel, ResDestroyModel, ResRecreateModel);
        REGISTER_RESOURCE_TYPE("mesh", ResCreateMesh, ResDestroyMesh, ResRecreateMesh);
        REGISTER_RESOURCE_TYPE("materialc", ResCreateMaterial, ResDestroyMaterial, ResRecreateMaterial);
        REGISTER_RESOURCE_TYPE("guic", ResCreateSceneDesc, ResDestroySceneDesc, 0);
        REGISTER_RESOURCE_TYPE("gui_scriptc", ResCreateGuiScript, ResDestroyGuiScript, 0);
        REGISTER_RESOURCE_TYPE("wavc", ResSoundDataCreate, ResSoundDataDestroy, 0);
        REGISTER_RESOURCE_TYPE("camerac", ResCameraCreate, ResCameraDestroy, ResCameraRecreate);

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

#define REGISTER_COMPONENT_TYPE(extension, context, new_world_func, delete_world_func, create_func, init_func, destroy_func, update_func, on_event_func, on_input_func)\
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
    component_type.m_OnEventFunction = on_event_func;\
    component_type.m_OnInputFunction = on_input_func;\
    component_type.m_InstanceHasUserData = (uint32_t)true;\
    go_result = dmGameObject::RegisterComponentType(regist, component_type);\
    if (go_result != dmGameObject::RESULT_OK)\
        return go_result;

        REGISTER_COMPONENT_TYPE("camerac", render_context,
                &CompCameraNewWorld, &CompCameraDeleteWorld,
                &CompCameraCreate, 0, &CompCameraDestroy,
                &CompCameraUpdate, &CompCameraOnEvent, 0);

        REGISTER_COMPONENT_TYPE("collisionobject", physics_context,
                &CompCollisionObjectNewWorld, &CompCollisionObjectDeleteWorld,
                &CompCollisionObjectCreate, &CompCollisionObjectInit, &CompCollisionObjectDestroy,
                &CompCollisionObjectUpdate, &CompCollisionObjectOnEvent, 0);

        REGISTER_COMPONENT_TYPE("wavc", 0x0,
                CompSoundNewWorld, CompSoundDeleteWorld,
                CompSoundCreate, 0, CompSoundDestroy,
                CompSoundUpdate, CompSoundOnEvent, 0);

        REGISTER_COMPONENT_TYPE("modelc", render_context,
                CompModelNewWorld, CompModelDeleteWorld,
                CompModelCreate, 0, CompModelDestroy,
                CompModelUpdate, CompModelOnEvent, 0);

        REGISTER_COMPONENT_TYPE("emitterc", emitter_context,
                &CompEmitterNewWorld, &CompEmitterDeleteWorld,
                &CompEmitterCreate, 0, &CompEmitterDestroy,
                &CompEmitterUpdate, &CompEmitterOnEvent, 0);

        REGISTER_COMPONENT_TYPE("guic", 0x0,
                CompGuiNewWorld, CompGuiDeleteWorld,
                CompGuiCreate, CompGuiInit, CompGuiDestroy,
                CompGuiUpdate, CompGuiOnEvent, CompGuiOnInput);

#undef REGISTER_COMPONENT_TYPE

        return go_result;
    }

    void RequestRayCast(dmGameObject::HCollection collection, const dmPhysics::RayCastRequest& request)
    {
        uint32_t type;
        dmResource::FactoryResult fact_result = dmResource::GetTypeFromExtension(dmGameObject::GetFactory(collection), "collisionobject", &type);
        if (fact_result != dmResource::FACTORY_RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for '%s' (%d)", "collisionobject", fact_result);
        }
        void* world = dmGameObject::FindWorld(collection, type);
        if (world != 0x0)
        {
            dmPhysics::HWorld physics_world = (dmPhysics::HWorld) world;
            dmPhysics::RequestRayCast(physics_world, request);
        }
    }

}
