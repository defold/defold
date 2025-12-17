// Copyright 2020-2025 The Defold Foundation
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

#include "component.h"

#include <dlib/static_assert.h>
#include <dlib/hash.h>

namespace dmGameObject
{

static ComponentTypeDescriptor g_ComponentTypeSentinel = {0};

Result RegisterComponentTypeDescriptor(ComponentTypeDescriptor* desc, const char* name, ComponentTypeCreateFunction create_fn, ComponentTypeDestroyFunction destroy_fn)
{
    DM_STATIC_ASSERT(dmGameObject::s_ComponentTypeDescBufferSize >= sizeof(ComponentTypeDescriptor), Invalid_Struct_Size);

    dmLogDebug("Registered component type descriptor %s", name);
    desc->m_Name = name;
    desc->m_CreateFn = create_fn;
    desc->m_DestroyFn = destroy_fn;
    desc->m_Next = g_ComponentTypeSentinel.m_Next;
    g_ComponentTypeSentinel.m_Next = desc;

    return dmGameObject::RESULT_OK;
}

Result CreateRegisteredComponentTypes(const ComponentTypeCreateCtx* ctx)
{
    ComponentTypeDescriptor* type_desc = g_ComponentTypeSentinel.m_Next;
    while (type_desc != 0)
    {
        ComponentType component_type;

        HResourceType resource_type;
        dmResource::Result factory_result = dmResource::GetTypeFromExtension(ctx->m_Factory, type_desc->m_Name, &resource_type);
        if (factory_result != dmResource::RESULT_OK)
        {
            dmLogWarning("Unable to get resource type for '%s': %s", type_desc->m_Name, dmResource::ResultToString(factory_result));
            return dmGameObject::RESULT_UNKNOWN_ERROR;
        }

        uint32_t component_type_index = 0;
        if (dmGameObject::FindComponentType(ctx->m_Register, resource_type, &component_type_index) != 0)
        {
            dmLogWarning("Component type '%s' already added!", type_desc->m_Name);
        }

        component_type.m_TypeIndex = dmGameObject::GetNumComponentTypes(ctx->m_Register);
        component_type.m_ResourceType = resource_type;
        component_type.m_Name = type_desc->m_Name;
        component_type.m_NameHash = dmHashString64(type_desc->m_Name);
        component_type.m_InstanceHasUserData = true;

        // Now, let the callback add the function pointers
        Result go_result = type_desc->m_CreateFn(ctx, &component_type);
        if (go_result != dmGameObject::RESULT_OK)
            return go_result;

        go_result = RegisterComponentType(ctx->m_Register, component_type);
        if (go_result != dmGameObject::RESULT_OK)
            return go_result;

        type_desc->m_TypeIndex = component_type.m_TypeIndex;

        type_desc = type_desc->m_Next;
    }
    return dmGameObject::RESULT_OK;
}

Result DestroyRegisteredComponentTypes(const ComponentTypeCreateCtx* ctx)
{
    // There is no UnregisterComponent type, but we want the opportunity to destroy any custom contexts that were created
    // during the registering process

    ComponentTypeDescriptor* type_desc = g_ComponentTypeSentinel.m_Next;
    while (type_desc != 0)
    {
        HComponentType component_type = dmGameObject::GetComponentType(ctx->m_Register, (uint32_t)type_desc->m_TypeIndex);

        if (type_desc->m_DestroyFn)
        {
            Result go_result = type_desc->m_DestroyFn(ctx, component_type);
            if (go_result != dmGameObject::RESULT_OK)
            {
                dmLogError("Failed to destroy component type %s", type_desc->m_Name);
            }
        }

        type_desc = type_desc->m_Next;
    }
    return dmGameObject::RESULT_OK;
}

void ComponentTypeSetNewWorldFn(HComponentType type, ComponentNewWorld fn)                  { type->m_NewWorldFunction = fn; }
void ComponentTypeSetDeleteWorldFn(HComponentType type, ComponentDeleteWorld fn)            { type->m_DeleteWorldFunction = fn; }
void ComponentTypeSetCreateFn(HComponentType type, ComponentCreate fn)                      { type->m_CreateFunction = fn; }
void ComponentTypeSetDestroyFn(HComponentType type, ComponentDestroy fn)                    { type->m_DestroyFunction = fn; }
void ComponentTypeSetInitFn(HComponentType type, ComponentInit fn)                          { type->m_InitFunction = fn; }
void ComponentTypeSetFinalFn(HComponentType type, ComponentFinal fn)                        { type->m_FinalFunction = fn; }
void ComponentTypeSetAddToUpdateFn(HComponentType type, ComponentAddToUpdate fn)            { type->m_AddToUpdateFunction = fn; }
void ComponentTypeSetGetFn(HComponentType type, ComponentGet fn)                            { type->m_GetFunction = fn; }
void ComponentTypeSetRenderFn(HComponentType type, ComponentsRender fn)                     { type->m_RenderFunction = fn; }
void ComponentTypeSetPreFixedUpdateFn(HComponentType type, ComponentsUpdate fn)             { type->m_PreFixedUpdateFunction = fn; }
void ComponentTypeSetPreUpdateFn(HComponentType type, ComponentsUpdate fn)                  { type->m_PreUpdateFunction = fn; }
void ComponentTypeSetUpdateFn(HComponentType type, ComponentsUpdate fn)                     { type->m_UpdateFunction = fn; }
void ComponentTypeSetLateUpdateFn(HComponentType type, ComponentsUpdate fn)                 { type->m_LateUpdateFunction = fn; }
void ComponentTypeSetFixedUpdateFn(HComponentType type, ComponentsUpdate fn)                { type->m_FixedUpdateFunction = fn; }
void ComponentTypeSetPostUpdateFn(HComponentType type, ComponentsPostUpdate fn)             { type->m_PostUpdateFunction = fn; }
void ComponentTypeSetOnMessageFn(HComponentType type, ComponentOnMessage fn)                { type->m_OnMessageFunction = fn; }
void ComponentTypeSetOnInputFn(HComponentType type, ComponentOnInput fn)                    { type->m_OnInputFunction = fn; }
void ComponentTypeSetOnReloadFn(HComponentType type, ComponentOnReload fn)                  { type->m_OnReloadFunction = fn; }
void ComponentTypeSetSetPropertiesFn(HComponentType type, ComponentSetProperties fn)        { type->m_SetPropertiesFunction = fn; }
void ComponentTypeSetGetPropertyFn(HComponentType type, ComponentGetProperty fn)            { type->m_GetPropertyFunction = fn; }
void ComponentTypeSetSetPropertyFn(HComponentType type, ComponentSetProperty fn)            { type->m_SetPropertyFunction = fn; }
void ComponentTypeSetContext(HComponentType type, void* context)                            { type->m_Context = context; }
void ComponentTypeSetReadsTransforms(HComponentType type, bool reads_transforms)            { type->m_ReadsTransforms = reads_transforms?1:0; }
void ComponentTypeSetPrio(HComponentType type, uint16_t prio)                               { type->m_UpdateOrderPrio = prio; }
void ComponentTypeSetHasUserData(HComponentType type, bool has_user_data)                   { type->m_InstanceHasUserData = has_user_data; }
void ComponentTypeSetChildIteratorFn(HComponentType type, FIteratorChildren fn)             { type->m_IterChildren = fn; }
void ComponentTypeSetPropertyIteratorFn(HComponentType type, FIteratorProperties fn)        { type->m_IterProperties = fn; }
// Getters
uint32_t    ComponentTypeGetTypeIndex(const HComponentType type)                            { return type->m_TypeIndex; }
void*       ComponentTypeGetContext(const HComponentType type)                              { return type->m_Context; }


} // namespace
