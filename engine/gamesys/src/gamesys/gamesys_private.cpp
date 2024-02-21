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

    int32_t FindAttributeIndex(const dmGraphics::VertexAttribute* attributes, uint32_t attributes_count, dmhash_t name_hash)
    {
        for (int i = 0; i < attributes_count; ++i)
        {
            if (attributes[i].m_NameHash == name_hash)
                return i;
        }
        return -1;
    }

    int32_t FindMaterialAttributeIndex(const DynamicAttributeInfo& info, dmhash_t name_hash)
    {
        for (int i = 0; i < info.m_NumInfos; ++i)
        {
            if (info.m_Infos[i].m_NameHash == name_hash)
                return i;
        }
        return -1;
    }

    void GetMaterialAttributeValues(const DynamicAttributeInfo& info, uint16_t dynamic_attribute_index, uint32_t max_value_size, const uint8_t** value_ptr, uint32_t* value_size)
    {
        *value_ptr  = (uint8_t*) info.m_Infos[dynamic_attribute_index].m_Values;
        *value_size = dmMath::Min(max_value_size, (uint32_t) sizeof(info.m_Infos[dynamic_attribute_index].m_Values));
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

    dmGameObject::PropertyResult ClearMaterialAttribute(
        dmArray<DynamicAttributeInfo>& dynamic_attribute_infos,
        dmArray<uint16_t>&             dynamic_attribute_free_indices,
        uint16_t                       dynamic_attribute_index,
        dmhash_t                       name_hash)
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
                    dynamic_info.m_Infos = 0x0;

                    // We might have filled up the free list already, so in this case we have options:
                    // 1. create more space in the index list
                    // 2. scan the list of entries for free items when a new dynamic property is created (in SetMaterialAttribute)
                    //
                    // Currently we are doing 1) and trimming the index list down to DYNAMIC_ATTRIBUTE_INCREASE_COUNT
                    // in SetMaterialAttribute when the index list is full.
                    if (dynamic_attribute_free_indices.Full())
                    {
                        dynamic_attribute_free_indices.OffsetCapacity(DYNAMIC_ATTRIBUTE_INCREASE_COUNT);
                    }
                    dynamic_attribute_free_indices.Push(dynamic_attribute_index);
                }
                else
                {
                    // Swap out the deleted entry with the last item in the list
                    // The property memory area will NOT be trimmed down
                    DynamicAttributeInfo::Info tmp_info             = dynamic_info.m_Infos[existing_index];
                    dynamic_info.m_Infos[existing_index]            = dynamic_info.m_Infos[dynamic_info.m_NumInfos-1];
                    dynamic_info.m_Infos[dynamic_info.m_NumInfos-1] = tmp_info;
                }
                dynamic_info.m_NumInfos--;
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    static inline float VertexAttributeDataTypeToFloat(const dmGraphics::VertexAttribute::DataType data_type, const uint8_t* value_ptr)
    {
        switch (data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:           return (float) ((int8_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:  return (float) value_ptr[0];
            case dmGraphics::VertexAttribute::TYPE_SHORT:          return (float) ((int16_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT: return (float) ((uint16_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_INT:            return (float) ((int32_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:   return (float) ((uint32_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_FLOAT:          return (float) ((float*) value_ptr)[0];
        }
        return 0;
    }

    static void VertexAttributeToFloats(const dmGraphics::VertexAttribute* attribute, const uint8_t* value_ptr, float* out)
    {
        dmGraphics::Type graphics_type = dmGraphics::GetGraphicsType(attribute->m_DataType);
        uint32_t bytes_per_element     = dmGraphics::GetTypeSize(graphics_type);
        for (int i = 0; i < attribute->m_ElementCount; ++i)
        {
            out[i] = VertexAttributeDataTypeToFloat(attribute->m_DataType, value_ptr + bytes_per_element * i);
        }
    }

    static dmGameObject::PropertyVar DynamicAttributeValuesToPropertyVar(float* values, uint32_t element_count, uint32_t element_index, bool use_element_index)
    {
        if (use_element_index)
        {
            return dmGameObject::PropertyVar(values[element_index]);
        }
        else
        {
            switch(element_count)
            {
                case 1: return dmGameObject::PropertyVar(values[0]);
                case 2: return dmGameObject::PropertyVar(dmVMath::Vector3(values[0], values[1], 0.0f));
                case 3: return dmGameObject::PropertyVar(dmVMath::Vector3(values[0], values[1], values[2]));
                case 4: return dmGameObject::PropertyVar(dmVMath::Vector4(values[0], values[1], values[2], values[3]));
            }
        }

        return dmGameObject::PropertyVar(false);
    }

    dmGameObject::PropertyResult GetMaterialAttribute(
        dmArray<DynamicAttributeInfo>&   dynamic_attribute_infos,
        dmArray<uint16_t>&               dynamic_attribute_free_indices,
        uint16_t                         dynamic_attribute_index,
        dmRender::HMaterial              material,
        dmhash_t                         name_hash,
        dmGameObject::PropertyDesc&      out_desc,
        CompGetMaterialAttributeCallback callback,
        void*                            callback_user_data)
    {
        dmRender::MaterialProgramAttributeInfo info;
        if (!dmRender::GetMaterialProgramAttributeInfo(material, name_hash, info))
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        // If we have a dynamic attribute set, we return that data
        if (dynamic_attribute_index != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            DynamicAttributeInfo& dynamic_info = dynamic_attribute_infos[dynamic_attribute_index];

            int32_t dynamic_info_index = FindMaterialAttributeIndex(dynamic_info, name_hash);

            if (dynamic_info_index >= 0)
            {
                out_desc.m_Variant = DynamicAttributeValuesToPropertyVar(dynamic_info.m_Infos[dynamic_info_index].m_Values, info.m_Attribute->m_ElementCount, info.m_ElementIndex, info.m_AttributeNameHash != name_hash);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        // Otherwise, we try to get it from the component itself via the callabck.
        // We need a callback mechanism here because we don't have a shared data layout for
        // the override attributes on component level (TODO: we should have).
        else
        {
            const dmGraphics::VertexAttribute* comp_attribute;

            // If this callback returns false, e.g a component resource might not have a value override for the attribute,
            // we fallback to the material attribute data instead
            if (callback(callback_user_data, info.m_AttributeNameHash, &comp_attribute))
            {
                uint32_t value_byte_size;
                dmGraphics::GetAttributeValues(*comp_attribute, &info.m_ValuePtr, &value_byte_size);
            }

            float values[4];
            VertexAttributeToFloats(info.m_Attribute, info.m_ValuePtr, values);
            out_desc.m_Variant = DynamicAttributeValuesToPropertyVar(values, info.m_Attribute->m_ElementCount, info.m_ElementIndex, info.m_AttributeNameHash != name_hash);
        }

        out_desc.m_ElementIds[0] = info.m_ElementIds[0];
        out_desc.m_ElementIds[1] = info.m_ElementIds[1];
        out_desc.m_ElementIds[2] = info.m_ElementIds[2];
        out_desc.m_ElementIds[3] = info.m_ElementIds[3];

        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetMaterialAttribute(
        dmArray<DynamicAttributeInfo>&   dynamic_attribute_infos,
        dmArray<uint16_t>&               dynamic_attribute_free_indices,
        uint16_t*                        dynamic_attribute_index,
        dmRender::HMaterial              material,
        dmhash_t                         name_hash,
        const dmGameObject::PropertyVar& var)
    {
        dmRender::MaterialProgramAttributeInfo info;
        if (!dmRender::GetMaterialProgramAttributeInfo(material, name_hash, info))
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        DynamicAttributeInfo* dynamic_info = 0;
        int32_t attribute_index = -1;

        // The attribute doesn't exist in the dynamic attribute table
        if (*dynamic_attribute_index == INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            // No free slots available, so we allocate more slots
            if (dynamic_attribute_free_indices.Empty())
            {
                const uint32_t current_count = dynamic_attribute_infos.Size();
                const uint32_t new_capacity = dmMath::Min(current_count + DYNAMIC_ATTRIBUTE_INCREASE_COUNT, (uint32_t) INVALID_DYNAMIC_ATTRIBUTE_INDEX);

                if (new_capacity >= INVALID_DYNAMIC_ATTRIBUTE_INDEX)
                {
                    dmLogError("Unable to allocate dynamic attributes, max dynamic attribute limit reached for sprites (%d)", INVALID_DYNAMIC_ATTRIBUTE_INDEX);
                    return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                }

                // Put all the new indices on the free list and trim the indices list down so we don't waste too much memory
                dynamic_attribute_free_indices.SetCapacity(DYNAMIC_ATTRIBUTE_INCREASE_COUNT);
                for (int i = new_capacity - 1; i >= current_count; i--)
                {
                    dynamic_attribute_free_indices.Push(i);
                }

                uint32_t fill_start = dynamic_attribute_infos.Capacity();
                dynamic_attribute_infos.SetCapacity(new_capacity);
                dynamic_attribute_infos.SetSize(dynamic_attribute_infos.Capacity());
                memset(&dynamic_attribute_infos[fill_start], 0, DYNAMIC_ATTRIBUTE_INCREASE_COUNT * sizeof(DynamicAttributeInfo));
            }

            // Grab a free index from the list
            *dynamic_attribute_index = dynamic_attribute_free_indices.Back();
            dynamic_attribute_free_indices.Pop();

            dynamic_info = &dynamic_attribute_infos[*dynamic_attribute_index];
            assert(dynamic_info->m_Infos == 0);

            dynamic_info->m_NumInfos            = 1;
            dynamic_info->m_Infos               = (DynamicAttributeInfo::Info*) malloc(sizeof(DynamicAttributeInfo::Info));
            dynamic_info->m_Infos[0].m_NameHash = info.m_AttributeNameHash;
            attribute_index = 0;
            memset(&dynamic_info->m_Infos[attribute_index].m_Values, 0, sizeof(dynamic_info->m_Infos[attribute_index].m_Values));
        }
        else
        {
            dynamic_info = &dynamic_attribute_infos[*dynamic_attribute_index];
            attribute_index = FindMaterialAttributeIndex(*dynamic_info, info.m_AttributeNameHash);

            // The attribute doesn't exist in the dynamic info table (i.e the list of overridden dynamic attributes)
            if (attribute_index < 0)
            {
                attribute_index = dynamic_info->m_NumInfos;
                dynamic_info->m_NumInfos++;
                dynamic_info->m_Infos = (DynamicAttributeInfo::Info*) realloc(dynamic_info->m_Infos, sizeof(DynamicAttributeInfo::Info) * dynamic_info->m_NumInfos);
                dynamic_info->m_Infos[attribute_index].m_NameHash = info.m_AttributeNameHash;
                memset(&dynamic_info->m_Infos[attribute_index].m_Values, 0, sizeof(dynamic_info->m_Infos[attribute_index].m_Values));
            }
        }

        // The values for the dynamic attribute are always represented by float values,
        // so we have compatability between properties and attributes without data conversion
        float* values = dynamic_info->m_Infos[attribute_index].m_Values;
        if (info.m_AttributeNameHash != name_hash)
        {
            values[info.m_ElementIndex] = var.m_Number;
        }
        else
        {
            memcpy(values, var.m_V4, sizeof(var.m_V4));
        }

        return dmGameObject::PROPERTY_RESULT_OK;
    }
}
