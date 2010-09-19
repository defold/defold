#include "gamesys.h"

#include <dlib/log.h>

#include "res_collision_object.h"
#include "res_convex_shape.h"
#include "res_emitter.h"
#include "res_texture.h"
#include "res_vertex_program.h"
#include "res_fragment_program.h"
#include "res_image_font.h"
#include "res_font.h"
#include "res_model.h"
#include "res_mesh.h"
#include "res_material.h"

#include "comp_collision_object.h"
#include "comp_emitter.h"
#include "comp_model.h"

#include "../proto/physics_ddf.h"

namespace dmGameSystem
{
    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory)
    {
        dmResource::FactoryResult e;

#define REGISTER_RESOURCE_TYPE(extension, create_func, destroy_func, recreate_func)\
    e = dmResource::RegisterType(factory, extension, 0, create_func, destroy_func, recreate_func);\
    if( e != dmResource::FACTORY_RESULT_OK ) return e;

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

#define REGISTER_COMPONENT_TYPE(extension, context, new_world_func, delete_world_func, create_func, init_func, destroy_func, update_func, on_event_func)\
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
    component_type.m_InstanceHasUserData = (uint32_t)true;\
    go_result = dmGameObject::RegisterComponentType(regist, component_type);\
    if (go_result != dmGameObject::RESULT_OK)\
        return go_result;

        REGISTER_COMPONENT_TYPE("collisionobject", physics_context,
                &CompCollisionObjectNewWorld, &CompCollisionObjectDeleteWorld,
                &CompCollisionObjectCreate, &CompCollisionObjectInit, &CompCollisionObjectDestroy,
                &CompCollisionObjectUpdate, &CompCollisionObjectOnEvent);

        REGISTER_COMPONENT_TYPE("emitterc", emitter_context,
                &CompEmitterNewWorld, &CompEmitterDeleteWorld,
                &CompEmitterCreate, 0, &CompEmitterDestroy,
                &CompEmitterUpdate, &CompEmitterOnEvent);

        REGISTER_COMPONENT_TYPE("modelc", render_context,
                CompModelNewWorld, CompModelDeleteWorld,
                CompModelCreate, 0, CompModelDestroy,
                CompModelUpdate, CompModelOnEvent);

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
