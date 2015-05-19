#include "gamesys.h"

#include "gamesys_private.h"

#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>

#include "resources/res_collection_proxy.h"
#include "resources/res_collision_object.h"
#include "resources/res_convex_shape.h"
#include "resources/res_emitter.h"
#include "resources/res_particlefx.h"
#include "resources/res_texture.h"
#include "resources/res_vertex_program.h"
#include "resources/res_fragment_program.h"
#include "resources/res_font_map.h"
#include "resources/res_model.h"
#include "resources/res_mesh.h"
#include "resources/res_material.h"
#include "resources/res_gui.h"
#include "resources/res_sound_data.h"
#include "resources/res_sound.h"
#include "resources/res_camera.h"
#include "resources/res_input_binding.h"
#include "resources/res_gamepad_map.h"
#include "resources/res_factory.h"
#include "resources/res_collection_factory.h"
#include "resources/res_light.h"
#include "resources/res_render_script.h"
#include "resources/res_render_prototype.h"
#include "resources/res_sprite.h"
#include "resources/res_textureset.h"
#include "resources/res_tilegrid.h"
#include "resources/res_spine_scene.h"
#include "resources/res_spine_model.h"
#include "resources/res_display_profiles.h"

#include "components/comp_collection_proxy.h"
#include "components/comp_collision_object.h"
#include "components/comp_emitter.h"
#include "components/comp_particlefx.h"
#include "components/comp_model.h"
#include "components/comp_gui.h"
#include "components/comp_sound.h"
#include "components/comp_camera.h"
#include "components/comp_factory.h"
#include "components/comp_collection_factory.h"
#include "components/comp_light.h"
#include "components/comp_sprite.h"
#include "components/comp_tilegrid.h"
#include "components/comp_spine_model.h"

#include "camera_ddf.h"
#include "physics_ddf.h"
#include "tile_ddf.h"
#include "sprite_ddf.h"

namespace dmGameSystem
{
    GuiContext::GuiContext()
    : m_Worlds()
    , m_RenderContext(0)
    , m_GuiContext(0)
    , m_ScriptContext(0)
    {
        m_Worlds.SetCapacity(128);
    }

    dmResource::Result RegisterResourceTypes(dmResource::HFactory factory, dmRender::HRenderContext render_context, GuiContext* gui_context, dmInput::HContext input_context, PhysicsContext* physics_context)
    {
        dmResource::Result e;

#define REGISTER_RESOURCE_TYPE(extension, context, create_func, destroy_func, recreate_func)\
    e = dmResource::RegisterType(factory, extension, context, create_func, destroy_func, recreate_func);\
    if( e != dmResource::RESULT_OK )\
    {\
        dmLogFatal("Unable to register resource type: %s", extension);\
        return e;\
    }\

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        REGISTER_RESOURCE_TYPE("collectionproxyc", 0, ResCollectionProxyCreate, ResCollectionProxyDestroy, ResCollectionProxyRecreate);
        REGISTER_RESOURCE_TYPE("collisionobjectc", physics_context, ResCollisionObjectCreate, ResCollisionObjectDestroy, ResCollisionObjectRecreate);
        REGISTER_RESOURCE_TYPE("convexshapec", physics_context, ResConvexShapeCreate, ResConvexShapeDestroy, ResConvexShapeRecreate);
        REGISTER_RESOURCE_TYPE("emitterc", 0, ResEmitterCreate, ResEmitterDestroy, ResEmitterRecreate);
        REGISTER_RESOURCE_TYPE("particlefxc", 0, ResParticleFXCreate, ResParticleFXDestroy, ResParticleFXRecreate);
        REGISTER_RESOURCE_TYPE("texturec", graphics_context, ResTextureCreate, ResTextureDestroy, ResTextureRecreate);
        REGISTER_RESOURCE_TYPE("vpc", graphics_context, ResVertexProgramCreate, ResVertexProgramDestroy, ResVertexProgramRecreate);
        REGISTER_RESOURCE_TYPE("fpc", graphics_context, ResFragmentProgramCreate, ResFragmentProgramDestroy, ResFragmentProgramRecreate);
        REGISTER_RESOURCE_TYPE("fontc", render_context, ResFontMapCreate, ResFontMapDestroy, ResFontMapRecreate);
        REGISTER_RESOURCE_TYPE("modelc", 0, ResCreateModel, ResDestroyModel, ResRecreateModel);
        REGISTER_RESOURCE_TYPE("meshc", graphics_context, ResCreateMesh, ResDestroyMesh, ResRecreateMesh);
        REGISTER_RESOURCE_TYPE("materialc", render_context, ResMaterialCreate, ResMaterialDestroy, ResMaterialRecreate);
        REGISTER_RESOURCE_TYPE("guic", gui_context, ResCreateSceneDesc, ResDestroySceneDesc, ResRecreateSceneDesc);
        REGISTER_RESOURCE_TYPE("gui_scriptc", gui_context, ResCreateGuiScript, ResDestroyGuiScript, ResRecreateGuiScript);
        REGISTER_RESOURCE_TYPE("wavc", 0, ResSoundDataCreate, ResSoundDataDestroy, ResSoundDataRecreate);
        REGISTER_RESOURCE_TYPE("oggc", 0, ResSoundDataCreate, ResSoundDataDestroy, ResSoundDataRecreate);
        REGISTER_RESOURCE_TYPE("soundc", 0, ResSoundCreate, ResSoundDestroy, ResSoundRecreate);
        REGISTER_RESOURCE_TYPE("camerac", 0, ResCameraCreate, ResCameraDestroy, ResCameraRecreate);
        REGISTER_RESOURCE_TYPE("input_bindingc", input_context, ResInputBindingCreate, ResInputBindingDestroy, ResInputBindingRecreate);
        REGISTER_RESOURCE_TYPE("gamepadsc", 0, ResGamepadMapCreate, ResGamepadMapDestroy, ResGamepadMapRecreate);
        REGISTER_RESOURCE_TYPE("factoryc", 0, ResFactoryCreate, ResFactoryDestroy, ResFactoryRecreate);
        REGISTER_RESOURCE_TYPE("collectionfactoryc", 0, ResCollectionFactoryCreate, ResCollectionFactoryDestroy, ResCollectionFactoryRecreate);
        REGISTER_RESOURCE_TYPE("lightc", 0, ResLightCreate, ResLightDestroy, ResLightRecreate);
        REGISTER_RESOURCE_TYPE("render_scriptc", render_context, ResRenderScriptCreate, ResRenderScriptDestroy, ResRenderScriptRecreate);
        REGISTER_RESOURCE_TYPE("renderc", render_context, ResRenderPrototypeCreate, ResRenderPrototypeDestroy, ResRenderPrototypeRecreate);
        REGISTER_RESOURCE_TYPE("spritec", 0, ResSpriteCreate, ResSpriteDestroy, ResSpriteRecreate);
        REGISTER_RESOURCE_TYPE("texturesetc", physics_context, ResTextureSetCreate, ResTextureSetDestroy, ResTextureSetRecreate);
        REGISTER_RESOURCE_TYPE(TILE_MAP_EXT, physics_context, ResTileGridCreate, ResTileGridDestroy, ResTileGridRecreate);
        REGISTER_RESOURCE_TYPE("spinescenec", 0, ResSpineSceneCreate, ResSpineSceneDestroy, ResSpineSceneRecreate);
        REGISTER_RESOURCE_TYPE(SPINE_MODEL_EXT, 0, ResSpineModelCreate, ResSpineModelDestroy, ResSpineModelRecreate);
        REGISTER_RESOURCE_TYPE("display_profilesc", render_context, ResDisplayProfilesCreate, ResDisplayProfilesDestroy, ResDisplayProfilesRecreate);

#undef REGISTER_RESOURCE_TYPE

        return e;
    }

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                dmGameObject::HRegister regist,
                                                dmRender::RenderContext* render_context,
                                                PhysicsContext* physics_context,
                                                ParticleFXContext* particlefx_context,
                                                GuiContext* gui_context,
                                                SpriteContext* sprite_context,
                                                CollectionProxyContext* collection_proxy_context,
                                                FactoryContext* factory_context,
                                                CollectionFactoryContext *collectionfactory_context,
                                                SpineModelContext* spine_model_context)
    {
        dmResource::ResourceType type;
        dmGameObject::ComponentType component_type;
        dmResource::Result factory_result;
        dmGameObject::Result go_result;

#define REGISTER_COMPONENT_TYPE(extension, prio, context, new_world_func, delete_world_func, create_func, destroy_func, init_func, final_func, add_to_update_func, update_func, post_update_func, on_message_func, on_input_func, on_reload_func, get_property_func, set_property_func)\
    factory_result = dmResource::GetTypeFromExtension(factory, extension, &type);\
    if (factory_result != dmResource::RESULT_OK)\
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
    component_type.m_DestroyFunction = destroy_func;\
    component_type.m_InitFunction = init_func;\
    component_type.m_FinalFunction = final_func;\
    component_type.m_AddToUpdateFunction = add_to_update_func;\
    component_type.m_UpdateFunction = update_func;\
    component_type.m_PostUpdateFunction = post_update_func;\
    component_type.m_OnMessageFunction = on_message_func;\
    component_type.m_OnInputFunction = on_input_func;\
    component_type.m_OnReloadFunction = on_reload_func;\
    component_type.m_GetPropertyFunction = get_property_func;\
    component_type.m_SetPropertyFunction = set_property_func;\
    component_type.m_InstanceHasUserData = (uint32_t)true;\
    component_type.m_UpdateOrderPrio = prio;\
    go_result = dmGameObject::RegisterComponentType(regist, component_type);\
    if (go_result != dmGameObject::RESULT_OK)\
        return go_result;

        /*
         * About update priority. Component types below have priority evenly spaced with increments by 100
         *
         */

        REGISTER_COMPONENT_TYPE("collectionproxyc", 100, collection_proxy_context,
                &CompCollectionProxyNewWorld, &CompCollectionProxyDeleteWorld,
                &CompCollectionProxyCreate, &CompCollectionProxyDestroy, 0, 0,
                &CompCollectionProxyAddToUpdate, &CompCollectionProxyUpdate, &CompCollectionProxyPostUpdate,
                &CompCollectionProxyOnMessage, &CompCollectionProxyOnInput, 0, 0, 0);

        // Priority 200 is reserved for script

        REGISTER_COMPONENT_TYPE("guic", 300, gui_context,
                CompGuiNewWorld, CompGuiDeleteWorld,
                CompGuiCreate, CompGuiDestroy, CompGuiInit, CompGuiFinal, CompGuiAddToUpdate,
                CompGuiUpdate, 0, CompGuiOnMessage, CompGuiOnInput, CompGuiOnReload, 0, 0);

        REGISTER_COMPONENT_TYPE("collisionobjectc", 400, physics_context,
                &CompCollisionObjectNewWorld, &CompCollisionObjectDeleteWorld,
                &CompCollisionObjectCreate, &CompCollisionObjectDestroy, 0, &CompCollisionObjectFinal, &CompCollisionObjectAddToUpdate,
                &CompCollisionObjectUpdate, &CompCollisionObjectPostUpdate, &CompCollisionObjectOnMessage, 0, &CompCollisionObjectOnReload, CompCollisionObjectGetProperty, CompCollisionObjectSetProperty);

        REGISTER_COMPONENT_TYPE("camerac", 500, render_context,
                &CompCameraNewWorld, &CompCameraDeleteWorld,
                &CompCameraCreate, &CompCameraDestroy, 0, 0, &CompCameraAddToUpdate,
                &CompCameraUpdate, 0, &CompCameraOnMessage, 0, &CompCameraOnReload, 0, 0);

        REGISTER_COMPONENT_TYPE("soundc", 600, 0x0,
                CompSoundNewWorld, CompSoundDeleteWorld,
                CompSoundCreate, CompSoundDestroy, 0, 0, CompSoundAddToUpdate,
                CompSoundUpdate, 0, CompSoundOnMessage, 0, 0, 0, 0);

        REGISTER_COMPONENT_TYPE("modelc", 700, render_context,
                CompModelNewWorld, CompModelDeleteWorld,
                CompModelCreate, CompModelDestroy, 0, 0, CompModelAddToUpdate,
                CompModelUpdate, 0, CompModelOnMessage, 0, 0, CompModelGetProperty, CompModelSetProperty);

        REGISTER_COMPONENT_TYPE("emitterc", 750, 0x0,
                &CompEmitterNewWorld, &CompEmitterDeleteWorld,
                &CompEmitterCreate, &CompEmitterDestroy, 0, 0, 0,
                0, 0, CompEmitterOnMessage, 0, 0, 0, 0);

        REGISTER_COMPONENT_TYPE("particlefxc", 800, particlefx_context,
                &CompParticleFXNewWorld, &CompParticleFXDeleteWorld,
                &CompParticleFXCreate, &CompParticleFXDestroy, 0, 0, &CompParticleFXAddToUpdate,
                &CompParticleFXUpdate, 0, &CompParticleFXOnMessage, 0, &CompParticleFXOnReload, 0, 0);

        REGISTER_COMPONENT_TYPE("factoryc", 900, factory_context,
                CompFactoryNewWorld, CompFactoryDeleteWorld,
                CompFactoryCreate, CompFactoryDestroy, 0, 0, 0,
                0, 0, CompFactoryOnMessage, 0, 0, 0, 0);

        REGISTER_COMPONENT_TYPE("collectionfactoryc", 950, collectionfactory_context,
                CompCollectionFactoryNewWorld, CompCollectionFactoryDeleteWorld,
                CompCollectionFactoryCreate, CompCollectionFactoryDestroy, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0);

        REGISTER_COMPONENT_TYPE("lightc", 1000, render_context,
                CompLightNewWorld, CompLightDeleteWorld,
                CompLightCreate, CompLightDestroy, 0, 0, CompLightAddToUpdate,
                CompLightUpdate, 0, CompLightOnMessage, 0, 0, 0, 0);

        REGISTER_COMPONENT_TYPE("spritec", 1100, sprite_context,
                CompSpriteNewWorld, CompSpriteDeleteWorld,
                CompSpriteCreate, CompSpriteDestroy, 0, 0, CompSpriteAddToUpdate,
                CompSpriteUpdate, 0, CompSpriteOnMessage, 0, CompSpriteOnReload, CompSpriteGetProperty, CompSpriteSetProperty);

        REGISTER_COMPONENT_TYPE(TILE_MAP_EXT, 1200, render_context,
                CompTileGridNewWorld, CompTileGridDeleteWorld,
                CompTileGridCreate, CompTileGridDestroy, 0, 0, CompTileGridAddToUpdate,
                CompTileGridUpdate, 0, CompTileGridOnMessage, 0, CompTileGridOnReload, CompTileGridGetProperty, CompTileGridSetProperty);

        REGISTER_COMPONENT_TYPE(SPINE_MODEL_EXT, 1300, spine_model_context,
                CompSpineModelNewWorld, CompSpineModelDeleteWorld,
                CompSpineModelCreate, CompSpineModelDestroy, 0, 0, CompSpineModelAddToUpdate,
                CompSpineModelUpdate, 0, CompSpineModelOnMessage, 0, CompSpineModelOnReload, CompSpineModelGetProperty, CompSpineModelSetProperty);

        #undef REGISTER_COMPONENT_TYPE

        return go_result;
    }
}
