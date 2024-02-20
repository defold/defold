// Copyright 2020-2024 The Defold Foundation
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

#include <stdarg.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include "gamesys_private.h"
#include "components/comp_private.h"
#include <dmsdk/gamesys/render_constants.h>

namespace dmGameSystem
{
    using namespace dmVMath;

    void LogMessageError(dmMessage::Message* message, const char* format, ...)
    {
        va_list lst;
        va_start(lst, format);

        char buf[512];

        int n = vsnprintf(buf, sizeof(buf), format, lst);

        if (n < (int) sizeof(buf))
        {
            const char* id_str = dmHashReverseSafe64(message->m_Id);

            const dmMessage::URL* sender = &message->m_Sender;
            const char* socket_name_sender = dmMessage::GetSocketName(sender->m_Socket);
            const char* path_name_sender = dmHashReverseSafe64(sender->m_Path);
            const char* fragment_name_sender = dmHashReverseSafe64(sender->m_Fragment);

            const dmMessage::URL* receiver = &message->m_Receiver;
            const char* socket_name_receiver = dmMessage::GetSocketName(receiver->m_Socket);
            const char* path_name_receiver = dmHashReverseSafe64(receiver->m_Path);
            const char* fragment_name_receiver = dmHashReverseSafe64(receiver->m_Fragment);

            n+= dmSnPrintf(buf + n, sizeof(buf) - n, " Message '%s' sent from %s:%s#%s to %s:%s#%s.",
                            id_str,
                            socket_name_sender, path_name_sender, fragment_name_sender,
                            socket_name_receiver, path_name_receiver, fragment_name_receiver);
        }

        if (n >= (int) sizeof(buf) - 1) {
            dmLogError("Buffer underflow when formatting message-error (LogMessageError)");
        }

        dmLogError("%s", buf);

        va_end(lst);
    }

    void ShowFullBufferError(const char* object_name, int max_count)
    {
        dmLogError("%s could not be created since the buffer is full (%d). This value cannot be changed", object_name, max_count);
    }

    void ShowFullBufferError(const char* object_name, const char* config_key, int max_count)
    {
        dmLogError("%s could not be created since the buffer is full (%d). Increase the '%s' value in [game.project](defold://open?path=/game.project)",
            object_name, max_count, config_key);
    }

    dmRender::RenderResourceType ResourcePathToRenderResourceType(const char* path)
    {
        const char* path_ext = dmResource::GetExtFromPath(path);
        if (strcmp(path_ext, ".materialc") == 0)
        {
            return dmRender::RENDER_RESOURCE_TYPE_MATERIAL;
        }
        else if (strcmp(path_ext, ".render_targetc") == 0)
        {
            return dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET;
        }
        return dmRender::RENDER_RESOURCE_TYPE_INVALID;
    }

    dmGameObject::PropertyResult GetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, int32_t value_index, dmGameObject::PropertyDesc& out_desc,
                                                        bool use_value_ptr, CompGetConstantCallback callback, void* callback_user_data)
    {
        dmhash_t constant_id = 0;
        dmhash_t* element_ids = 0x0;
        uint32_t element_index = ~0u;
        uint16_t constant_array_size = 0;
        bool result = dmRender::GetMaterialProgramConstantInfo(material, name_hash, &constant_id, &element_ids, &element_index, &constant_array_size);
        if (result)
        {
            uint32_t num_values;
            Vector4* value = 0x0;
            dmRender::HConstant comp_constant;
            bool is_matrix4_type;

            if (callback(callback_user_data, constant_id, &comp_constant))
            {
                value = dmRender::GetConstantValues(comp_constant, &num_values);

                is_matrix4_type = dmRender::GetConstantType(comp_constant) == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4;
                if (is_matrix4_type)
                {
                    value_index *= 4;
                }

                if (value_index >= num_values)
                {
                    return dmGameObject::PROPERTY_RESULT_INVALID_INDEX;
                }

                value = &value[value_index];
            }

            out_desc.m_ValueType   = dmGameObject::PROP_VALUE_ARRAY;
            out_desc.m_ArrayLength = constant_array_size;

            if (constant_id == name_hash)
            {
                if (element_ids != 0x0)
                {
                    out_desc.m_ElementIds[0] = element_ids[0];
                    out_desc.m_ElementIds[1] = element_ids[1];
                    out_desc.m_ElementIds[2] = element_ids[2];
                    out_desc.m_ElementIds[3] = element_ids[3];
                }

                if (value != 0x0)
                {
                    if (is_matrix4_type)
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(*((dmVMath::Matrix4*)value));
                    }
                    else
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(*value);
                    }

                    if (use_value_ptr)
                    {
                        // TODO: Make this more robust. If the constant is e.g. animated (which might get the pointer)
                        // and then the memory is reallocated (E.g. constant value array grew due to newly set values),
                        // then crashes could occur. Or what if it's animated, and then we reset the constant?
                        out_desc.m_ValuePtr = (float*)value;
                    }
                }
                else
                {
                    // The value wasn't found in the component's overridden constants
                    // so we use the material's default values for the constant
                    dmRender::HConstant constant;
                    dmRender::GetMaterialProgramConstant(material, constant_id, constant);

                    is_matrix4_type = dmRender::GetConstantType(constant) == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4;
                    value = dmRender::GetConstantValues(constant, &num_values);

                    if (is_matrix4_type)
                    {
                        value_index *= 4;
                    }

                    if (value_index >= num_values)
                    {
                        return dmGameObject::PROPERTY_RESULT_INVALID_INDEX;
                    }

                    if (is_matrix4_type)
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(*((dmVMath::Matrix4*) &value[value_index]));
                    }
                    else
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(value[value_index]);
                    }
                }
            }
            else
            {
                if (value != 0x0)
                {
                    float* val = ((float*)value) + element_index;
                    out_desc.m_Variant = dmGameObject::PropertyVar(*val);
                    if (use_value_ptr)
                    {
                        // TODO: Make this more robust. If the constant is e.g. animated (which might get the pointer)
                        // and then the memory is reallocated (E.g. constant value array grew due to newly set values),
                        // then crashes could occur. Or what if it's animated, and then we reset the constant?
                        out_desc.m_ValuePtr = val;
                    }
                }
                else
                {
                    dmRender::HConstant constant;
                    dmRender::GetMaterialProgramConstant(material, constant_id, constant);
                    dmVMath::Vector4* material_values = dmRender::GetConstantValues(constant, &num_values);
                    if (value_index >= num_values)
                    {
                        return dmGameObject::PROPERTY_RESULT_INVALID_INDEX;
                    }

                    float v = material_values[value_index].getElem(element_index);
                    out_desc.m_Variant = dmGameObject::PropertyVar(v);
                }
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult SetMaterialConstant(dmRender::HMaterial material, dmhash_t name_hash, const dmGameObject::PropertyVar& var, int32_t value_index, CompSetConstantCallback callback, void* callback_user_data)
    {
        dmhash_t constant_id = 0;
        dmhash_t* element_ids = 0x0;
        uint32_t element_index = ~0u;
        uint16_t num_components = 0;
        bool result = dmRender::GetMaterialProgramConstantInfo(material, name_hash, &constant_id, &element_ids, &element_index, &num_components);
        if (result)
        {
            if (dmRender::GetMaterialConstantLocation(material, constant_id) != dmGraphics::INVALID_UNIFORM_LOCATION)
            {
                if (constant_id == name_hash)
                {
                    if (var.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR4 &&
                        var.m_Type != dmGameObject::PROPERTY_TYPE_QUAT &&
                        var.m_Type != dmGameObject::PROPERTY_TYPE_MATRIX4)
                    {
                        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
                    }
                    callback(callback_user_data, constant_id, value_index, 0x0, var);
                }
                else
                {
                    if (var.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                    {
                        return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
                    }
                    callback(callback_user_data, constant_id, value_index, &element_index, var);
                }
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    void InitializeMaterialAttributeInfos(dmArray<DynamicAttributeInfo>& dynamic_attribute_infos, dmArray<uint16_t>& dynamic_attribute_free_indices, uint32_t initial_capacity)
    {
        dynamic_attribute_infos.SetCapacity(initial_capacity);
        dynamic_attribute_infos.SetSize(initial_capacity);
        dynamic_attribute_free_indices.SetCapacity(initial_capacity);
        for (int i = 0; i < initial_capacity; ++i)
        {
            dynamic_attribute_free_indices.Push(initial_capacity - 1 - i);
        }
    }

    void DestroyMaterialAttributeInfos(dmArray<DynamicAttributeInfo>& dynamic_attribute_infos)
    {
        for (int i = 0; i < dynamic_attribute_infos.Size(); ++i)
        {
            if (dynamic_attribute_infos[i].m_Infos)
            {
                free(dynamic_attribute_infos[i].m_Infos);
            }
        }
    }

    int32_t FindMaterialAttributeIndex(DynamicAttributeInfo info, dmhash_t name_hash)
    {
        for (int i = 0; i < info.m_NumInfos; ++i)
        {
            if (info.m_Infos[i].m_NameHash == name_hash)
                return i;
        }
        return -1;
    }

    dmGameObject::PropertyResult ClearMaterialAttribute(dmArray<DynamicAttributeInfo>& dynamic_attribute_infos, dmArray<uint16_t>& dynamic_attribute_free_indices, uint16_t dynamic_attribute_index, dmhash_t name_hash)
    {
        if (dynamic_attribute_index != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            DynamicAttributeInfo& dynamic_info = dynamic_attribute_infos[dynamic_attribute_index];

            int32_t existing_index = FindMaterialAttributeIndex(dynamic_info, name_hash);
            if (existing_index >= 0)
            {
                if (dynamic_info.m_NumInfos == 1)
                {
                    free(dynamic_info.m_Infos);
                    dynamic_attribute_free_indices.Push(dynamic_attribute_index);
                }
                else
                {
                    // Swap out the entry with the last item in the list
                    DynamicAttributeInfo::Info tmp_info             = dynamic_info.m_Infos[existing_index];
                    dynamic_info.m_Infos[existing_index]            = dynamic_info.m_Infos[dynamic_info.m_NumInfos-1];
                    dynamic_info.m_Infos[dynamic_info.m_NumInfos-1] = tmp_info;
                    dynamic_info.m_NumInfos--;
                }
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult GetMaterialAttribute(
        dmArray<DynamicAttributeInfo>& dynamic_attribute_infos,
        dmArray<uint16_t>&             dynamic_attribute_free_indices,
        uint16_t*                      dynamic_attribute_index,
        dmRender::HMaterial material, dmhash_t name_hash, dmGameObject::PropertyDesc& out_desc)
    {
        const dmGraphics::VertexAttribute* material_attribute;
        const uint8_t* value_ptr;
        uint32_t num_values;
        dmhash_t* element_ids = 0x0;
        uint32_t element_index = ~0u;
        dmhash_t attribute_id = 0;

        if (!GetMaterialProgramAttributeInfo(material, name_hash, &attribute_id, &element_ids, &element_index, &material_attribute, &value_ptr, &num_values))
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        if (element_ids != 0x0)
        {
            out_desc.m_ElementIds[0] = element_ids[0];
            out_desc.m_ElementIds[1] = element_ids[1];
            out_desc.m_ElementIds[2] = element_ids[2];
            out_desc.m_ElementIds[3] = element_ids[3];
        }

        if (*dynamic_attribute_index != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            DynamicAttributeInfo& dynamic_info = dynamic_attribute_infos[*dynamic_attribute_index];

            int32_t dynamic_info_index = FindMaterialAttributeIndex(dynamic_info, name_hash);

            if (dynamic_info_index >= 0)
            {
                if (attribute_id != name_hash)
                {
                    out_desc.m_Variant = dmGameObject::PropertyVar((float) dynamic_info.m_Infos[dynamic_info_index].m_Value.getElem(element_index));
                }
                else
                {
                    out_desc.m_Variant = dmGameObject::PropertyVar(dynamic_info.m_Infos[dynamic_info_index].m_Value);
                }
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        else
        {
            float* f_ptr = (float*) value_ptr;

            if (attribute_id != name_hash)
            {
                out_desc.m_Variant = dmGameObject::PropertyVar(f_ptr[element_index]);
            }
            else
            {
                switch(material_attribute->m_ElementCount)
                {
                    case 1:
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(f_ptr[0]);
                    } break;
                    case 2:
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(dmVMath::Vector3(f_ptr[0], f_ptr[1], 0.0f));
                    } break;
                    case 3:
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(dmVMath::Vector3(f_ptr[0], f_ptr[1], f_ptr[2]));
                    } break;
                    case 4:
                    {
                        out_desc.m_Variant = dmGameObject::PropertyVar(dmVMath::Vector4(f_ptr[0], f_ptr[1], f_ptr[2], f_ptr[3]));
                    } break;
                }
            }
        }
        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetMaterialAttribute(
        dmArray<DynamicAttributeInfo>& dynamic_attribute_infos,
        dmArray<uint16_t>&             dynamic_attribute_free_indices,
        uint16_t*                      dynamic_attribute_index,
        dmRender::HMaterial material, dmhash_t name_hash, const dmGameObject::PropertyVar& var)
    {
        const dmGraphics::VertexAttribute* material_attribute;
        const uint8_t* value_ptr;
        uint32_t num_values;
        dmhash_t* element_ids = 0x0;
        uint32_t element_index = ~0u;
        dmhash_t attribute_id = 0;

        if (!GetMaterialProgramAttributeInfo(material, name_hash, &attribute_id, &element_ids, &element_index, &material_attribute, &value_ptr, &num_values))
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        if (*dynamic_attribute_index == INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            // Find new indices to put on the list
            if (dynamic_attribute_free_indices.Empty())
            {
                const uint32_t current_count = dynamic_attribute_infos.Size();
                const uint32_t new_capacity = dmMath::Min(current_count + 32, (uint32_t) INVALID_DYNAMIC_ATTRIBUTE_INDEX);

                if (new_capacity >= INVALID_DYNAMIC_ATTRIBUTE_INDEX)
                {
                    dmLogError("Unable to allocate dynamic attributes, max dynamic attribute limit reached for sprites (%d)", INVALID_DYNAMIC_ATTRIBUTE_INDEX);
                    return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                }
                for (int i = new_capacity - 1; i >= current_count; i--)
                {
                    dynamic_attribute_free_indices.Push(i);
                }

                dynamic_attribute_infos.SetCapacity(new_capacity);
                dynamic_attribute_infos.SetSize(dynamic_attribute_infos.Capacity());
            }

            // Grab a free index from the list
            *dynamic_attribute_index = dynamic_attribute_free_indices.Back();
            dynamic_attribute_free_indices.Pop();

            DynamicAttributeInfo& dynamic_info = dynamic_attribute_infos[*dynamic_attribute_index];
            assert(dynamic_info.m_Infos == 0);

            dynamic_info.m_Infos               = (DynamicAttributeInfo::Info*) malloc(sizeof(DynamicAttributeInfo::Info));
            dynamic_info.m_NumInfos            = 1;
            dynamic_info.m_Infos[0].m_NameHash = attribute_id;

            if (attribute_id != name_hash)
            {
                float* f_ptr = (float*) &dynamic_info.m_Infos[0].m_Value;
                f_ptr[element_index] = var.m_Number;
            }
            else
            {
                memcpy(&dynamic_info.m_Infos[0].m_Value, &var.m_V4, sizeof(var.m_V4));
            }
        }
        else
        {
            DynamicAttributeInfo& dynamic_info = dynamic_attribute_infos[*dynamic_attribute_index];
            int32_t attribute_index = FindMaterialAttributeIndex(dynamic_info, attribute_id);

            if (attribute_index < 0)
            {
                dynamic_info.m_NumInfos++;
                dynamic_info.m_Infos = (DynamicAttributeInfo::Info*) realloc(dynamic_info.m_Infos, sizeof(DynamicAttributeInfo::Info) * dynamic_info.m_NumInfos);
                dynamic_info.m_Infos[dynamic_info.m_NumInfos-1].m_NameHash = attribute_id;
                attribute_index = dynamic_info.m_NumInfos-1;
            }

            if (attribute_id != name_hash)
            {
                float* f_ptr = (float*) &dynamic_info.m_Infos[attribute_index].m_Value;
                f_ptr[element_index] = var.m_Number;
            }
            else
            {
                memcpy(&dynamic_info.m_Infos[attribute_index].m_Value, &var.m_V4, sizeof(var.m_V4));
            }
        }
        return dmGameObject::PROPERTY_RESULT_OK;
    }
}
