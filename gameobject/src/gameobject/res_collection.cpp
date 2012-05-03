#include "res_collection.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include "gameobject.h"
#include "gameobject_private.h"
#include "gameobject_props.h"

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
            regist->m_CurrentCollection = collection;
            regist->m_AccumulatedTranslation = Vector3(0, 0, 0);
            regist->m_AccumulatedRotation = Quat::identity();

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
                Quat rot = regist->m_AccumulatedRotation * instance_desc.m_Rotation;
                Vector3 pos = rotate(regist->m_AccumulatedRotation, Vector3(instance_desc.m_Position)) + regist->m_AccumulatedTranslation;

                dmGameObject::SetPosition(instance, Point3(pos));
                dmGameObject::SetRotation(instance, rot);

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
                    // TODO: fix this better
                    uint32_t next_component_instance_data = 0;
                    uint32_t comp_count = components.Size();
                    uintptr_t* component_instance_data = 0;
                    for (uint32_t j = 0; j < comp_count; ++j)
                    {
                        Prototype::Component& component = components[i];
                        ComponentType* component_type = component.m_Type;
                        if (component_type->m_InstanceHasUserData)
                        {
                            component_instance_data = &instance->m_ComponentInstanceUserData[next_component_instance_data++];
                        }
                    }
                    if (type->m_SetPropertiesFunction != 0x0)
                    {
                        ComponentSetPropertiesParams params;
                        params.m_Instance = instance;
                        const uint32_t buffer_size = 1024;
                        uint8_t buffer[buffer_size];
                        uint32_t actual = SerializeProperties(comp_prop.m_Properties.m_Data, comp_prop.m_Properties.m_Count, buffer, buffer_size);
                        if (actual == 0)
                        {
                            dmLogError("Could not instantiate game object '%s' in collection %s.", instance_desc.m_Id, filename);
                            res = dmResource::RESULT_FORMAT_ERROR;
                            goto bail;
                        }
                        else if (buffer_size < actual)
                        {
                            dmLogError("Properties could not be stored when loading %s: too many properties.", filename);
                            res = dmResource::RESULT_FORMAT_ERROR;
                            goto bail;
                        }
                        else
                        {
                            params.m_PropertyBuffer = buffer;
                            params.m_PropertyBufferSize = actual;
                            params.m_UserData = component_instance_data;
                            type->m_SetPropertiesFunction(params);
                        }
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
                    // Reverse transform
                    Quat inv_acc_rot = conj(regist->m_AccumulatedRotation);
                    Quat rot = inv_acc_rot * dmGameObject::GetRotation(child);
                    Point3 pos = Point3(rotate(inv_acc_rot, Vector3(dmGameObject::GetPosition(child) - regist->m_AccumulatedTranslation)));
                    dmGameObject::SetPosition(child, pos);
                    dmGameObject::SetRotation(child, rot);
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

        // Load sub collections
        for (uint32_t i = 0; i < collection_desc->m_CollectionInstances.m_Count; ++i)
        {
            dmGameObjectDDF::CollectionInstanceDesc& coll_instance_desc = collection_desc->m_CollectionInstances[i];

            dmStrlCpy(prev_identifier_path, regist->m_CurrentIdentifierPath, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
            dmStrlCat(regist->m_CurrentIdentifierPath, coll_instance_desc.m_Id, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);
            dmStrlCat(regist->m_CurrentIdentifierPath, ID_SEPARATOR, DM_GAMEOBJECT_CURRENT_IDENTIFIER_PATH_MAX);

            Collection* child_coll;
            Vector3 prev_translation = regist->m_AccumulatedTranslation;
            Quat prev_rotation = regist->m_AccumulatedRotation;

            Quat rot = regist->m_AccumulatedRotation * coll_instance_desc.m_Rotation;
            Vector3 trans = (regist->m_AccumulatedRotation * Quat(Vector3(coll_instance_desc.m_Position), 0.0f) * conj(regist->m_AccumulatedRotation)).getXYZ()
                       + regist->m_AccumulatedTranslation;

            regist->m_AccumulatedRotation = rot;
            regist->m_AccumulatedTranslation = trans;

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

            regist->m_AccumulatedTranslation = prev_translation;
            regist->m_AccumulatedRotation = prev_rotation;
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
