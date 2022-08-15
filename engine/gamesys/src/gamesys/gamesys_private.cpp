// Copyright 2020-2022 The Defold Foundation
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

            out_desc.m_ValueType = dmGameObject::PROP_VALUE_ARRAY;

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
            int32_t location = dmRender::GetMaterialConstantLocation(material, constant_id);
            if (location >= 0)
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

}
