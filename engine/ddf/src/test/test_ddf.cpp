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

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../ddf/ddf.h"
#include <dlib/memory.h>
#include <dlib/dstrings.h>
#include <dlib/sys.h>
#include <dlib/testutil.h>

/*
 * TODO:
 * Tests
 * - Fields
 *   - Extra fields
 *   - Fields that are missing
 */

#ifndef DDF_EXPOSE_DESCRIPTORS
#define DDF_EXPOSE_DESCRIPTORS
#endif

#include "test/test_ddf_proto.h"
#include "test/test_ddf_proto.pb.h"

enum MyEnum
{
    MYENUM,
};

static bool DDFStringSaveFunction(void* context, const void* buffer, uint32_t buffer_size)
{
    std::string* str = (std::string*) context;
    str->insert(str->size(), (const char*) buffer, buffer_size);
    return true;
}

static dmDDF::Result DDFSaveToString(const void* message, const dmDDF::Descriptor* desc, std::string& str)
{
    return SaveMessage(message, desc, &str, DDFStringSaveFunction);
}

TEST(Misc, TestEnumSize)
{
    ASSERT_EQ(sizeof(uint32_t), sizeof(MyEnum));
}

TEST(Simple, Descriptor)
{
    // Test descriptor
    const dmDDF::Descriptor& d = DUMMY::TestDDF_Simple_DESCRIPTOR;
    ASSERT_STREQ("simple", d.m_Name);
    ASSERT_EQ((uint32_t) 4, d.m_Size);
    ASSERT_EQ((uint32_t) 1, d.m_FieldCount);

    // Test field(s)
    const dmDDF::FieldDescriptor& f1 = d.m_Fields[0];
    ASSERT_STREQ("a", f1.m_Name);
    ASSERT_EQ((uint32_t)1, f1.m_Number);
    ASSERT_EQ((uint32_t) dmDDF::TYPE_INT32, f1.m_Type);
    ASSERT_EQ(0, f1.m_MessageDescriptor);
    ASSERT_EQ((uint32_t)0, f1.m_Offset);
}

TEST(Simple, LoadSave)
{
    int32_t test_values[] = { INT32_MIN, INT32_MAX, 0 };

    for (uint32_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); ++i)
    {
        TestDDF::Simple simple;
        simple.set_a(test_values[i]);
        std::string msg_str = simple.SerializeAsString();

        const char* msg_buf = msg_str.c_str();
        uint32_t msg_buf_size = msg_str.size();
        void* message;

        dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
        ASSERT_EQ(dmDDF::RESULT_OK, e);

        DUMMY::TestDDF::Simple* msg = (DUMMY::TestDDF::Simple*) message;
        ASSERT_EQ(simple.a(), msg->m_A);

        std::string msg_str2;
        e = DDFSaveToString(message, &DUMMY::TestDDF_Simple_DESCRIPTOR, msg_str2);
        ASSERT_EQ(dmDDF::RESULT_OK, e);
        ASSERT_EQ(msg_str, msg_str2);

        dmDDF::FreeMessage(message);
    }
}

TEST(Simple, LoadWithTemplateFunction)
{
    int32_t test_values[] = { INT32_MIN, INT32_MAX, 0 };

    for (uint32_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); ++i)
    {
        TestDDF::Simple simple;
        simple.set_a(test_values[i]);
        std::string msg_str = simple.SerializeAsString();
        const char* msg_buf = msg_str.c_str();
        uint32_t msg_buf_size = msg_str.size();

        DUMMY::TestDDF::Simple* msg;
        dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &msg);
        ASSERT_EQ(dmDDF::RESULT_OK, e);

        ASSERT_EQ(simple.a(), msg->m_A);

        dmDDF::FreeMessage(msg);
    }
}

#if !defined(DM_PLATFORM_IOS)
// TODO: Disabled on iOS
// We have add functionality to located tmp-dir on iOS. See issue #624
TEST(Simple, LoadFromFile)
{
    int32_t test_values[] = { INT32_MIN, INT32_MAX, 0 };

    for (uint32_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); ++i)
    {
        TestDDF::Simple simple;
        simple.set_a(test_values[i]);

        char path[512];
        const char* file_name = dmTestUtil::MakeHostPath(path, sizeof(path), "__TEMPFILE__");
        {
            std::fstream output(file_name,  std::ios::out | std::ios::trunc | std::ios::binary);
            ASSERT_EQ(true, simple.SerializeToOstream(&output));
        }

        void* message;
        dmDDF::Result e = dmDDF::LoadMessageFromFile(file_name, &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
        ASSERT_EQ(dmDDF::RESULT_OK, e);

        dmSys::Unlink(file_name);

        DUMMY::TestDDF::Simple* msg = (DUMMY::TestDDF::Simple*) message;
        ASSERT_EQ(simple.a(), msg->m_A);

        dmDDF::FreeMessage(message);
    }
}
#endif

TEST(Simple, LoadFromFile2)
{
    char path[512];
    const char* file_name = dmTestUtil::MakeHostPath(path, sizeof(path), "DOES_NOT_EXISTS");
    void *message;
    dmDDF::Result e = dmDDF::LoadMessageFromFile(file_name, &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_IO_ERROR, e);
}

TEST(ScalarTypes, Types)
{
    // Test descriptor
    const dmDDF::Descriptor& d = DUMMY::TestDDF_ScalarTypes_DESCRIPTOR;
    ASSERT_STREQ("scalar_types", d.m_Name);
    ASSERT_EQ(8, d.m_FieldCount);

    // Test field(s)
    const char* names[] =
    {
        "float_val",
        "double_val",
        "int32_val",
        "uint32_val",
        "int64_val",
        "uint64_val",
        "string_val",
        "bool_val",
    };

    enum dmDDF::Type types[] =
    {
        dmDDF::TYPE_FLOAT,
        dmDDF::TYPE_DOUBLE,
        dmDDF::TYPE_INT32,
        dmDDF::TYPE_UINT32,
        dmDDF::TYPE_INT64,
        dmDDF::TYPE_UINT64,
        dmDDF::TYPE_STRING,
        dmDDF::TYPE_BOOL,
    };

    for (uint32_t i = 0; i < d.m_FieldCount; ++i)
    {
        const dmDDF::FieldDescriptor* f = &d.m_Fields[i];
        ASSERT_STREQ(names[i], f->m_Name);
        ASSERT_EQ(i+1, f->m_Number);
        ASSERT_EQ((uint32_t) types[i], f->m_Type);
    }
}

TEST(ScalarTypes, Load)
{
    TestDDF::ScalarTypes scalar_types;
    scalar_types.set_float_val(1.0f);
    scalar_types.set_double_val(2.0);
    scalar_types.set_int32_val(INT32_MAX);
    scalar_types.set_uint32_val(UINT32_MAX);
    scalar_types.set_int64_val(INT64_MAX);
    scalar_types.set_uint64_val(UINT64_MAX);
    scalar_types.set_string_val("foo");
    scalar_types.set_bool_val(true);

    std::string msg_str = scalar_types.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_ScalarTypes_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::ScalarTypes* msg = (DUMMY::TestDDF::ScalarTypes*) message;
    ASSERT_EQ(scalar_types.float_val(), msg->m_FloatVal);
    ASSERT_EQ(scalar_types.double_val(), msg->m_DoubleVal);
    ASSERT_EQ(scalar_types.int32_val(), msg->m_Int32Val);
    ASSERT_EQ(scalar_types.uint32_val(), msg->m_Uint32Val);
    ASSERT_EQ(scalar_types.int64_val(), msg->m_Int64Val);
    ASSERT_EQ(scalar_types.uint64_val(), msg->m_Uint64Val);
    ASSERT_STREQ(scalar_types.string_val().c_str(), msg->m_StringVal);
    ASSERT_EQ(scalar_types.bool_val(), msg->m_BoolVal);

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_ScalarTypes_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Enum, Simple)
{
    ASSERT_EQ(10, DUMMY::TestDDF::TEST_ENUM_VAL1);
    ASSERT_EQ(20, DUMMY::TestDDF::TEST_ENUM_VAL2);

    const char* enum_vals[] = {"TEST_ENUM_VAL1", "TEST_ENUM_VAL2"};

    ASSERT_STREQ(enum_vals[0], DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[0].m_Name);
    ASSERT_STREQ(enum_vals[1], DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[1].m_Name);

    ASSERT_EQ(10, DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[0].m_Value);
    ASSERT_EQ(20, DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[1].m_Value);

    ASSERT_STREQ(enum_vals[0], GetEnumName(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, 10));
    ASSERT_STREQ(enum_vals[1], GetEnumName(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, 20));
    ASSERT_EQ(0, GetEnumName(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, -1));

    ASSERT_EQ(10, GetEnumValue(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, enum_vals[0]));
    ASSERT_EQ(20, GetEnumValue(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, enum_vals[1]));
}

TEST(Simple01Repeated, Load)
{
    const int count = 2;

    TestDDF::Simple01Repeated repated;
    for (int i = 0; i < count; ++i)
    {
        TestDDF::Simple01*s = repated.add_array();
        s->set_x(i);
        s->set_y(i+100);
    }

    std::string msg_str = repated.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Simple01Repeated_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Simple01Repeated* msg = (DUMMY::TestDDF::Simple01Repeated*) message;
    ASSERT_EQ((uint32_t) count, msg->m_Array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        ASSERT_EQ(repated.array(i).x(), msg->m_Array.m_Data[i].m_X);
        ASSERT_EQ(repated.array(i).y(), msg->m_Array.m_Data[i].m_Y);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Simple01Repeated_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Simple02Repeated, Load)
{
    const int count = 10;

    TestDDF::Simple02Repeated repated;
    for (int i = 0; i < count; ++i)
    {
        repated.add_array(i * 10);
    }

    std::string msg_str = repated.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Simple02Repeated_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Simple02Repeated* msg = (DUMMY::TestDDF::Simple02Repeated*) message;
    ASSERT_EQ((uint32_t)count, msg->m_Array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        ASSERT_EQ(repated.array(i), msg->m_Array.m_Data[i]);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Simple02Repeated_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);


    dmDDF::FreeMessage(message);
}

TEST(StringRepeated, Load)
{
    const int count = 10;

    TestDDF::StringRepeated repated;
    for (int i = 0; i < count; ++i)
    {
        char tmp[32];
        sprintf(tmp, "%d", i*10);
        repated.add_array(tmp);
    }

    std::string msg_str = repated.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_StringRepeated_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::StringRepeated* msg = (DUMMY::TestDDF::StringRepeated*) message;
    ASSERT_EQ((uint32_t) count, msg->m_Array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        ASSERT_STREQ(repated.array(i).c_str(), msg->m_Array.m_Data[i]);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_StringRepeated_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(NestedMessage, Load)
{

    TestDDF::NestedMessage nested_message;
    TestDDF::NestedMessage::Nested n1;
    TestDDF::NestedMessage::Nested n2;
    n1.set_x(10);
    n2.set_x(20);

    *nested_message.mutable_n1() = n1;
    *nested_message.mutable_n2() = n2;

    std::string msg_str = nested_message.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_NestedMessage_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    DUMMY::TestDDF::NestedMessage* msg = (DUMMY::TestDDF::NestedMessage*) message;

    ASSERT_EQ(n1.x(), msg->m_N1.m_X);
    ASSERT_EQ(n2.x(), msg->m_N2.m_X);

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_NestedMessage_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Mesh, Load)
{
    const int count = 10;

    TestDDF::Mesh mesh;
    for (int i = 0; i < count; ++i)
    {
        mesh.add_vertices((float) i*10 + 1);
        mesh.add_vertices((float) i*10 + 2);
        mesh.add_vertices((float) i*10 + 3);

        mesh.add_indices(i*3 + 0);
        mesh.add_indices(i*3 + 1);
        mesh.add_indices(i*3 + 2);
    }
    mesh.set_primitive_count(count);
    mesh.set_name("MyMesh");
    mesh.set_primitive_type(TestDDF::Mesh_Primitive_TRIANGLES);

    std::string msg_str = mesh.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Mesh_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Mesh* msg = (DUMMY::TestDDF::Mesh*) message;
    ASSERT_EQ((uint32_t) count, msg->m_PrimitiveCount);
    ASSERT_STREQ(mesh.name().c_str(), msg->m_Name);
    ASSERT_EQ((uint32_t) mesh.primitive_type(), (uint32_t) msg->m_PrimitiveType);

    for (int i = 0; i < count * 3; ++i)
    {
        ASSERT_EQ(mesh.vertices(i), msg->m_Vertices.m_Data[i]);
        ASSERT_EQ(mesh.indices(i), msg->m_Indices.m_Data[i]);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Mesh_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(NestedArray, Load)
{
    const int count1 = 2;
    const int count2 = 2;

    TestDDF::NestedArray pb_nested;
    pb_nested.set_d(1);
    pb_nested.set_e(1);

    for (int i = 0; i < count1; ++i)
    {
        TestDDF::NestedArraySub1* sub1 = pb_nested.add_array1();
        sub1->set_b(i*2+0);
        sub1->set_c(i*2+1);
        for (int j = 0; j < count2; ++j)
        {
            TestDDF::NestedArraySub2* sub2 = sub1->add_array2();
            sub2->set_a(j*10+0);
        }
    }

    std::string pb_msg_str = pb_nested.SerializeAsString();
    const char* pb_msg_buf = pb_msg_str.c_str();
    uint32_t pb_msg_buf_size = pb_msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) pb_msg_buf, pb_msg_buf_size, &DUMMY::TestDDF_NestedArray_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    DUMMY::TestDDF::NestedArray* nested = (DUMMY::TestDDF::NestedArray*) message;

    ASSERT_EQ((uint32_t) count1, nested->m_Array1.m_Count);
    ASSERT_EQ(pb_nested.d(), nested->m_D);
    ASSERT_EQ(pb_nested.e(), nested->m_E);

    for (int i = 0; i < count1; ++i)
    {
        ASSERT_EQ((uint32_t) count2, nested->m_Array1.m_Data[i].m_Array2.m_Count);
        ASSERT_EQ(pb_nested.array1(i).b(), nested->m_Array1.m_Data[i].m_B);
        ASSERT_EQ(pb_nested.array1(i).c(), nested->m_Array1.m_Data[i].m_C);
        for (int j = 0; j < count2; ++j)
        {
            ASSERT_EQ(pb_nested.array1(i).array2(j).a(), nested->m_Array1.m_Data[i].m_Array2.m_Data[j].m_A);
        }
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_NestedArray_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(pb_msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Bytes, Load)
{
    TestDDF::Bytes bytes;
    bytes.set_pad(".."); // Three bytes for alignment test
    bytes.set_data((void*) "foo", 3);
    std::string msg_str = bytes.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Bytes_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Bytes* msg = (DUMMY::TestDDF::Bytes*) message;
    // Ensure at least 4 bytes aligment
    ASSERT_EQ((uint32_t) 3, msg->m_Data.m_Count);
    ASSERT_EQ('f', msg->m_Data[0]);
    ASSERT_EQ('o', msg->m_Data[1]);
    ASSERT_EQ('o', msg->m_Data[2]);

    ASSERT_EQ(0U, ((uintptr_t) msg->m_Data.m_Data) & 3);

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Bytes_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Material, Load)
{
    TestDDF::MaterialDesc material_desc;
    material_desc.set_name("Simple");
    material_desc.set_fragment_program("simple");
    material_desc.set_vertex_program("simple");
    TestDDF::MaterialDesc_Parameter* p =  material_desc.add_fragment_parameters();
    p->set_name("color");
    p->set_type(TestDDF::MaterialDesc_ParameterType_VECTOR3);
    p->set_semantic(TestDDF::MaterialDesc_ParameterSemantic_COLOR);
    p->set_register_(0);
    p->mutable_value()->set_x(0.2);
    p->mutable_value()->set_y(0);
    p->mutable_value()->set_z(0.5);
    p->mutable_value()->set_w(0);

    std::string msg_str = material_desc.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    DUMMY::TestDDF::MaterialDesc* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_MaterialDesc_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_STREQ(material_desc.name().c_str(), message->m_Name );
    ASSERT_STREQ(material_desc.vertex_program().c_str(), message->m_VertexProgram );
    ASSERT_STREQ(material_desc.fragment_parameters(0).name().c_str(), message->m_FragmentParameters[0].m_Name);
    ASSERT_EQ((uint32_t) material_desc.fragment_parameters(0).type(), (uint32_t) message->m_FragmentParameters[0].m_Type);
    ASSERT_EQ((uint32_t) material_desc.fragment_parameters(0).semantic(), (uint32_t) message->m_FragmentParameters[0].m_Semantic);
    ASSERT_EQ((uint32_t) material_desc.fragment_parameters(0).register_(), (uint32_t) message->m_FragmentParameters[0].m_Register);
    ASSERT_EQ(material_desc.fragment_parameters(0).value().x(), message->m_FragmentParameters[0].m_Value.m_X);
    ASSERT_EQ(material_desc.fragment_parameters(0).value().y(), message->m_FragmentParameters[0].m_Value.m_Y);
    ASSERT_EQ(material_desc.fragment_parameters(0).value().z(), message->m_FragmentParameters[0].m_Value.m_Z);
    ASSERT_EQ(material_desc.fragment_parameters(0).value().w(), message->m_FragmentParameters[0].m_Value.m_W);

    dmDDF::FreeMessage(message);
}

TEST(MissingRequired, Load)
{
    TestDDF::MissingRequiredTemplate missing_req_temp;
    missing_req_temp.set_a(10);

    std::string msg_str = missing_req_temp.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_MissingRequired_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_MISSING_REQUIRED, e);
    ASSERT_EQ(0, message);
}

TEST(TestStructAlias, LoadSave)
{
    TestDDF::TestStructAlias struct_alias;
    struct_alias.mutable_vec2()->set_x(10);
    struct_alias.mutable_vec2()->set_y(20);

    std::string msg_str = struct_alias.SerializeAsString();

    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    DUMMY::TestDDF::TestStructAlias* message;

    dmDDF::Result e = dmDDF::LoadMessage<DUMMY::TestDDF::TestStructAlias>((void*) msg_buf, msg_buf_size, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_EQ(struct_alias.vec2().x(), message->m_Vec2.m_X);
    ASSERT_EQ(struct_alias.vec2().y(), message->m_Vec2.m_Y);

    std::string msg_str2;
    e = DDFSaveToString(message, DUMMY::TestDDF::TestStructAlias::m_DDFDescriptor, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(TestDefault, LoadSave)
{
    TestDDF::TestDefault defaulto;

    std::string msg_str = defaulto.SerializeAsString();

    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    DUMMY::TestDDF::TestDefault* message;

    dmDDF::Result e = dmDDF::LoadMessage<DUMMY::TestDDF::TestDefault>((void*) msg_buf, msg_buf_size, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_EQ(10.0f, defaulto.float_val());
    ASSERT_EQ(20.0f, defaulto.double_val());
    ASSERT_EQ(-1122, defaulto.int32_val());
    ASSERT_EQ(1234U, defaulto.uint32_val());
    ASSERT_EQ(-5678, defaulto.int64_val());
    ASSERT_EQ(5566U, defaulto.uint64_val());
    ASSERT_STREQ("a default value", defaulto.string_val().c_str());
    ASSERT_STREQ("", defaulto.non_default_string().c_str());
    ASSERT_EQ(0U, defaulto.non_default_uint64());
    ASSERT_EQ(0.0f, defaulto.quat().x());
    ASSERT_EQ(0.0f, defaulto.quat().y());
    ASSERT_EQ(0.0f, defaulto.quat().z());
    ASSERT_EQ(1.0f, defaulto.quat().w());

    ASSERT_EQ(0.0f, defaulto.sub_message().quat().x());
    ASSERT_EQ(0.0f, defaulto.sub_message().quat().y());
    ASSERT_EQ(0.0f, defaulto.sub_message().quat().z());
    ASSERT_EQ(1.0f, defaulto.sub_message().quat().w());


    ASSERT_EQ(defaulto.float_val(), message->m_FloatVal);
    ASSERT_EQ(defaulto.double_val(), message->m_DoubleVal);
    ASSERT_EQ(defaulto.int32_val(), message->m_Int32Val);
    ASSERT_EQ(defaulto.uint32_val(), message->m_Uint32Val);
    ASSERT_EQ(defaulto.int64_val(), message->m_Int64Val);
    ASSERT_EQ(defaulto.uint64_val(), message->m_Uint64Val);
    ASSERT_STREQ(defaulto.string_val().c_str(), message->m_StringVal);
    ASSERT_STREQ(defaulto.empty_string_val().c_str(), message->m_EmptyStringVal);
    ASSERT_STREQ(defaulto.non_default_string().c_str(), message->m_NonDefaultString);

    ASSERT_EQ(defaulto.non_default_uint64(), message->m_NonDefaultUint64);

    ASSERT_EQ(defaulto.sub_message().quat().x(), message->m_SubMessage.m_Quat.m_X);
    ASSERT_EQ(defaulto.sub_message().quat().y(), message->m_SubMessage.m_Quat.m_Y);
    ASSERT_EQ(defaulto.sub_message().quat().z(), message->m_SubMessage.m_Quat.m_Z);
    ASSERT_EQ(defaulto.sub_message().quat().w(), message->m_SubMessage.m_Quat.m_W);

    ASSERT_EQ(defaulto.quat().x(), message->m_Quat.m_X);

    ASSERT_EQ((int) defaulto.enum_(), (int) message->m_Enum);

    ASSERT_EQ(defaulto.bool_val(), message->m_BoolVal);
    ASSERT_EQ(defaulto.bool_val_false(), message->m_BoolValFalse);

    std::string msg_str2;
    e = DDFSaveToString(message, DUMMY::TestDDF::TestDefault::m_DDFDescriptor, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    //NOTE: We can't compare here. We store every value, ie also optional values that have value identical to the default
    //This might change in the future
    //ASSERT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Descriptor, GetDescriptor)
{
    ASSERT_EQ(&DUMMY::TestDDF_Simple_DESCRIPTOR, dmDDF::GetDescriptor("simple"));
    ASSERT_EQ(&DUMMY::TestDDF_NestedType_DESCRIPTOR, dmDDF::GetDescriptor("nested_type"));
    ASSERT_EQ(&DUMMY::TestDDF_NestedType_TheNestedType_DESCRIPTOR, dmDDF::GetDescriptor("the_nested_type"));
    ASSERT_EQ(&DUMMY::TestDDF_MaterialDesc_DESCRIPTOR, dmDDF::GetDescriptor("material_desc"));
}

TEST(PointerOffset, ResolvePointers)
{
    const char* values = "The quick brown fox";
    const char* name = "Bengan";
    const char* names[] = {"Vyvyan", "Rik", "Neil", "Mike"};
    TestDDF::ResolvePointers srcmsg;
    srcmsg.set_data((uint8_t*)values, strlen(values)+1);
    srcmsg.set_name(name);
    for( size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
        srcmsg.add_names(names[i]);
    }

    std::string msg_str = srcmsg.SerializeAsString();

    DUMMY::TestDDF::ResolvePointers* msg;
    uint32_t msg_size;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_str.c_str(), msg_str.size(), &DUMMY::TestDDF_ResolvePointers_DESCRIPTOR, (void**)&msg, dmDDF::OPTION_OFFSET_POINTERS, &msg_size);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    void* msgcopy = malloc(msg_size);
    memcpy(msgcopy, msg, msg_size);

    memset(msg, 0, msg_size); // Works so long as the FreeMessage only frees the memory
    dmDDF::FreeMessage(msg);

    msg = (DUMMY::TestDDF::ResolvePointers*)msgcopy;

    ASSERT_EQ(strlen(values)+1, msg->m_Data.m_Count);
    ASSERT_TRUE( uintptr_t(msg->m_Data.m_Data) <= (msg_size - (strlen(values)+1)) ); // the offset should be between [0, sizeof message - value length]
    ASSERT_STREQ( values, (const char*)((uintptr_t)msg + (uintptr_t)msg->m_Data.m_Data));
    ASSERT_STREQ( name, (const char*)((uintptr_t)msg + (uintptr_t)msg->m_Name));
    for( size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
        uintptr_t* nameoffsets = (uintptr_t*)((uintptr_t)msg + (uintptr_t)msg->m_Names.m_Data);
        ASSERT_STREQ( names[i], (const char*) ((uintptr_t)msg + nameoffsets[i]));
    }

    //test the resolving of pointers
    e = dmDDF::ResolvePointers(&DUMMY::TestDDF_ResolvePointers_DESCRIPTOR, msg);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_STREQ( values, (const char*)msg->m_Data.m_Data);
    ASSERT_STREQ( name, (const char*)msg->m_Name);
    for( size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i) {
        ASSERT_STREQ( names[i], (const char*)msg->m_Names[i]);
    }

    free(msg);
}

TEST(AlignmentTests, AlignStruct)
{
    DM_STATIC_ASSERT(sizeof(DUMMY::TestDDF::TestMessageAlignment) % 16 == 0, Invalid_Struct_Size);
}

TEST(AlignmentTests, AlignField)
{
    size_t struct_size = sizeof(DUMMY::TestDDF::TestFieldAlignment);
    DUMMY::TestDDF::TestFieldAlignment *dummy = 0x0;
    ASSERT_EQ(dmMemory::RESULT_OK, dmMemory::AlignedMalloc((void**)&dummy, 16, struct_size));

    ASSERT_EQ(16, offsetof(DUMMY::TestDDF::TestFieldAlignment, m_NeedsToBeAligned1));
    ASSERT_EQ(32, offsetof(DUMMY::TestDDF::TestFieldAlignment, m_NeedsToBeAligned2));
    ASSERT_EQ(0, ((unsigned long)(&(dummy->m_NeedsToBeAligned1))) % 16);
    ASSERT_EQ(0, ((unsigned long)(&(dummy->m_NeedsToBeAligned2))) % 16);

    dmMemory::AlignedFree(dummy);
}

TEST(OneOfTests, LoadSimple)
{
    TestDDF::OneOfMessage oneof_message_desc;

    oneof_message_desc.set_not_in_oneof(99);

    // the second call will overwrite the enum value for the one_of_field_one oneof
    oneof_message_desc.set_enum_val(::TestDDF::TEST_ENUM_VAL2);
    oneof_message_desc.set_int_val(1337);

    // Set a nested oneof
    TestDDF::OneOfMessage::Nested nested;
    nested.set_nested_int_val(10);
    *oneof_message_desc.mutable_nested_val() = nested;

    // Set a string
    std::string simple_string_valid = "set_simple_string_two_test_string";
    oneof_message_desc.set_simple_string_two(simple_string_valid);

    std::string msg_str   = oneof_message_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::OneOfMessage* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_OneOfMessage_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(1337, message->m_OneOfFieldOne.m_IntVal);
    ASSERT_EQ(99,   message->m_NotInOneof);
    ASSERT_EQ(10,   message->m_NestedVal.m_NestedFieldName.m_NestedIntVal);
    ASSERT_EQ(13,   message->m_NestedVal.m_NestedButNotInOneof);
    ASSERT_STREQ(simple_string_valid.c_str(), message->m_OneOfFieldTwo.m_SimpleStringTwo);

    dmDDF::FreeMessage(message);
}

TEST(OneOfTests, Repeated)
{
    TestDDF::OneOfMessageRepeat oneof_message_desc;

    uint64_t large_number = 0x100000001;

    TestDDF::OneOfMessageRepeat::Nested* nested_one_valid = oneof_message_desc.add_nested_one_of();
    nested_one_valid->set_val_b(256);

    TestDDF::OneOfMessageRepeat::Nested* nested_two_valid = oneof_message_desc.add_nested_one_of();
    nested_two_valid->set_val_a(large_number);

    std::string msg_str   = oneof_message_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::OneOfMessageRepeat* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_OneOfMessageRepeat_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(2,            message->m_NestedOneOf.m_Count);
    ASSERT_EQ(256,          message->m_NestedOneOf[0].m_Values.m_ValB);
    ASSERT_EQ(large_number, message->m_NestedOneOf[1].m_Values.m_ValA);
    ASSERT_EQ(0,            message->m_NestedOneOfWithDefaults.m_Values.m_ValA);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_OneOfMessageRepeat_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    // Note: We can't compare the serialized input with the saved string..
    //       When we load a repeated field, we load all the internal fields in the repeated field,
    //       because we don't know which ones have been set. This is not the case for non-repeated fields,
    //       since the fields that haven't been set will not be present in the input buffer.
    //       This is not a problem per se, other than that the serialized string will contain a bit more data.
    // ASSERT_EQ(msg_str, save_str);

    DUMMY::TestDDF::OneOfMessageRepeat* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &DUMMY::TestDDF_OneOfMessageRepeat_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(256,          saved_message->m_NestedOneOf[0].m_Values.m_ValB);
    ASSERT_EQ(large_number, saved_message->m_NestedOneOf[1].m_Values.m_ValA);

    dmDDF::FreeMessage(message);
}

TEST(OneOfTests, Save)
{
    std::string oneof_string_val = "This is a somewhat long string!";
    TestDDF::OneOfMessageSave oneof_message_desc;
    oneof_message_desc.set_int_val(999);
    oneof_message_desc.set_string_val(oneof_string_val);

    std::string msg_str   = oneof_message_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::OneOfMessageSave* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_OneOfMessageSave_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    // These are member numbers in the protobuf message
    ASSERT_EQ(message->m_OneOfFieldOneOfIndex, 2);
    ASSERT_EQ(message->m_OneOfFieldStringOneOfIndex, 6);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_OneOfMessageSave_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, save_str);
    ASSERT_STREQ(oneof_string_val.c_str(), message->m_OneOfFieldString.m_StringVal);

    DUMMY::TestDDF::OneOfMessageSave* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &DUMMY::TestDDF_OneOfMessageSave_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_EQ(message->m_OneOfField.m_IntVal, saved_message->m_OneOfField.m_IntVal);
    ASSERT_EQ((int)message->m_OneOfField.m_BoolVal, (int)saved_message->m_OneOfField.m_BoolVal);
    ASSERT_STREQ(message->m_OneOfFieldString.m_StringVal, saved_message->m_OneOfFieldString.m_StringVal);

    dmDDF::FreeMessage(message);
}

TEST(OneOfTests, Nested)
{
    TestDDF::OneOfMessageNested oneof_message_desc;

    TestDDF::OneOfMessageNested_SubTwo* sub_two = new TestDDF::OneOfMessageNested_SubTwo();
    TestDDF::OneOfMessageNested_SubOne* sub_one_for_two = new TestDDF::OneOfMessageNested_SubOne;
    sub_one_for_two->set_value_two(13.0f);
    sub_one_for_two->set_non_one_of(1337);

    sub_two->set_allocated_value_sub_one(sub_one_for_two);
    oneof_message_desc.set_allocated_two(sub_two);

    std::string msg_str   = oneof_message_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::OneOfMessageNested* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_OneOfMessageNested_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_NEAR(13.0f, message->m_Composite.m_Two.m_Values.m_ValueSubOne.m_Values.m_ValueTwo, 0.0001f);
    ASSERT_EQ(1337, message->m_Composite.m_Two.m_Values.m_ValueSubOne.m_NonOneOf);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_OneOfMessageNested_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_EQ(msg_str, save_str);

    dmDDF::FreeMessage(message);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}
