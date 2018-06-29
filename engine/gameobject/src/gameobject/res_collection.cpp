#include "res_collection.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include "gameobject.h"
#include "gameobject_private.h"
#include "gameobject_props.h"
#include "gameobject_props_ddf.h"

#include "../proto/gameobject/gameobject_ddf.h"

namespace dmGameObject
{
	static void ReleaseResources(dmResource::HFactory factory, HCollection hcollection)
    {
    	DeleteCollection(hcollection); // delay delete
    }

	static dmResource::Result AcquireResources(const char* name, dmResource::HFactory factory, dmGameObject::HRegister regist, dmGameObjectDDF::CollectionDesc* collection_desc, const char* filename, HCollection* out_hcollection)
	{
        // NOTE: Be careful about control flow. See below with dmMutex::Unlock, return, etc
        dmResource::Result res = dmResource::RESULT_OK;

        uint32_t collection_capacity = dmGameObject::GetCollectionDefaultCapacity(regist);

        HCollection hcollection = NewCollection(collection_desc->m_Name, factory, regist, collection_capacity);
        if (hcollection == 0)
        {
            dmDDF::FreeMessage(collection_desc);
            return dmResource::RESULT_OUT_OF_RESOURCES;
        }
        Collection* collection = hcollection->m_Collection;
        collection->m_ScaleAlongZ = collection_desc->m_ScaleAlongZ;

        uint32_t created_instances = 0;
        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            Prototype* proto = 0x0;
            dmGameObject::HInstance instance = 0x0;
            if (instance_desc.m_Prototype != 0x0)
            {
                dmResource::Result error = dmResource::Get(factory, instance_desc.m_Prototype, (void**)&proto);
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
                if (scale.getX() == 0 && scale.getY() == 0 && scale.getZ() == 0) {
                    scale = Vector3(instance_desc.m_Scale, instance_desc.m_Scale, instance_desc.m_Scale);
                }

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
                                    dmLogError("Could not read properties of game object '%s' in collection %s.", instance_desc.m_Id, filename);
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

bail:
        if (res != dmResource::RESULT_OK)
        {
            // Loading of root-collection is responsible for deleting
            DeleteCollection(collection);
            collection = 0;
            hcollection = 0;
        }

        *out_hcollection = hcollection;
        return res;
    }

    dmResource::Result ResCollectionPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameObjectDDF::CollectionDesc* collection_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(params.m_Buffer, params.m_BufferSize, &collection_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        for (uint32_t i = 0; i < collection_desc->m_Instances.m_Count; ++i)
        {
            const dmGameObjectDDF::InstanceDesc& instance_desc = collection_desc->m_Instances[i];
            if (instance_desc.m_Prototype != 0x0)
            {
                dmResource::PreloadHint(params.m_HintInfo, instance_desc.m_Prototype);
            }
        }

        *params.m_PreloadData = collection_desc;
        return dmResource::RESULT_OK;
    }

    static size_t CalcSize(Collection* collection)
    {
    	size_t size = sizeof(Collection) + sizeof(CollectionHandle);
        size += collection->m_InstanceIndices.Capacity()*sizeof(uint16_t);
        size += collection->m_WorldTransforms.Capacity()*sizeof(Matrix4);
        size += collection->m_IDToInstance.Capacity()*(sizeof(Instance*)+sizeof(dmhash_t));
        size += collection->m_InputFocusStack.Capacity()*sizeof(Instance*);
        size += collection->m_Instances.Capacity()*sizeof(Instance*);
        return size;
    }

    dmResource::Result ResCollectionCreate(const dmResource::ResourceCreateParams& params)
    {
        Register* regist = (Register*) params.m_Context;
        dmGameObjectDDF::CollectionDesc* collection_desc = (dmGameObjectDDF::CollectionDesc*) params.m_PreloadData;

        HCollection hcollection = 0;
        dmResource::Result res = AcquireResources(collection_desc->m_Name, params.m_Factory, regist, collection_desc, params.m_Filename, &hcollection);
        if (res != dmResource::RESULT_OK)
        {
        	return res;
        }

        dmDDF::FreeMessage(collection_desc);

        params.m_Resource->m_Resource = hcollection;
        params.m_Resource->m_ResourceSize = CalcSize(hcollection->m_Collection);
        return res;
    }

    dmResource::Result ResCollectionDestroy(const dmResource::ResourceDestroyParams& params)
    {
        HCollection hcollection = (HCollection) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, hcollection);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResCollectionRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmGameObjectDDF::CollectionDesc* collection_desc;
        dmDDF::Result e = dmDDF::LoadMessage<dmGameObjectDDF::CollectionDesc>(params.m_Buffer, params.m_BufferSize, &collection_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        HCollection prev_hcollection = (HCollection) params.m_Resource->m_Resource;
        Collection* prev_collection = prev_hcollection->m_Collection;
        Register* regist = (Register*) params.m_Context;
        bool was_initialized = IsCollectionInitialized(prev_collection);

        if (was_initialized)
        {
            dmGameObject::Final(prev_hcollection);
        }
        dmGameObject::DetachCollection(prev_collection);

        HCollection delete_hcollection = 0;
        dmResource::Result res = AcquireResources(collection_desc->m_Name, params.m_Factory, regist, collection_desc, params.m_Filename, &delete_hcollection);
        if (dmResource::RESULT_OK == res)
        {
        	// We cannot simply swap the HCollection, since that's the resource that has been handed out
        	// so we swap the internal pointers, and tag the new hcollection for deletion
        	Collection* new_collection = delete_hcollection->m_Collection;

        	prev_hcollection->m_Collection = new_collection;
            prev_collection->m_HCollection = delete_hcollection;
        	delete_hcollection->m_Collection = prev_collection;
			new_collection->m_HCollection = prev_hcollection;

			if( was_initialized )
			{
				if (!dmGameObject::Init(prev_hcollection) ) // this is the new collection
				{
					dmLogWarning("Failed to initialize collection: %s", collection_desc->m_Name);

                    // For those things that actually did manage to initialize (e.g. send events)
                    dmGameObject::Final(prev_hcollection);

                    // Swap them back
                    prev_hcollection->m_Collection = prev_collection;
                    prev_collection->m_HCollection = prev_hcollection;
                    delete_hcollection->m_Collection = new_collection;
                    new_collection->m_HCollection = delete_hcollection;

                    dmGameObject::DeleteCollection(new_collection);

                    // Reattach/reinit the collection
                    dmGameObject::AttachCollection(prev_collection, collection_desc->m_Name, params.m_Factory, regist, prev_hcollection);
                    if (was_initialized)
                    {
                        dmGameObject::Init(prev_hcollection);
                    }

                    dmDDF::FreeMessage(collection_desc);
                    return dmResource::RESULT_UNKNOWN_ERROR;
				}
			}

            dmGameObject::DeleteCollection(delete_hcollection->m_Collection);

            params.m_Resource->m_PrevResource = 0;
        	params.m_Resource->m_ResourceSize = CalcSize(prev_hcollection->m_Collection);
        }
        else
        {
        	dmGameObject::AttachCollection(prev_collection, collection_desc->m_Name, params.m_Factory, regist, prev_hcollection);
        }
        dmDDF::FreeMessage(collection_desc);
        return res;
    }


}
