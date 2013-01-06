#include "res_collection.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include "gameobject.h"
#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_ddf.h"

#include "../proto/gameobject_ddf.h"

namespace dmGameObject
{
    dmResource::Result ResCollectionCreate(dmResource::HFactory factory,
                                                void* context,
                                                const void* buffer, uint32_t buffer_size,
                                                dmResource::SResourceDescriptor* resource,
                                                const char* filename)
    {
        Register* regist = (Register*) context;
        char prev_identifier_path[DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX];
        char tmp_ident[DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX];

        dmGameObjectDDF::CollectionDesc* collection_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(buffer, buffer_size, &collection_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }
        dmResource::Result res = dmResource::RESULT_OK;

        // NOTE: Be careful about control flow. See below with dmMutex::Unlock, return, etc
        dmMutex::Lock(regist->m_Mutex);

        HCollection collection;
        bool loading_root;
        if (regist->m_CurrentCollection == 0)
        {
            loading_root = true;
            // TODO: How to configure 1024. In collection?
            collection = NewCollection(collection_desc->m_Name, factory, regist, 1024);
            if (collection == 0)
            {
                dmMutex::Unlock(regist->m_Mutex);
                return dmResource::RESULT_OUT_OF_RESOURCES;
            }
            collection->m_ScaleAlongZ = collection_desc->m_ScaleAlongZ;
            regist->m_CurrentCollection = collection;
            regist->m_AccumulatedTransform.SetIdentity();

            // NOTE: Root-collection name is not prepended to identifier
            prev_identifier_path[0] = '\0';
            regist->m_CurrentIdentifierPath[0] = *ID_SEPARATOR;
            regist->m_CurrentIdentifierPath[1] = '\0';
        }
        else
        {
            loading_root = false;
            collection = regist->m_CurrentCollection;
            dmStrlCpy(prev_identifier_path, regist->m_CurrentIdentifierPath, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
        }

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            dmGameObject::HInstance instance = dmGameObject::New(collection, instance_desc.m_Prototype);
            if (instance != 0x0)
            {
                instance->m_ScaleAlongZ = collection_desc->m_ScaleAlongZ;
                dmTransform::TransformS1 instance_t(Vector3(instance_desc.m_Position), instance_desc.m_Rotation, instance_desc.m_Scale);
                if (instance->m_ScaleAlongZ)
                {
                    instance->m_Transform = dmTransform::Mul(regist->m_AccumulatedTransform, instance_t);
                }
                else
                {
                    instance->m_Transform = dmTransform::MulNoScaleZ(regist->m_AccumulatedTransform, instance_t);
                }

                dmHashInit64(&instance->m_CollectionPathHashState, true);
                dmHashUpdateBuffer64(&instance->m_CollectionPathHashState, regist->m_CurrentIdentifierPath, strlen(regist->m_CurrentIdentifierPath));

                dmStrlCpy(tmp_ident, regist->m_CurrentIdentifierPath, sizeof(tmp_ident));
                dmStrlCat(tmp_ident, instance_desc.m_Id, sizeof(tmp_ident));

                if (dmGameObject::SetIdentifier(collection, instance, tmp_ident) != dmGameObject::RESULT_OK)
                {
                    dmLogError("Unable to set identifier %s for %s. Name clash?", tmp_ident, instance_desc.m_Id);
                }

                // Set properties

                uint32_t comp_prop_count = instance_desc.m_ComponentProperties.m_Count;
                for (uint32_t i = 0; i < comp_prop_count; ++i)
                {
                    const dmGameObjectDDF::ComponentPropertyDesc& comp_prop = instance_desc.m_ComponentProperties[i];
                    uint8_t index;
                    dmGameObject::Result result = GetComponentIndex(instance, dmHashString64(comp_prop.m_Id), &index);
                    if (result == RESULT_COMPONENT_NOT_FOUND)
                    {
                        dmLogWarning("The component '%s' could not be found in '%s' when setting properties.", comp_prop.m_Id, instance_desc.m_Id);
                        continue;
                    }
                    dmArray<Prototype::Component>& components = instance->m_Prototype->m_Components;
                    ComponentType* type = components[index].m_Type;
                    if (!type->m_InstanceHasUserData || type->m_SetPropertiesFunction == 0x0)
                    {
                        dmLogError("Unable to set properties for the component '%s' in game object '%s' since it has no ability to store them.", comp_prop.m_Id, instance_desc.m_Id);
                        res = dmResource::RESULT_FORMAT_ERROR;
                        goto bail;
                    }

                    ComponentSetPropertiesParams params;
                    params.m_Instance = instance;
                    bool r = CreatePropertyDataUserData(&comp_prop.m_PropertyDecls, &params.m_PropertyData.m_UserData);
                    if (!r)
                    {
                        dmLogError("Could not instantiate game object '%s' in collection %s.", instance_desc.m_Id, filename);
                        res = dmResource::RESULT_FORMAT_ERROR;
                        goto bail;
                    }
                    else
                    {
                        params.m_PropertyData.m_GetPropertyCallback = GetPropertyCallbackDDF;
                        params.m_PropertyData.m_FreeUserDataCallback = DestroyPropertyDataUserData;
                        uint32_t component_instance_data_index = 0;
                        for (uint32_t j = 0; j < index; ++j)
                        {
                            if (components[i].m_Type->m_InstanceHasUserData)
                                ++component_instance_data_index;
                        }
                        uintptr_t* component_instance_data = &instance->m_ComponentInstanceUserData[component_instance_data_index];
                        params.m_UserData = component_instance_data;
                        type->m_SetPropertiesFunction(params);
                    }
                }
            }
            else
            {
                dmLogError("Could not instantiate game object from prototype %s.", instance_desc.m_Prototype);
                res = dmResource::RESULT_FORMAT_ERROR; // TODO: Could be out-of-resources as well..
                goto bail;
            }
        }

        // Setup hierarchy
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];

            dmStrlCpy(tmp_ident, regist->m_CurrentIdentifierPath, sizeof(tmp_ident));
            dmStrlCat(tmp_ident, instance_desc.m_Id, sizeof(tmp_ident));

            dmGameObject::HInstance parent = dmGameObject::GetInstanceFromIdentifier(collection, dmHashString64(tmp_ident));
            assert(parent);

            for (uint32_t j = 0; j < instance_desc.m_Children.m_Count; ++j)
            {
                dmGameObject::HInstance child = dmGameObject::GetInstanceFromIdentifier(collection, dmGameObject::GetAbsoluteIdentifier(parent, instance_desc.m_Children[j], strlen(instance_desc.m_Children[j])));
                if (child)
                {
                    dmGameObject::Result r = dmGameObject::SetParent(child, parent);
                    if (r == dmGameObject::RESULT_OK)
                    {
                        // Reverse transform
                        if (child->m_ScaleAlongZ)
                        {
                            child->m_Transform = dmTransform::Mul(dmTransform::Inv(regist->m_AccumulatedTransform), child->m_Transform);
                        }
                        else
                        {
                            child->m_Transform = dmTransform::MulNoScaleZ(dmTransform::Inv(regist->m_AccumulatedTransform), child->m_Transform);
                        }
                    }
                    else
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

        // Load sub collections
        for (uint32_t i = 0; i < collection_desc->m_CollectionInstances.m_Count; ++i)
        {
            dmGameObjectDDF::CollectionInstanceDesc& coll_instance_desc = collection_desc->m_CollectionInstances[i];

            dmStrlCpy(prev_identifier_path, regist->m_CurrentIdentifierPath, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
            dmStrlCat(regist->m_CurrentIdentifierPath, coll_instance_desc.m_Id, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
            dmStrlCat(regist->m_CurrentIdentifierPath, ID_SEPARATOR, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

            Collection* child_coll;
            dmTransform::TransformS1 prev_transform = regist->m_AccumulatedTransform;
            dmTransform::TransformS1 coll_instance_t(Vector3(coll_instance_desc.m_Position),
                    coll_instance_desc.m_Rotation, coll_instance_desc.m_Scale);

            regist->m_AccumulatedTransform = Mul(regist->m_AccumulatedTransform, coll_instance_t);

            dmResource::Result r = dmResource::Get(factory, coll_instance_desc.m_Collection, (void**) &child_coll);
            if (r != dmResource::RESULT_OK)
            {
                res = r;
                goto bail;
            }
            else
            {
                assert(child_coll != collection);
                dmResource::Release(factory, (void*) child_coll);
            }

            dmStrlCpy(regist->m_CurrentIdentifierPath, prev_identifier_path, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

            regist->m_AccumulatedTransform = prev_transform;
        }

        if (loading_root)
        {
            resource->m_Resource = (void*) collection;
        }
        else
        {
            // We must create a child collection. We can't return "collection" and release.
            // The root collection is not yet created.
            resource->m_Resource = (void*) new Collection(factory, regist, 1);
        }
bail:
        dmDDF::FreeMessage(collection_desc);
        dmStrlCpy(regist->m_CurrentIdentifierPath, prev_identifier_path, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

        if (loading_root && res != dmResource::RESULT_OK)
        {
            // Loading of root-collection is responsible for deleting
            DeleteCollection(collection);
        }

        if (loading_root)
        {
            // We must reset this to next load.
            regist->m_CurrentCollection = 0;
        }

        dmMutex::Unlock(regist->m_Mutex);
        return res;
    }

    dmResource::Result ResCollectionDestroy(dmResource::HFactory factory,
                                                 void* context,
                                                 dmResource::SResourceDescriptor* resource)
    {
        HRegister regist = (HRegister)context;
        HCollection collection = (HCollection) resource->m_Resource;
        if (regist->m_CurrentCollection != 0x0)
        {
            delete collection;
        }
        else
        {
            DeleteCollection(collection);
        }
        return dmResource::RESULT_OK;
    }
}
