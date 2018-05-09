#include "res_collection.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include "gameobject.h"
#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_ddf.h"

#include "../proto/gameobject/gameobject_ddf.h"
#include "gameobject_props.h"

namespace dmGameObject
{
    dmResource::Result ResCollectionPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameObjectDDF::CollectionDesc* collection_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(params.m_Buffer, params.m_BufferSize, &collection_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        const dmGameObjectDDF::InstanceDesc* instances = collection_desc->m_Instances.m_Data;
        uint32_t n_instances = collection_desc->m_Instances.m_Count;
        for (uint32_t i = 0; i < n_instances; ++i)
        {
            if (instances[i].m_Prototype != 0x0)
            {
                dmResource::PreloadHint(params.m_HintInfo, instances[i].m_Prototype);
            }
        }
        const char** resources = collection_desc->m_PropertyResources.m_Data;
        uint32_t n_resources = collection_desc->m_PropertyResources.m_Count;
        for (uint32_t i = 0; i < n_resources; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, resources[i]);
        }

        *params.m_PreloadData = collection_desc;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionCreate(const dmResource::ResourceCreateParams& params)
    {
        Register* regist = (Register*) params.m_Context;
        dmResource::Result res = dmResource::RESULT_OK;

        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) params.m_PreloadData;

        // NOTE: Be careful about control flow. See below with dmMutex::Unlock, return, etc
        dmMutex::Lock(regist->m_Mutex);

        uint32_t created_instances = 0;
        uint32_t collection_capacity = dmGameObject::GetCollectionDefaultCapacity(regist);
        HCollection collection = NewCollection(collection_desc->m_Name, params.m_Factory, regist, collection_capacity);
        if (collection == 0)
        {
            res = dmResource::RESULT_OUT_OF_RESOURCES;
            goto bail;
        }

        res = LoadPropertyResources(params.m_Factory, collection_desc->m_PropertyResources.m_Data, collection_desc->m_PropertyResources.m_Count, collection->m_PropertyResources);
        if(res != dmResource::RESULT_OK)
        {
            goto bail;
        }

        collection->m_ScaleAlongZ = collection_desc->m_ScaleAlongZ;
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            Prototype* proto = 0x0;
            dmResource::HFactory factory = collection->m_Factory;
            dmGameObject::HInstance instance = 0x0;
            if (instance_desc.m_Prototype != 0x0)
            {
                dmResource::Result error = dmResource::Get(params.m_Factory, instance_desc.m_Prototype, (void**)&proto);
                if (error == dmResource::RESULT_OK) {
                    instance = dmGameObject::NewInstance(collection, proto, instance_desc.m_Prototype);
                    if (instance == 0) {
                        dmResource::Release(factory, proto);
                    }
                }
            }
            if (instance != 0x0)
            {
                instance->m_ScaleAlongZ = collection_desc->m_ScaleAlongZ;

                // support legacy pipeline which outputs 0 for Scale3 and scale in Scale
                Vector3 scale = instance_desc.m_Scale3;
                if (scale.getX() == 0 && scale.getY() == 0 && scale.getZ() == 0)
                        scale = Vector3(instance_desc.m_Scale, instance_desc.m_Scale, instance_desc.m_Scale);

                instance->m_Transform = dmTransform::Transform(Vector3(instance_desc.m_Position), instance_desc.m_Rotation, scale);

                dmHashInit64(&instance->m_CollectionPathHashState, true);
                const char* path_end = strrchr(instance_desc.m_Id, *ID_SEPARATOR);
                if (path_end == 0x0)
                {
                    dmLogError("The id of %s has an incorrect format, missing path specifier.", instance_desc.m_Id);
                }
                else
                {
                    dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, instance_desc.m_Id, path_end - instance_desc.m_Id + 1);
                }

                if (dmGameObject::SetIdentifier(collection, instance, instance_desc.m_Id) != dmGameObject::RESULT_OK)
                {
                    dmLogError("Unable to set identifier %s. Name clash?", instance_desc.m_Id);
                }

                created_instances++;
            }
            else
            {
                dmLogError("Could not instantiate game object from prototype %s.", instance_desc.m_Prototype);
                res = dmResource::RESULT_FORMAT_ERROR; // TODO: Could be out-of-resources as well..
                break;
            }
        }

        // Setup hierarchy
        for (uint32_t i = 0; i < created_instances; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

            dmGameObject::HInstance parent = dmGameObject::GetInstanceFromIdentifier(collection, dmHashString64(instance_desc.m_Id));
            assert(parent);

            for (uint32_t j = 0; j < instance_desc.m_Children.m_Count; ++j)
            {
                dmGameObject::HInstance child = dmGameObject::GetInstanceFromIdentifier(collection, dmGameObject::GetAbsoluteIdentifier(parent, instance_desc.m_Children[j], strlen(instance_desc.m_Children[j])));
                if (child)
                {
                    dmGameObject::Result r = dmGameObject::SetParent(child, parent);
                    if (r != dmGameObject::RESULT_OK)
                    {
                        dmLogError("Unable to set %s as parent to %s (%d)", instance_desc.m_Id, instance_desc.m_Children[j], r);
                    }
                }
                else
                {
                    dmLogError("Child not found: %s", instance_desc.m_Children[j]);
                }
            }
        }

        dmGameObject::UpdateTransforms(collection);

        // Create components and set properties
        for (uint32_t i = 0; i < created_instances; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

            dmGameObject::HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, dmHashString64(instance_desc.m_Id));

            bool result = dmGameObject::CreateComponents(collection, instance);
            if (result) {
                // Set properties
                uint32_t component_instance_data_index = 0;
                dmArray<Prototype::Component>& components = instance->m_Prototype->m_Components;
                uint32_t comp_count = components.Size();
                for (uint32_t comp_i = 0; comp_i < comp_count; ++comp_i)
                {
                    Prototype::Component& component = components[comp_i];
                    ComponentType* type = component.m_Type;
                    if (type->m_SetPropertiesFunction != 0x0)
                    {
                        if (!type->m_InstanceHasUserData)
                        {
                            dmLogError("Unable to set properties for the component '%s' in game object '%s' since it has no ability to store them.", dmHashReverseSafe64(component.m_Id), instance_desc.m_Id);
                            res = dmResource::RESULT_FORMAT_ERROR;
                            goto bail;
                        }
                        ComponentSetPropertiesParams set_params;
                        set_params.m_Instance = instance;
                        uint32_t comp_prop_count = instance_desc.m_ComponentProperties.m_Count;
                        for (uint32_t prop_i = 0; prop_i < comp_prop_count; ++prop_i)
                        {
                            const dmGameObjectDDF::ComponentPropertyDesc& comp_prop = instance_desc.m_ComponentProperties[prop_i];
                            if (dmHashString64(comp_prop.m_Id) == component.m_Id)
                            {
                                bool r = CreatePropertySetUserData(&comp_prop.m_PropertyDecls, &set_params.m_PropertySet.m_UserData);
                                if (!r)
                                {
                                    dmLogError("Could not read properties of game object '%s' in collection %s.", instance_desc.m_Id, params.m_Filename);
                                    res = dmResource::RESULT_FORMAT_ERROR;
                                    goto bail;
                                }
                                else
                                {
                                    set_params.m_PropertySet.m_GetPropertyCallback = GetPropertyCallbackDDF;
                                    set_params.m_PropertySet.m_FreeUserDataCallback = DestroyPropertySetUserData;
                                }
                                break;
                            }
                        }
                        uintptr_t* component_instance_data = &instance->m_ComponentInstanceUserData[component_instance_data_index];
                        set_params.m_UserData = component_instance_data;
                        type->m_SetPropertiesFunction(set_params);
                    }
                    if (component.m_Type->m_InstanceHasUserData)
                        ++component_instance_data_index;
                }
            } else {
                dmGameObject::ReleaseIdentifier(collection, instance);
                dmGameObject::UndoNewInstance(collection, instance);
                res = dmResource::RESULT_FORMAT_ERROR;
            }
        }

        if (collection_desc->m_CollectionInstances.m_Count != 0)
            dmLogError("Sub collections must be merged before loading.");

        params.m_Resource->m_Resource = (void*) collection;
        {
            uint32_t size = sizeof(Collection);
            size += collection->m_InstanceIndices.Capacity()*sizeof(uint16_t);
            size += collection->m_WorldTransforms.Capacity()*sizeof(Matrix4);
            size += collection->m_IDToInstance.Capacity()*(sizeof(Instance*)+sizeof(dmhash_t));
            size += collection->m_InputFocusStack.Capacity()*sizeof(Instance*);
            size += collection->m_Instances.Capacity()*sizeof(Instance*);
            params.m_Resource->m_ResourceSize = size;
        }
bail:
        dmDDF::FreeMessage(collection_desc);

        if ((res != dmResource::RESULT_OK) && (collection != 0x0))
        {
            UnloadPropertyResources(params.m_Factory, collection->m_PropertyResources);
            // Loading of root-collection is responsible for deleting
            DeleteCollection(collection);
        }

        dmMutex::Unlock(regist->m_Mutex);
        return res;
    }

    dmResource::Result ResCollectionDestroy(const dmResource::ResourceDestroyParams& params)
    {
        HCollection collection = (HCollection) params.m_Resource->m_Resource;
        UnloadPropertyResources(params.m_Factory, collection->m_PropertyResources);
        DeleteCollection(collection);
        return dmResource::RESULT_OK;
    }
}
