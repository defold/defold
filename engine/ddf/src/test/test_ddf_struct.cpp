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

#include <jc_test/jc_test.h>

#include "test_ddf.h"

#include "ddf/ddf_struct.h"

static dmStructDDF::Struct::FieldsEntry* FindEntry(dmStructDDF::Struct* message, const char* key)
{
    for (uint32_t i = 0; i < message->m_Fields.m_Count; ++i)
    {
        if (strcmp(message->m_Fields[i].m_Key, key) == 0)
            return &message->m_Fields[i];
    }

    return 0;
}

void TestStructSimple(const char* msg, uint32_t msg_size)
{
    // Load
    dmStructDDF::Struct* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg, msg_size, &dmStructDDF_Struct_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* hello = FindEntry(message, "hello");
    ASSERT_STREQ("world", hello->m_Value->m_Kind.m_StringValue);

    dmStructDDF::Struct::FieldsEntry* number = FindEntry(message, "number");
    ASSERT_NEAR(1337.0, number->m_Value->m_Kind.m_NumberValue, 0.001);

    dmStructDDF::Struct::FieldsEntry* boolean = FindEntry(message, "boolean");
    ASSERT_TRUE(boolean->m_Value->m_Kind.m_BoolValue);

    dmStructDDF::Struct::FieldsEntry* nothing = FindEntry(message, "nothing");
    ASSERT_EQ(dmStructDDF::NULL_VALUE, nothing->m_Value->m_Kind.m_NullValue);

    // Save
    std::string save_str;
    e = DDFSaveToString(message, &dmStructDDF_Struct_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &dmStructDDF_Struct_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* hello2 = FindEntry(saved_message, "hello");
    ASSERT_STREQ("world", hello2->m_Value->m_Kind.m_StringValue);

    dmStructDDF::Struct::FieldsEntry* number2 = FindEntry(saved_message, "number");
    ASSERT_NEAR(1337.0, number2->m_Value->m_Kind.m_NumberValue, 0.001);

    dmStructDDF::Struct::FieldsEntry* boolean2 = FindEntry(saved_message, "boolean");
    ASSERT_TRUE(boolean2->m_Value->m_Kind.m_BoolValue);

    dmStructDDF::Struct::FieldsEntry* nothing2 = FindEntry(saved_message, "nothing");
    ASSERT_EQ(dmStructDDF::NULL_VALUE, nothing2->m_Value->m_Kind.m_NullValue);

    dmDDF::FreeMessage(message);
    dmDDF::FreeMessage(saved_message);
}

void TestStructNested(const char* msg, uint32_t msg_size)
{
    // Load
    dmStructDDF::Struct* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg, msg_size, &dmStructDDF_Struct_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* user = FindEntry(message, "user");

    dmStructDDF::Struct::FieldsEntry* field_id = FindEntry(&user->m_Value->m_Kind.m_StructValue, "id");
    ASSERT_NEAR(123.0, field_id->m_Value->m_Kind.m_NumberValue, 0.001);

    dmStructDDF::Struct::FieldsEntry* field_name = FindEntry(&user->m_Value->m_Kind.m_StructValue, "name");
    ASSERT_STREQ("Mr.X", field_name->m_Value->m_Kind.m_StringValue);

    // Save
    std::string save_str;
    e = DDFSaveToString(message, &dmStructDDF_Struct_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &dmStructDDF_Struct_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* user2 = FindEntry(saved_message, "user");

    dmStructDDF::Struct::FieldsEntry* field_id_2 = FindEntry(&user2->m_Value->m_Kind.m_StructValue, "id");
    ASSERT_NEAR(123.0, field_id_2->m_Value->m_Kind.m_NumberValue, 0.001);

    dmStructDDF::Struct::FieldsEntry* field_name_2 = FindEntry(&user2->m_Value->m_Kind.m_StructValue, "name");
    ASSERT_STREQ("Mr.X", field_name_2->m_Value->m_Kind.m_StringValue);

    dmDDF::FreeMessage(message);
    dmDDF::FreeMessage(saved_message);
}

void TestStructList(const char* msg, uint32_t msg_size)
{
    // Load
    dmStructDDF::Struct* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg, msg_size, &dmStructDDF_Struct_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* values = FindEntry(message, "values");
    dmStructDDF::ListValue* list = values->m_Value->m_Kind.m_ListValue;

    ASSERT_EQ(3, list->m_Values.m_Count);
    ASSERT_NEAR(1.0, list->m_Values[0].m_Kind.m_NumberValue, 0.001);
    ASSERT_STREQ("two", list->m_Values[1].m_Kind.m_StringValue);
    ASSERT_FALSE(list->m_Values[2].m_Kind.m_BoolValue);

    // Save
    std::string save_str;
    e = DDFSaveToString(message, &dmStructDDF_Struct_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &dmStructDDF_Struct_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* values2 = FindEntry(saved_message, "values");
    dmStructDDF::ListValue* list2 = values2->m_Value->m_Kind.m_ListValue;

    ASSERT_EQ(3, list2->m_Values.m_Count);
    ASSERT_NEAR(1.0, list2->m_Values[0].m_Kind.m_NumberValue, 0.001);
    ASSERT_STREQ("two", list2->m_Values[1].m_Kind.m_StringValue);
    ASSERT_FALSE(list2->m_Values[2].m_Kind.m_BoolValue);

    dmDDF::FreeMessage(saved_message);
    dmDDF::FreeMessage(message);
}

void TestStructJSON(const char* msg, uint32_t msg_size)
{
    // Load
    dmStructDDF::Struct* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg, msg_size, &dmStructDDF_Struct_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* name = FindEntry(message, "name");
    ASSERT_STREQ("engine", name->m_Value->m_Kind.m_StringValue);

    dmStructDDF::Struct::FieldsEntry* version = FindEntry(message, "version");
    ASSERT_NEAR(3.0, version->m_Value->m_Kind.m_NumberValue, 0.001);

    dmStructDDF::Struct::FieldsEntry* features = FindEntry(message, "features");
    ASSERT_EQ(2, features->m_Value->m_Kind.m_ListValue->m_Values.m_Count);

    dmStructDDF::Struct::FieldsEntry* config = FindEntry(message, "config");
    ASSERT_TRUE(config->m_Value->m_Kind.m_StructValue.m_Fields[0].m_Value->m_Kind.m_BoolValue);

    dmStructDDF::Struct::FieldsEntry* debug = FindEntry(&config->m_Value->m_Kind.m_StructValue, "debug");
    ASSERT_TRUE(debug->m_Value->m_Kind.m_BoolValue);

    // Save
    std::string save_str;
    e = DDFSaveToString(message, &dmStructDDF_Struct_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &dmStructDDF_Struct_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmStructDDF::Struct::FieldsEntry* name2 = FindEntry(saved_message, "name");
    ASSERT_STREQ("engine", name2->m_Value->m_Kind.m_StringValue);

    dmStructDDF::Struct::FieldsEntry* version2 = FindEntry(saved_message, "version");
    ASSERT_NEAR(3.0, version2->m_Value->m_Kind.m_NumberValue, 0.001);

    dmStructDDF::Struct::FieldsEntry* features2 = FindEntry(saved_message, "features");
    ASSERT_EQ(2, features2->m_Value->m_Kind.m_ListValue->m_Values.m_Count);

    dmStructDDF::Struct::FieldsEntry* config2 = FindEntry(saved_message, "config");
    ASSERT_TRUE(config2->m_Value->m_Kind.m_StructValue.m_Fields[0].m_Value->m_Kind.m_BoolValue);

    dmStructDDF::Struct::FieldsEntry* debug2 = FindEntry(&config2->m_Value->m_Kind.m_StructValue, "debug");
    ASSERT_TRUE(debug2->m_Value->m_Kind.m_BoolValue);

    dmDDF::FreeMessage(saved_message);
    dmDDF::FreeMessage(message);
}
