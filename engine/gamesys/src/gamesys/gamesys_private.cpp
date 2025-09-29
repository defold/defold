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

        if (message != 0 && (n < (int) sizeof(buf)))
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
        if (strcmp(path_ext, "materialc") == 0)
        {
            return dmRender::RENDER_RESOURCE_TYPE_MATERIAL;
        }
        else if (strcmp(path_ext, "render_targetc") == 0)
        {
            return dmRender::RENDER_RESOURCE_TYPE_RENDER_TARGET;
        }
        else if (strcmp(path_ext, "computec") == 0)
        {
            return dmRender::RENDER_RESOURCE_TYPE_COMPUTE;
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

    // Prepares the list of attributes that could potentially overrides an already specified material attribute
    void FillAttributeInfos(DynamicAttributePool* dynamic_attribute_pool, uint16_t component_dynamic_attribute_index, const dmGraphics::VertexAttribute* component_attributes, uint32_t num_component_attributes, dmGraphics::VertexAttributeInfos* material_infos, dmGraphics::VertexAttributeInfos* component_infos)
    {
        component_infos->m_NumInfos     = material_infos->m_NumInfos;
        component_infos->m_VertexStride = material_infos->m_VertexStride;

        for (int i = 0; i < material_infos->m_NumInfos; ++i)
        {
            dmhash_t name_hash = material_infos->m_Infos[i].m_NameHash;

            component_infos->m_Infos[i] = material_infos->m_Infos[i];

            // 1. Fill from dynamic attributes first
            if (dynamic_attribute_pool != 0x0 && component_dynamic_attribute_index != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
            {
                const DynamicAttributeInfo& dynamic_info = dynamic_attribute_pool->Get(component_dynamic_attribute_index);
                int32_t dynamic_attribute_index = FindMaterialAttributeIndex(dynamic_info, name_hash);
                if (dynamic_attribute_index >= 0)
                {
                    component_infos->m_Infos[i].m_ValuePtr        = (uint8_t*) &dynamic_info.m_Infos[dynamic_attribute_index].m_Values;
                    component_infos->m_Infos[i].m_ValueVectorType = dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4;
                    component_infos->m_Infos[i].m_DataType        = dmGraphics::VertexAttribute::TYPE_FLOAT;
                    continue;
                }
            }

            // 2. If that failed, we try to fill from the resource attribute
            int component_attribute_index = FindAttributeIndex(component_attributes, num_component_attributes, name_hash);
            if (component_attribute_index >= 0)
            {
                // We don't use the byte size from the overridden attribute here, since we still need to match
                // the stride of the materials vertex declaration
                uint32_t value_byte_size_tmp;
                dmGraphics::GetAttributeValues(component_attributes[component_attribute_index],
                    &component_infos->m_Infos[i].m_ValuePtr, &value_byte_size_tmp);
            }

            // 3. If all of the above failed, we fallback to using the material attribute instead
        }
    }

    // Prepares the list of material attributes by getting all the vertex attributes and values for each attribute
    // as specified in the material
    void FillMaterialAttributeInfos(dmRender::HMaterial material, dmGraphics::HVertexDeclaration vx_decl, dmGraphics::VertexAttributeInfos* infos, dmGraphics::CoordinateSpace default_coordinate_space)
    {
        const dmGraphics::VertexAttribute* material_attributes;
        uint32_t material_attributes_count;
        dmRender::GetMaterialProgramAttributes(material, &material_attributes, &material_attributes_count);
        infos->m_NumInfos     = dmMath::Min(material_attributes_count, (uint32_t) dmGraphics::MAX_VERTEX_STREAM_COUNT);
        infos->m_VertexStride = dmGraphics::GetVertexDeclarationStride(vx_decl);

        uint32_t value_byte_size;
        for (int i = 0; i < infos->m_NumInfos; ++i)
        {
            dmGraphics::VertexAttributeInfo& info = infos->m_Infos[i];
            info.m_NameHash         = material_attributes[i].m_NameHash;
            info.m_SemanticType     = material_attributes[i].m_SemanticType;
            info.m_DataType         = material_attributes[i].m_DataType;
            info.m_CoordinateSpace  = material_attributes[i].m_CoordinateSpace;
            info.m_VectorType       = material_attributes[i].m_VectorType;
            info.m_Normalize        = material_attributes[i].m_Normalize;
            info.m_StepFunction     = material_attributes[i].m_StepFunction;
            info.m_ValueVectorType  = material_attributes[i].m_VectorType;

            if (info.m_CoordinateSpace == dmGraphics::COORDINATE_SPACE_DEFAULT)
            {
                info.m_CoordinateSpace = default_coordinate_space;
            }

            dmRender::GetMaterialProgramAttributeValues(material, i, &info.m_ValuePtr, &value_byte_size);
        }
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

    void ConvertMaterialAttributeValuesToDataType(const DynamicAttributeInfo& info, uint32_t dynamic_attribute_index, const dmGraphics::VertexAttribute* attribute, uint8_t* value_ptr)
    {
        float* values = info.m_Infos[dynamic_attribute_index].m_Values;

        dmGraphics::Type graphics_type = dmGraphics::GetGraphicsType(attribute->m_DataType);
        uint32_t bytes_per_element     = dmGraphics::GetTypeSize(graphics_type);
        uint32_t bytes_to_copy         = dmMath::Min(bytes_per_element * attribute->m_ElementCount, (uint32_t) sizeof(info.m_Infos[dynamic_attribute_index].m_Values));

        if (attribute->m_DataType == dmGraphics::VertexAttribute::TYPE_FLOAT)
        {
            memcpy(value_ptr, values, bytes_to_copy);
        }
        else
        {
            for (int i = 0; i < attribute->m_ElementCount; ++i)
            {
                WriteVertexAttributeFromFloat(value_ptr + bytes_per_element * i, values[i], attribute->m_DataType);
            }
        }
    }

    void InitializeMaterialAttributeInfos(DynamicAttributePool& pool, uint32_t initial_capacity)
    {
        pool.SetCapacity(initial_capacity);
    }

    void DestroyMaterialAttributeInfos(DynamicAttributePool& pool)
    {
        dmArray<DynamicAttributeInfo>& dynamic_attribute_infos = pool.GetRawObjects();

        for (int i = 0; i < dynamic_attribute_infos.Size(); ++i)
        {
            if (dynamic_attribute_infos[i].m_Infos)
            {
                free(dynamic_attribute_infos[i].m_Infos);
                dynamic_attribute_infos[i].m_Infos = 0;
            }
        }
    }

    void FreeMaterialAttribute(DynamicAttributePool& pool, uint16_t dynamic_attribute_index)
    {
        if (dynamic_attribute_index == INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            return;
        }

        DynamicAttributeInfo& dynamic_info = pool.Get(dynamic_attribute_index);
        if (dynamic_info.m_Infos)
        {
            assert(dynamic_info.m_NumInfos > 0);
            free(dynamic_info.m_Infos);
        }

        pool.Free(dynamic_attribute_index, true);
    }

    dmGameObject::PropertyResult ClearMaterialAttribute(
        DynamicAttributePool& pool,
        uint32_t              dynamic_attribute_index,
        dmhash_t              name_hash)
    {
        if (dynamic_attribute_index == INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        DynamicAttributeInfo& dynamic_info = pool.Get(dynamic_attribute_index);
        int32_t existing_index             = FindMaterialAttributeIndex(dynamic_info, name_hash);

        if (existing_index >= 0)
        {
            if (dynamic_info.m_NumInfos == 1)
            {
                free(dynamic_info.m_Infos);
                pool.Free(dynamic_attribute_index, true);
            }
            else
            {
                // Swap out the deleted entry with the last item in the list
                // The property memory area will NOT be trimmed down
                DynamicAttributeInfo::Info tmp_info             = dynamic_info.m_Infos[existing_index];
                dynamic_info.m_Infos[existing_index]            = dynamic_info.m_Infos[dynamic_info.m_NumInfos-1];
                dynamic_info.m_Infos[dynamic_info.m_NumInfos-1] = tmp_info;
                dynamic_info.m_NumInfos--;
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
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
        DynamicAttributePool&            pool,
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

        out_desc.m_ElementIds[0] = info.m_ElementIds[0];
        out_desc.m_ElementIds[1] = info.m_ElementIds[1];
        out_desc.m_ElementIds[2] = info.m_ElementIds[2];
        out_desc.m_ElementIds[3] = info.m_ElementIds[3];

        // If we have a dynamic attribute set, we return that data
        if (dynamic_attribute_index != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            DynamicAttributeInfo& dynamic_info = pool.Get(dynamic_attribute_index);

            int32_t dynamic_info_index = FindMaterialAttributeIndex(dynamic_info, info.m_AttributeNameHash);

            if (dynamic_info_index >= 0)
            {
                out_desc.m_Variant = DynamicAttributeValuesToPropertyVar(dynamic_info.m_Infos[dynamic_info_index].m_Values, info.m_Attribute->m_ElementCount, info.m_ElementIndex, info.m_AttributeNameHash != name_hash);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }
        // Otherwise, we try to get it from the component itself via the callabck.
        // We need a callback mechanism here because we don't have a shared data layout for
        // the override attributes on component level (TODO: we should have).
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

        return dmGameObject::PROPERTY_RESULT_OK;
    }

    dmGameObject::PropertyResult SetMaterialAttribute(
        DynamicAttributePool&            pool,
        uint16_t*                        dynamic_attribute_index,
        dmRender::HMaterial              material,
        dmhash_t                         name_hash,
        const dmGameObject::PropertyVar& var,
        CompGetMaterialAttributeCallback callback,
        void*                            callback_user_data)
    {
        if (var.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER &&
            var.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR3 &&
            var.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR4)
        {
            return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_TYPE;
        }

        dmRender::MaterialProgramAttributeInfo info;
        if (!dmRender::GetMaterialProgramAttributeInfo(material, name_hash, info))
        {
            return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        DynamicAttributeInfo* dynamic_info = 0;
        int32_t attribute_index = -1;
        bool needs_initial_copying = false;

        // The attribute doesn't exist in the dynamic attribute table
        if (*dynamic_attribute_index == INVALID_DYNAMIC_ATTRIBUTE_INDEX)
        {
            // No free slots available, so we allocate more slots

            if (pool.Full())
            {
                const uint32_t current_count = pool.Capacity();
                const uint32_t new_capacity = dmMath::Min(current_count + DYNAMIC_ATTRIBUTE_INCREASE_COUNT, (uint32_t) INVALID_DYNAMIC_ATTRIBUTE_INDEX);

                if (new_capacity >= INVALID_DYNAMIC_ATTRIBUTE_INDEX)
                {
                    dmLogError("Unable to allocate dynamic attributes, max dynamic attribute limit reached (%d).", INVALID_DYNAMIC_ATTRIBUTE_INDEX);
                    return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                }

                pool.SetCapacity(new_capacity);
            }

            DynamicAttributeInfo new_info  = {};
            new_info.m_NumInfos            = 1;
            new_info.m_Infos               = (DynamicAttributeInfo::Info*) malloc(sizeof(DynamicAttributeInfo::Info));
            new_info.m_Infos[0].m_NameHash = info.m_AttributeNameHash;

            attribute_index = 0;
            uint32_t new_index = pool.Alloc();
            pool.Set(new_index, new_info);

            dynamic_info = &pool.Get(new_index);
            *dynamic_attribute_index = (uint16_t)new_index;
            needs_initial_copying = true;
        }
        else
        {
            dynamic_info = &pool.Get(*dynamic_attribute_index);
            attribute_index = FindMaterialAttributeIndex(*dynamic_info, info.m_AttributeNameHash);

            // The attribute doesn't exist in the dynamic info table (i.e the list of overridden dynamic attributes)
            if (attribute_index < 0)
            {
                attribute_index = dynamic_info->m_NumInfos;
                dynamic_info->m_NumInfos++;
                dynamic_info->m_Infos = (DynamicAttributeInfo::Info*) realloc(dynamic_info->m_Infos, sizeof(DynamicAttributeInfo::Info) * dynamic_info->m_NumInfos);
                dynamic_info->m_Infos[attribute_index].m_NameHash = info.m_AttributeNameHash;
                needs_initial_copying = true;
            }
        }

        // The values for the dynamic attribute are always represented by float values,
        // so we have compatability between properties and attributes without data conversion
        float* values = dynamic_info->m_Infos[attribute_index].m_Values;

        // If we have created a new dynamic attribute, we need to copy the data from the material attribute
        if (needs_initial_copying)
        {
            // First, clear out the old data so we have a clean starting point
            memset(values, 0, sizeof(dynamic_info->m_Infos[attribute_index].m_Values));

            const dmGraphics::VertexAttribute* comp_attribute;
            // The component might have a value override for the attribute
            if (callback(callback_user_data, info.m_AttributeNameHash, &comp_attribute))
            {
                uint32_t value_byte_size;
                dmGraphics::GetAttributeValues(*comp_attribute, &info.m_ValuePtr, &value_byte_size);
            }

            // Then, we need to convert each element of the attribute data to a float, since that is the backing storage for the override
            VertexAttributeToFloats(info.m_Attribute, info.m_ValuePtr, values);
        }

        // go.set("#sprite", "attribute_vec.x", 10.0)
        if (info.m_AttributeNameHash != name_hash)
        {
            values[info.m_ElementIndex] = var.m_Number;
        }
        // go.set("#sprite", "attribute_val", 10.0)
        else if (var.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
        {
            values[0] = var.m_Number;
        }
        // go.set("#sprite", "attribute_vec", vmath.vector4(0.0, 1.0, 2.0, 3.0))
        else
        {
            memcpy(values, var.m_V4, sizeof(var.m_V4));
        }

        return dmGameObject::PROPERTY_RESULT_OK;
    }
}
