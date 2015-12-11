#include "gameobject.h"
#include "gameobject_private.h"
#include <dlib/log.h>

#include <sol/runtime.h>
#include <sol/reflect.h>
#include <dlib/sol.h>

namespace dmGameObject
{

    extern "C"
    {
        struct SolComponentNewWorldParams
        {
            ::Any m_Context;
            uint32_t m_MaxInstances;
            ::Any m_World;
        };

        struct SolComponentDeleteWorldParams
        {
            ::Any m_Context;
            ::Any m_World;
        };

        struct SolComponentCreateParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Collection;
            dmSol::HProxy m_Instance;
            Point3 m_Position;
            Quat m_Rotation;
            ::Any m_Resource;
            ::Any m_World;
            uint8_t m_ComponentIndex;
            ::Any m_UserData; // output
        };

        struct SolComponentDestroyParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Collection;
            dmSol::HProxy m_Instance;
            ::Any m_World;
            ::Any m_UserData;
        };

        // these are the same
        typedef SolComponentDestroyParams SolComponentInitParams;
        typedef SolComponentDestroyParams SolComponentFinalParams;
        typedef SolComponentDestroyParams SolComponentAddToUpdateParams;

        struct SolComponentsUpdateParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Collection;
            ::Any m_World;
            float m_DT;
        };

        struct SolComponentsPostUpdateParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Collection;
            ::Any m_World;
        };

        struct SolComponentsRenderParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Collection;
            ::Any m_World;
            ::Any m_UserData;
        };

        struct SolComponentOnMessageParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Instance;
            ::Any m_World;
            ::Any m_UserData;
            dmSol::Handle m_Message;
            dmhash_t m_MessageId;
        };

        struct SolComponentSetPropertiesParams
        {
            dmSol::HProxy m_Instance;
            // PropertySet m_PropertySet;
            ::Any m_UserData;
        };

        struct SolComponentGetPropertyParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Instance;
            ::Any m_World;
            dmhash_t m_PropertyId;
            ::Any m_UserData;
            ::Any m_Value;
            dmhash_t m_ElementIds[4];
        };

        struct SolComponentSetPropertyParams
        {
            ::Any m_Context;
            dmSol::HProxy m_Instance;
            ::Any m_World;
            dmhash_t m_PropertyId;
            ::Any m_UserData;
            ::Any m_Value;
        };

        SolComponentNewWorldParams* gameobject_get_comp_new_world_params();
        SolComponentDeleteWorldParams* gameobject_get_comp_delete_world_params();
        SolComponentCreateParams* gameobject_get_comp_create_params();
        SolComponentDestroyParams* gameobject_get_comp_destroy_params();
        SolComponentInitParams* gameobject_get_comp_init_params();
        SolComponentFinalParams* gameobject_get_comp_final_params();
        SolComponentAddToUpdateParams* gameobject_get_comp_add_to_update_params();
        SolComponentsUpdateParams* gameobject_get_comps_update_params();
        SolComponentsPostUpdateParams* gameobject_get_comps_post_update_params();
        SolComponentsRenderParams* gameobject_get_comps_render_params();
        SolComponentOnMessageParams* gameobject_get_comp_on_message_params();
        SolComponentSetPropertiesParams* gameobject_get_comp_set_properties_params();
        SolComponentSetPropertyParams* gameobject_get_comp_set_property_params();
        SolComponentGetPropertyParams* gameobject_get_comp_get_property_params();
    }

    static ::Any GetSolWorld(const ComponentWorld& world)
    {
        if (!world.m_Ptr)
            return reflect_create_reference_any(0, 0);
        else
            return reflect_create_reference_any(world.m_SolType, world.m_Ptr);
    }

    CreateResult SolComponentNewWorld(const ComponentNewWorldParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;

        SolComponentNewWorldParams* sol_params = gameobject_get_comp_new_world_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_MaxInstances = params.m_MaxInstances;
        CreateResult r = (CreateResult) comp->m_NewWorldFunction(sol_params);
        params.m_World->m_Ptr = (void*)reflect_get_any_value(sol_params->m_World);
        params.m_World->m_SolType = reflect_get_any_type(sol_params->m_World);

        dmSol::SafePin(sol_params->m_World);

        // Optimization: Selectively clear the fields that contain references since the params object is shared
        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    CreateResult SolComponentDeleteWorld(const ComponentDeleteWorldParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentDeleteWorldParams* sol_params = gameobject_get_comp_delete_world_params();

        ::Any world = GetSolWorld(params.m_World);

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = world;
        CreateResult r = (CreateResult) comp->m_DeleteWorldFunction(sol_params);

        dmSol::SafeUnpin(world);

        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    // For now only because Any won't fit into uintptr_t
    // There is some low hanging allocation fruit here for fixing this
    struct UserDataHolder
    {
        ::Any m_UserData;
    };

    CreateResult SolComponentCreate(const ComponentCreateParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        (*params.m_UserData) = 0;

        if (!comp->m_CreateFunction)
        {
            return CREATE_RESULT_OK;
        }

        SolComponentCreateParams* sol_params = gameobject_get_comp_create_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);
        sol_params->m_Position = params.m_Position;
        sol_params->m_Rotation = params.m_Rotation;
        sol_params->m_ComponentIndex = params.m_ComponentIndex;

        if (dmResource::GetSolAnyPtr(params.m_Collection->m_Factory, params.m_Resource, &sol_params->m_Resource) != dmResource::RESULT_OK)
        {
            dmLogError("Could not get Sol pointer for resource.");
            memset(sol_params, 0x00, sizeof(*sol_params));
            return CREATE_RESULT_UNKNOWN_ERROR;
        }

        CreateResult r = (CreateResult) comp->m_CreateFunction(sol_params);

        if (r == CREATE_RESULT_OK)
        {
            // Create the little user data object that holds the component's any data
            UserDataHolder* comp_data = new UserDataHolder();
            comp_data->m_UserData = sol_params->m_UserData;
            (*params.m_UserData) = (uintptr_t) comp_data;

            dmSol::SafePin(comp_data->m_UserData);
        }

        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    CreateResult SolComponentInit(const ComponentInitParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentInitParams* sol_params = gameobject_get_comp_init_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);

        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        if (comp_data)
        {
            sol_params->m_UserData = comp_data->m_UserData;
        }

        CreateResult res = (CreateResult) comp->m_InitFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return res;
    }

    CreateResult SolComponentFinal(const ComponentFinalParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentFinalParams* sol_params = gameobject_get_comp_final_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);

        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        if (comp_data)
        {
            sol_params->m_UserData = comp_data->m_UserData;
        }

        CreateResult res = (CreateResult) comp->m_FinalFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return res;
    }

    CreateResult SolComponentAddToUpdate(const ComponentAddToUpdateParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentAddToUpdateParams* sol_params = gameobject_get_comp_add_to_update_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);

        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        if (comp_data)
        {
            sol_params->m_UserData = comp_data->m_UserData;
        }

        CreateResult r = (CreateResult) comp->m_AddToUpdateFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    CreateResult SolComponentDestroy(const ComponentDestroyParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        CreateResult r = CREATE_RESULT_OK;

        if (comp->m_DestroyFunction)
        {
            SolComponentDestroyParams* sol_params = gameobject_get_comp_destroy_params();
            sol_params->m_Context = comp->m_Context;
            sol_params->m_World = GetSolWorld(params.m_World);
            sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
            sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);

            if (comp_data)
            {
                sol_params->m_UserData = comp_data->m_UserData;
            }

            r = (CreateResult) comp->m_DestroyFunction(sol_params);
            memset(sol_params, 0x00, sizeof(*sol_params));
        }

        if (comp_data)
        {
            dmSol::SafeUnpin(comp_data->m_UserData);
            delete comp_data;
        }

        return r;
    }

    UpdateResult SolComponentUpdate(const ComponentsUpdateParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentsUpdateParams* sol_params = gameobject_get_comps_update_params();
        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        sol_params->m_DT = params.m_UpdateContext->m_DT;
        UpdateResult r = (UpdateResult) comp->m_UpdateFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    UpdateResult SolComponentPostUpdate(const ComponentsPostUpdateParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentsPostUpdateParams* sol_params = gameobject_get_comps_post_update_params();
        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        UpdateResult r = (UpdateResult) comp->m_PostUpdateFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    UpdateResult SolComponentRender(const ComponentsRenderParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentsRenderParams* sol_params = gameobject_get_comps_render_params();
        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Collection = GetCollectionSolProxy(params.m_Collection);
        UpdateResult r = (UpdateResult) comp->m_RenderFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return r;
    }

    static uint8_t s_MessageTypeToken;

    UpdateResult SolComponentOnMessage(const ComponentOnMessageParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentOnMessageParams* sol_params = gameobject_get_comp_on_message_params();

        dmSol::Handle msg_handle = dmSol::MakeHandle(params.m_Message, &s_MessageTypeToken);
        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);
        sol_params->m_Message = msg_handle;
        sol_params->m_MessageId = params.m_MessageId;

        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        if (comp_data)
        {
            sol_params->m_UserData = comp_data->m_UserData;
        }

        UpdateResult res = (UpdateResult) comp->m_OnMessageFunction(sol_params);

        memset(sol_params, 0x00, sizeof(*sol_params));
        dmSol::FreeHandle(msg_handle);
        return res;
    }

    PropertyResult SolComponentSetProperties(const ComponentSetPropertiesParams& params)
    {
        dmLogError("Set properties is not implemented for sol!");
        return PROPERTY_RESULT_UNSUPPORTED_OPERATION;
    }

    extern "C" ::Any gameobject_property_variant_to_any(PropertyType type);
    extern "C" PropertyType gameobject_get_variant_type(::Any any);

    PropertyResult SolComponentGetProperty(const ComponentGetPropertyParams& params, PropertyDesc& out_value)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentGetPropertyParams* sol_params = gameobject_get_comp_get_property_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);
        sol_params->m_PropertyId = params.m_PropertyId;

        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        if (comp_data)
        {
            sol_params->m_UserData = comp_data->m_UserData;
        }

        PropertyResult res = (PropertyResult) comp->m_GetPropertyFunction(sol_params);
        PropertyType type_out = (PropertyType) gameobject_get_variant_type(sol_params->m_Value);

        if (res == PROPERTY_RESULT_OK)
        {
            if (type_out != PROPERTY_TYPE_COUNT)
            {
                // assume are reference types for now (because they are)
                void *ptr;
                uint32_t size;
                uint64_t any_val = reflect_get_any_value(sol_params->m_Value);
                if (reflect_get_any_type(sol_params->m_Value)->referenced_type)
                {
                    ptr = (void*)any_val;
                    size = dmSol::SizeOf(sol_params->m_Value);
                }
                else
                {
                    ptr = &any_val;
                    size = sizeof(any_val);
                }

                assert(size > 0 && size <= sizeof(out_value.m_Variant.m_URL)); // url is biggest?
                memcpy(&out_value.m_Variant.m_V4[0], ptr, size);
                out_value.m_Variant.m_Type = type_out;
                out_value.m_ValuePtr = 0;
                for (size_t i=0;i!=4;i++)
                    out_value.m_ElementIds[i] = sol_params->m_ElementIds[i];
            }
            else
            {
                dmLogError("GetProperty returned unknown value");
                res = PROPERTY_RESULT_UNSUPPORTED_VALUE;
            }
        }

        memset(sol_params, 0x00, sizeof(*sol_params));
        return res;
    }


    PropertyResult SolComponentSetProperty(const ComponentSetPropertyParams& params)
    {
        ComponentTypeSol* comp = (ComponentTypeSol*) params.m_Context;
        SolComponentSetPropertyParams* sol_params = gameobject_get_comp_set_property_params();

        sol_params->m_Context = comp->m_Context;
        sol_params->m_World = GetSolWorld(params.m_World);
        sol_params->m_Instance = GetInstanceSolProxy(params.m_Instance);
        sol_params->m_PropertyId = params.m_PropertyId;

        // TODO: Make this support non-reference types instead of the PropertyValNumber* structs
        ::Any sol_value = gameobject_property_variant_to_any(params.m_Value.m_Type);
        void *ref_ptr = (void*)reflect_get_any_value(sol_value);
        void *value_ptr = (void*)&sol_value._value;

        switch (params.m_Value.m_Type)
        {
            case PROPERTY_TYPE_NUMBER:
            {
                *((double*)value_ptr) = params.m_Value.m_Number;
                break;
            }
            case PROPERTY_TYPE_HASH:
            {
                *((dmhash_t*)value_ptr) = params.m_Value.m_Hash;
                break;
            }
            case PROPERTY_TYPE_URL:
            {
                assert(dmSol::SizeOf(sol_value) == sizeof(params.m_Value.m_URL));
                memcpy(ref_ptr, params.m_Value.m_URL, sizeof(params.m_Value.m_URL));
                break;
            }
            case PROPERTY_TYPE_VECTOR3:
            {
                assert(dmSol::SizeOf(sol_value) >= 3*sizeof(float));
                memcpy(ref_ptr, params.m_Value.m_V4, 3*sizeof(float));
                break;
            }
            case PROPERTY_TYPE_VECTOR4:
            {
                assert(dmSol::SizeOf(sol_value) == 4*sizeof(float));
                memcpy(ref_ptr, params.m_Value.m_V4, 4*sizeof(float));
                break;
            }
            case PROPERTY_TYPE_QUAT:
            {
                assert(dmSol::SizeOf(sol_value) == 4*sizeof(float));
                memcpy(ref_ptr, params.m_Value.m_V4, 4*sizeof(float));
                break;
            }
            default:
            {
                memset(sol_params, 0x00, sizeof(*sol_params));
                return PROPERTY_RESULT_UNSUPPORTED_TYPE;
            }
        }

        sol_params->m_Value = sol_value;

        UserDataHolder* comp_data = (UserDataHolder*) (*params.m_UserData);
        if (comp_data)
        {
            sol_params->m_UserData = comp_data->m_UserData;
        }

        PropertyResult res = (PropertyResult) comp->m_SetPropertyFunction(sol_params);
        memset(sol_params, 0x00, sizeof(*sol_params));
        return res;
    }

    bool DDFMessageToSolInto(const dmDDF::Descriptor* descriptor, const ::Type* sol_type, void* msg, uint32_t size, void* output, uint32_t output_size)
    {
        // This implementation is a travesty.
        uint8_t tmp_buf_0[8*1024];
        if (size > sizeof(tmp_buf_0))
        {
            dmLogError("Failed to convert message, too big!");
            return false;
        }

        memcpy(tmp_buf_0, msg, size);
        dmDDF::RebaseMessagePointers(descriptor, tmp_buf_0, tmp_buf_0, 0);

        // Save and then reload. Let's copy the message back and forth a million times.
        // Loading it will ensure proper SOL allocation
        uint8_t tmp_buf_1[8*1024];
        dmArray<uint8_t> serialized(tmp_buf_1, 0, sizeof(tmp_buf_1));
        if (dmDDF::RESULT_OK != dmDDF::SaveMessageToArray(tmp_buf_0, descriptor, serialized))
        {
            dmLogError("Failed to convert message, too big?");
            return false;
        }

        return dmDDF::RESULT_OK == dmDDF::LoadMessage(serialized.Begin(), serialized.Size(), descriptor, &output,
                                                      dmDDF::OPTION_PRE_ALLOCATED | dmDDF::OPTION_SOL, &output_size);
    }

    extern "C"
    {
        void SolGameObjectGetWorldMatrix(dmSol::HProxy instance_proxy, float* out)
        {
            HInstance instance = (HInstance) dmSol::ProxyGet(instance_proxy);
            if (!instance)
            {
                dmLogError("Invalid instance supplied");
                return;
            }
            HCollection collection = instance->m_Collection;
            Matrix4 mtx = collection->m_WorldTransforms[instance->m_Index];
            for (int c=0;c<4;c++)
            {
                for (int r=0;r<4;r++)
                {
                    *out++ = mtx.getElem(c, r);
                }
            }
        }

        bool SolGameObjectGetScaleAlongZ(dmSol::HProxy instance_proxy)
        {
            HInstance instance = (HInstance) dmSol::ProxyGet(instance_proxy);
            if (!instance)
            {
                dmLogError("Invalid instance supplied");
                return false;
            }
            return ScaleAlongZ(instance);
        }

        // Grand function to convert a message to sol-accessible format!
        ::Any SolGameObjectGetMessageDataInternal(dmSol::Handle msg_handle, ::Any empty_value)
        {
            dmMessage::Message* msg = (dmMessage::Message*) dmSol::GetHandle(msg_handle, &s_MessageTypeToken);
            if (!msg)
            {
                dmLogError("Supplied message is not accessible, returning empty value.");
                return empty_value;
            }

            const dmDDF::Descriptor* desc = (const dmDDF::Descriptor*) msg->m_Descriptor;
            if (desc != 0)
            {
                ::Type* msg_type = (::Type*) dmDDF::GetSolTypeFromHash(msg->m_Id);
                if (msg_type && msg_type->referenced_type && msg_type->referenced_type->struct_type)
                {
                    // allocate and load into
                    void* sol_msg = runtime_alloc_struct(msg_type->referenced_type);
                    bool res = DDFMessageToSolInto(desc, msg_type, msg->m_Data, msg->m_DataSize, sol_msg, msg_type->referenced_type->struct_type->size);
                    runtime_unpin(sol_msg);
                    if (!res)
                    {
                        return empty_value;
                    }
                    return reflect_create_reference_any(msg_type, sol_msg);
                }
                else
                {
                    const char *name = (const char*)dmHashReverse64(msg->m_Id, 0);
                    dmLogError("No SOL type for %s %s", desc->m_Name, name);
                }
            }
            return empty_value;
        }

        // Loads message data into already allocated struct
        bool SolGameObjectGetMessageDataInto(dmSol::Handle msg_handle, ::Any into)
        {
            dmMessage::Message* msg = (dmMessage::Message*) dmSol::GetHandle(msg_handle, &s_MessageTypeToken);
            if (!msg)
            {
                dmLogError("Supplied message is not accessible, returning empty value.");
                return false;
            }

            const dmDDF::Descriptor* desc = (const dmDDF::Descriptor*) msg->m_Descriptor;
            if (desc != 0)
            {
                const ::Type* msg_type = dmDDF::GetSolTypeFromHash(msg->m_Id);
                if (!msg_type || !msg_type->referenced_type || !msg_type->referenced_type->struct_type)
                {
                    dmLogError("Supplied message does not support loading into (no sol type)");
                    return false;
                }
                if (msg_type != reflect_get_any_type(into))
                {
                    dmLogError("Supplied message message does not match supplied object");
                    return false;
                }
                return DDFMessageToSolInto((const dmDDF::Descriptor*)msg->m_Descriptor, msg_type, msg->m_Data, msg->m_DataSize,
                                           (void*)reflect_get_any_value(into), msg_type->referenced_type->struct_type->size);
            }
            return false;
        }

        struct ComponentMessagePath
        {
            dmSol::HProxy m_InstanceProxy;
            dmhash_t m_Fragment;
        };

        bool SolGameObjectGetMessageSenderComponentPath(dmSol::HProxy instance_proxy, dmSol::Handle msg_handle, ComponentMessagePath* path)
        {
            HInstance instance = (HInstance) dmSol::ProxyGet(instance_proxy);
            dmMessage::Message* msg = (dmMessage::Message*) dmSol::GetHandle(msg_handle, &s_MessageTypeToken);
            if (!msg || !instance || !path)
            {
                dmLogError("Invalid parameters to function");
                return false;
            }
            HInstance sender_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(instance), msg->m_Sender.m_Path);
            if (sender_instance)
            {
                path->m_InstanceProxy = GetInstanceSolProxy(sender_instance);
                path->m_Fragment = msg->m_Sender.m_Fragment;
                return true;
            }
            return true;
        }

        bool SolGameObjectPostMessage(dmSol::HProxy sender_proxy, uint8_t sender_component_index, ComponentMessagePath* receiver_path, ::Any message)
        {
            if (!sender_proxy || !receiver_path)
            {
                dmLogError("post_message: invalid arguments sent");
                return false;
            }

            HInstance sender_instance = (HInstance) dmSol::ProxyGet(sender_proxy);
            HInstance receiver_instance = (HInstance) dmSol::ProxyGet(receiver_path->m_InstanceProxy);
            if (!sender_instance || !receiver_instance)
            {
                dmLogError("post_message: invalid instances supplied");
                return false;
            }

            dmMessage::URL receiver;
            receiver.m_Socket = dmGameObject::GetMessageSocket(receiver_instance->m_Collection);

            dmMessage::URL sender;
            sender.m_Socket = dmGameObject::GetMessageSocket(sender_instance->m_Collection);

            if (!dmMessage::IsSocketValid(receiver.m_Socket) || !dmMessage::IsSocketValid(sender.m_Socket))
            {
                dmLogError("post_message: sockets are not valid");
                return false;
            }

            sender.m_Path = dmGameObject::GetIdentifier(sender_instance);
            dmGameObject::Result go_result = dmGameObject::GetComponentId(sender_instance, sender_component_index, &sender.m_Fragment);
            if (go_result != dmGameObject::RESULT_OK)
            {
                dmLogError("post_message: could not get component id");
                return false;
            }

            receiver.m_Path = dmGameObject::GetIdentifier(receiver_instance);
            receiver.m_Fragment = receiver_path->m_Fragment;

            ::Type* type = reflect_get_any_type(message);
            void* ptr = (void*) reflect_get_any_value(message);

            const dmDDF::Descriptor* descriptor = dmDDF::GetDescriptorFromSolType(type);
            if (descriptor)
            {
                // Store & load trick.
                uint8_t tmp_buf[8*1024];
                dmArray<uint8_t> serialized(tmp_buf, 0, sizeof(tmp_buf));
                dmDDF::FixupRepeatedFieldCounts(descriptor, ptr);
                if (dmDDF::RESULT_OK != dmDDF::SaveMessageToArray(ptr, descriptor, serialized))
                {
                    dmLogError("post_message: failed to convert message, too big?");
                    return false;
                }

                void* output = 0;
                uint32_t size = 0;
                if (dmDDF::RESULT_OK != dmDDF::LoadMessage(serialized.Begin(), serialized.Size(), descriptor, &output, 0, &size))
                {
                    dmLogError("post_message: reload failed");
                    return false;
                }

                // Rebase into local pointers
                dmDDF::RebaseMessagePointers(descriptor, output, 0, output);
                dmMessage::Result result = dmMessage::Post(&sender, &receiver, descriptor->m_NameHash, 0, (uintptr_t)descriptor, output, size);
                dmDDF::FreeMessage(output);
                return result = dmMessage::RESULT_OK;
            }
            else
            {
                dmLogError("I do not know how to send a message of this type!");
                return false;
            }
        }
    }
}
