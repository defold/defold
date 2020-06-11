// Copyright 2020 The Defold Foundation
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

namespace dmGameObject
{

static ComponentTypeDescriptor g_ComponentTypeSentinel = {0};

Result RegisterComponentTypeDescriptor(ComponentTypeDescriptor* desc, const char* name, ComponentTypeCreateFunction create_fn)
{
    DM_STATIC_ASSERT(dmGameObject::s_ComponentTypeDescBufferSize >= sizeof(ComponentTypeDescriptor), Invalid_Struct_Size);

    desc->m_Name = name;
    desc->m_CreateFn = create_fn;
    desc->m_Next = g_ComponentTypeSentinel.m_Next;
    g_ComponentTypeSentinel.m_Next = desc;

    return dmGameObject::RESULT_OK;
}

// TODO: Clear the descriptor table?

Result CreateRegisteredComponentTypes(const ComponentTypeCreateCtx* ctx)
{
    ComponentTypeDescriptor* type_desc = g_ComponentTypeSentinel.m_Next;
    while (type_desc != 0)
    {
        ComponentType component_type;

        dmResource::ResourceType resource_type;
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

        component_type.m_ResourceType = resource_type;
        component_type.m_Name = type_desc->m_Name;
        component_type.m_InstanceHasUserData = true;

        // Now, let the callback add the function pointers
        Result go_result = type_desc->m_CreateFn(ctx, &component_type);
        if (go_result != dmGameObject::RESULT_OK)
            return go_result;

        go_result = RegisterComponentType(ctx->m_Register, component_type);
        if (go_result != dmGameObject::RESULT_OK)
            return go_result;

        type_desc = type_desc->m_Next;
    }
    return dmGameObject::RESULT_OK;
}


void ComponentTypeSetNewWorldFn(ComponentType* type, ComponentNewWorld fn)          { type->m_NewWorldFunction = fn; }
void ComponentTypeSetDeleteWorldFn(ComponentType* type, ComponentDeleteWorld fn)    { type->m_DeleteWorldFunction = fn; }
void ComponentTypeSetCreateFn(ComponentType* type, ComponentCreate fn)              { type->m_CreateFunction = fn; }
void ComponentTypeSetDestroyFn(ComponentType* type, ComponentDestroy fn)            { type->m_DestroyFunction = fn; }
void ComponentTypeSetInitFn(ComponentType* type, ComponentInit fn)                  { type->m_InitFunction = fn; }
void ComponentTypeSetFinalFn(ComponentType* type, ComponentFinal fn)                { type->m_FinalFunction = fn; }
void ComponentTypeSetAddToUpdateFn(ComponentType* type, ComponentAddToUpdate fn)    { type->m_AddToUpdateFunction = fn; }
void ComponentTypeSetGetFn(ComponentType* type, ComponentGet fn)                    { type->m_GetFunction = fn; }
void ComponentTypeSetRenderFn(ComponentType* type, ComponentsRender fn)             { type->m_RenderFunction = fn; }
void ComponentTypeSetUpdateFn(ComponentType* type, ComponentsUpdate fn)             { type->m_UpdateFunction = fn; }
void ComponentTypeSetPostUpdateFn(ComponentType* type, ComponentsPostUpdate fn)     { type->m_PostUpdateFunction = fn; }
void ComponentTypeSetOnMessageFn(ComponentType* type, ComponentOnMessage fn)        { type->m_OnMessageFunction = fn; }
void ComponentTypeSetOnInputFn(ComponentType* type, ComponentOnInput fn)            { type->m_OnInputFunction = fn; }
void ComponentTypeSetOnReloadFn(ComponentType* type, ComponentOnReload fn)          { type->m_OnReloadFunction = fn; }
void ComponentTypeSetSetPropertiesFn(ComponentType* type, ComponentSetProperties fn){ type->m_SetPropertiesFunction = fn; }
void ComponentTypeSetGetPropertyFn(ComponentType* type, ComponentGetProperty fn)    { type->m_GetPropertyFunction = fn; }
void ComponentTypeSetSetPropertyFn(ComponentType* type, ComponentSetProperty fn)    { type->m_SetPropertyFunction = fn; }
void ComponentTypeSetContext(ComponentType* type, void* context)                    { type->m_Context = context; }
void ComponentTypeSetReadsTransforms(ComponentType* type, bool reads_transforms)    { type->m_ReadsTransforms = reads_transforms?1:0; }
void ComponentTypeSetPrio(ComponentType* type, uint16_t prio)                       { type->m_UpdateOrderPrio = prio; }
void ComponentTypeSetHasUserData(ComponentType* type, bool has_user_data)           { type->m_InstanceHasUserData = has_user_data; }


} // namespace
