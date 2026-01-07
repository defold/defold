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


// NOTE:
// Ideally we should be able to import the ddf_struct.proto from test_ddf_proto.proto,
// but doing so will generate build and linker errors due to including both our generated
// header files as well as the protoc generated header files.
//
// To circumvent this, we include the ddfc generated header inside a different module called "test_ddf_struct.cpp"
// and perform the ddf struct tests inside that module instead of here.
#include "ddf/ddf_struct.pb.h"

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

dmDDF::Result DDFSaveToString(const void* message, const dmDDF::Descriptor* desc, std::string& str)
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
        snprintf(tmp, 32, "%d", i*10);
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
    dmDDF::FreeMessage(message);
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

    dmDDF::FreeMessage(saved_message);
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

    dmDDF::FreeMessage(saved_message);
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

TEST(OneOfTests, Recursive)
{
    TestDDF::TestMessageRecursive oneof_message_desc;
    TestDDF::TestMessageRecursive_Object* root        = oneof_message_desc.add_objects();
    TestDDF::TestMessageRecursive_Member* root_member = root->add_members();
    TestDDF::TestMessageRecursive_Object* child0      = new TestDDF::TestMessageRecursive_Object();

    child0->set_name("child0");
    root->set_name("root");
    root_member->set_name("root_member");
    root_member->set_allocated_obj_val(child0);

    std::string msg_str   = oneof_message_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::TestMessageRecursive* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_TestMessageRecursive_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    dmDDF::FreeMessage(message);
}

TEST(Recursive, TreeSimple)
{
    TestDDF::MessageRecursiveB* msg_a_child = new TestDDF::MessageRecursiveB();
    msg_a_child->set_val_b(666);

    TestDDF::MessageRecursiveA* msg_a = new TestDDF::MessageRecursiveA();
    msg_a->set_val_a(999);
    msg_a->set_allocated_my_b(msg_a_child);

    TestDDF::MessageRecursiveB msg_b;
    msg_b.set_allocated_my_a(msg_a);
    msg_b.set_val_b(1337);

    // Structure
    // msg_b (MessageRecursiveB)
    //   |_ val_b : 1337
    //   |_ my_a  :
    //     |_ val_a : 999
    //     |_ my_b  :
    //        |_ val_b : 666
    //        |_ my_a  : NULL

    std::string msg_str   = msg_b.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::MessageRecursiveB* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_MessageRecursiveB_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_EQ(1337, message->m_ValB);
    ASSERT_EQ(999, message->m_MyA.m_ValA);
    ASSERT_EQ(666, message->m_MyA.m_MyB->m_ValB);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_MessageRecursiveB_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::MessageRecursiveB* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &DUMMY::TestDDF_MessageRecursiveB_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_EQ(1337, saved_message->m_ValB);
    ASSERT_EQ(999, saved_message->m_MyA.m_ValA);
    ASSERT_EQ(666, saved_message->m_MyA.m_MyB->m_ValB);

    dmDDF::FreeMessage(message);
    dmDDF::FreeMessage(saved_message);
}

TEST(Recursive, Repeated)
{
    TestDDF::RecursiveRepeat msg;
    TestDDF::MessageRecursiveA* item_0     = msg.add_list_a();
    TestDDF::MessageRecursiveB* item_0_b   = new TestDDF::MessageRecursiveB();
    TestDDF::MessageRecursiveA* item_0_b_a = new TestDDF::MessageRecursiveA();

    TestDDF::MessageRecursiveA* item_1   = msg.add_list_a();
    TestDDF::MessageRecursiveB* item_1_b = new TestDDF::MessageRecursiveB();

    std::string test_string = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";

    msg.add_list_numbers(32768);
    msg.add_list_numbers(32769);
    msg.set_float_val(0.5f);
    msg.set_string_val(test_string);

    item_0->set_val_a(1338);
    item_0->set_allocated_my_b(item_0_b);
    {
        item_0_b->set_val_b(999);
        item_0_b->set_allocated_my_a(item_0_b_a);
        {
            item_0_b_a->set_val_a(-1337);
        }
    }

    item_1->set_val_a(666);
    item_1->set_allocated_my_b(item_1_b);
    {
        item_1_b->set_val_b(2001);
    }

    std::string msg_str   = msg.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::RecursiveRepeat* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_RecursiveRepeat_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::MessageRecursiveA* child_0 = &message->m_ListA[0];
    DUMMY::TestDDF::MessageRecursiveA* child_1 = &message->m_ListA[1];

    ASSERT_EQ(1338, child_0->m_ValA);
    ASSERT_EQ(999, child_0->m_MyB->m_ValB);
    ASSERT_EQ(-1337, child_0->m_MyB->m_MyA.m_ValA);

    ASSERT_EQ(666, child_1->m_ValA);
    ASSERT_EQ(2001, child_1->m_MyB->m_ValB);

    ASSERT_EQ(32768, message->m_ListNumbers[0]);
    ASSERT_EQ(32769, message->m_ListNumbers[1]);
    ASSERT_NEAR(0.5f, message->m_FloatVal, 0.01f);
    ASSERT_STREQ(test_string.c_str(), message->m_StringVal);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_RecursiveRepeat_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::RecursiveRepeat* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &DUMMY::TestDDF_RecursiveRepeat_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_EQ(1338, saved_message->m_ListA[0].m_ValA);
    ASSERT_EQ(999, saved_message->m_ListA[0].m_MyB->m_ValB);
    ASSERT_EQ(-1337, saved_message->m_ListA[0].m_MyB->m_MyA.m_ValA);

    ASSERT_EQ(666, saved_message->m_ListA[1].m_ValA);
    ASSERT_EQ(2001, saved_message->m_ListA[1].m_MyB->m_ValB);

    ASSERT_EQ(32768, saved_message->m_ListNumbers[0]);
    ASSERT_EQ(32769, saved_message->m_ListNumbers[1]);
    ASSERT_NEAR(0.5f, saved_message->m_FloatVal, 0.01f);
    ASSERT_STREQ(test_string.c_str(), saved_message->m_StringVal);

    dmDDF::FreeMessage(message);
    dmDDF::FreeMessage(saved_message);
}

TEST(JSON, Simple)
{
    /*
    {
        "root" : {
            "array_int" : [ 1337, 999, 666 ],
            "object_value" : {
                "object_array" : [ 2001, 13 ]
            }
        }
    }
    */

    TestDDF::JSONObject root;
    {
        TestDDF::JSONValue* array_int_value = new TestDDF::JSONValue();
        TestDDF::JSONArray* array_int_value_items = new TestDDF::JSONArray();

        TestDDF::JSONValue* array_int_value_items_1 = array_int_value_items->add_items();
        array_int_value_items_1->set_number_value(1337);

        TestDDF::JSONValue* array_int_value_items_2 = array_int_value_items->add_items();
        array_int_value_items_2->set_number_value(999);

        TestDDF::JSONValue* array_int_value_items_3 = array_int_value_items->add_items();
        array_int_value_items_3->set_number_value(666);

        array_int_value->set_allocated_array_value(array_int_value_items);

        TestDDF::JSONPair* array_int = root.add_pairs();
        array_int->set_key("array_int");
        array_int->set_allocated_value(array_int_value);
    }

    {
        TestDDF::JSONArray* object_value_object_array = new TestDDF::JSONArray();
        TestDDF::JSONValue* object_value_object_array_1 = object_value_object_array->add_items();
        object_value_object_array_1->set_number_value(2001);

        TestDDF::JSONValue* object_value_object_array_2 = object_value_object_array->add_items();
        object_value_object_array_2->set_number_value(13);

        TestDDF::JSONValue* object_value_object_array_3 = object_value_object_array->add_items();
        object_value_object_array_3->set_number_value(27);

        TestDDF::JSONObject* object_array = new TestDDF::JSONObject();
        TestDDF::JSONPair* object_array_key = object_array->add_pairs();
        TestDDF::JSONValue* object_array_value = new TestDDF::JSONValue();

        object_array_value->set_allocated_array_value(object_value_object_array);

        object_array_key->set_key("object_array");
        object_array_key->set_allocated_value(object_array_value);

        TestDDF::JSONValue* object_value = new TestDDF::JSONValue();
        object_value->set_allocated_object_value(object_array);

        TestDDF::JSONPair* object = root.add_pairs();
        object->set_key("object_value");
        object->set_allocated_value(object_value);
    }

    std::string msg_str   = root.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::JSONObject* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_JSONObject_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::JSONPair* json_array_int = &message->m_Pairs[0];
    DUMMY::TestDDF::JSONArray* json_array_int_items = json_array_int->m_Value.m_Value.m_ArrayValue;

    DUMMY::TestDDF::JSONPair* json_object_array = &message->m_Pairs[1];
    DUMMY::TestDDF::JSONObject* json_object_array_items = json_object_array->m_Value.m_Value.m_ObjectValue;
    DUMMY::TestDDF::JSONArray* json_object_array_items_items = json_object_array_items->m_Pairs[0].m_Value.m_Value.m_ArrayValue;

    ASSERT_STREQ("array_int", json_array_int->m_Key);

    ASSERT_EQ(1337, json_array_int_items->m_Items[0].m_Value.m_NumberValue);
    ASSERT_EQ(999, json_array_int_items->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(666, json_array_int_items->m_Items[2].m_Value.m_NumberValue);

    ASSERT_STREQ("object_value", json_object_array->m_Key);

    ASSERT_EQ(2001, json_object_array_items_items->m_Items[0].m_Value.m_NumberValue);
    ASSERT_EQ(13, json_object_array_items_items->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(27, json_object_array_items_items->m_Items[2].m_Value.m_NumberValue);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_JSONObject_DESCRIPTOR, save_str);
    dmDDF::FreeMessage(message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::JSONObject* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &DUMMY::TestDDF_JSONObject_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ASSERT_STREQ("array_int", saved_message->m_Pairs[0].m_Key);

    ASSERT_EQ(1337, saved_message->m_Pairs[0].m_Value.m_Value.m_ArrayValue->m_Items[0].m_Value.m_NumberValue);
    ASSERT_EQ(999, saved_message->m_Pairs[0].m_Value.m_Value.m_ArrayValue->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(666, saved_message->m_Pairs[0].m_Value.m_Value.m_ArrayValue->m_Items[2].m_Value.m_NumberValue);

    ASSERT_STREQ("object_value", saved_message->m_Pairs[1].m_Key);

    ASSERT_EQ(2001, saved_message->m_Pairs[1].m_Value.m_Value.m_ObjectValue->m_Pairs[0].m_Value.m_Value.m_ArrayValue->m_Items[0].m_Value.m_NumberValue);
    ASSERT_EQ(13, saved_message->m_Pairs[1].m_Value.m_Value.m_ObjectValue->m_Pairs[0].m_Value.m_Value.m_ArrayValue->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(27, saved_message->m_Pairs[1].m_Value.m_Value.m_ObjectValue->m_Pairs[0].m_Value.m_Value.m_ArrayValue->m_Items[2].m_Value.m_NumberValue);

    dmDDF::FreeMessage(saved_message);
}

void ValidateComplexJSONMessage(DUMMY::TestDDF::JSONObject* message)
{
    // ----------------------------------------------------------------------
    // Assertions — Verify structure
    // ----------------------------------------------------------------------
    ASSERT_EQ(3u, message->m_Pairs.m_Count);
    ASSERT_STREQ("array_int",    message->m_Pairs[0].m_Key);
    ASSERT_STREQ("object_value", message->m_Pairs[1].m_Key);
    ASSERT_STREQ("mixed_array",  message->m_Pairs[2].m_Key);

    // array_int
    DUMMY::TestDDF::JSONArray* array_int = message->m_Pairs[0].m_Value.m_Value.m_ArrayValue;
    ASSERT_EQ(3u, array_int->m_Items.m_Count);
    ASSERT_EQ(1337, array_int->m_Items[0].m_Value.m_NumberValue);
    ASSERT_EQ(999,  array_int->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(666,  array_int->m_Items[2].m_Value.m_NumberValue);

    // object_value
    DUMMY::TestDDF::JSONObject* object_value = message->m_Pairs[1].m_Value.m_Value.m_ObjectValue;
    ASSERT_EQ(2u, object_value->m_Pairs.m_Count);

    DUMMY::TestDDF::JSONArray* obj_array = object_value->m_Pairs[0].m_Value.m_Value.m_ArrayValue;
    ASSERT_EQ(3u, obj_array->m_Items.m_Count);
    ASSERT_EQ(2001, obj_array->m_Items[0].m_Value.m_NumberValue);
    ASSERT_EQ(13,   obj_array->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(27,   obj_array->m_Items[2].m_Value.m_NumberValue);

    // nested_object
    DUMMY::TestDDF::JSONObject* nested_object = object_value->m_Pairs[1].m_Value.m_Value.m_ObjectValue;
    ASSERT_EQ(3u, nested_object->m_Pairs.m_Count);
    ASSERT_STREQ("value1", nested_object->m_Pairs[0].m_Value.m_Value.m_StringValue);
    ASSERT_EQ(false, nested_object->m_Pairs[1].m_Value.m_Value.m_BoolValue);

    DUMMY::TestDDF::JSONArray* deep_array = nested_object->m_Pairs[2].m_Value.m_Value.m_ArrayValue;
    ASSERT_EQ(2u, deep_array->m_Items.m_Count);
    DUMMY::TestDDF::JSONObject* inner_object = deep_array->m_Items[0].m_Value.m_ObjectValue;
    ASSERT_EQ(1u, inner_object->m_Pairs.m_Count);
    ASSERT_EQ(42, inner_object->m_Pairs[0].m_Value.m_Value.m_NumberValue);
    ASSERT_STREQ("text_in_array", deep_array->m_Items[1].m_Value.m_StringValue);

    // mixed_array
    DUMMY::TestDDF::JSONArray* mixed_array = message->m_Pairs[2].m_Value.m_Value.m_ArrayValue;
    ASSERT_EQ(4u, mixed_array->m_Items.m_Count);
    ASSERT_STREQ("string_in_array", mixed_array->m_Items[0].m_Value.m_StringValue);
    ASSERT_EQ(12345, mixed_array->m_Items[1].m_Value.m_NumberValue);
    ASSERT_EQ(true, mixed_array->m_Items[2].m_Value.m_BoolValue);
    ASSERT_STREQ("obj_in_array_val", mixed_array->m_Items[3].m_Value.m_ObjectValue->m_Pairs[0].m_Value.m_Value.m_StringValue);
}

TEST(JSON, Complex)
{
    TestDDF::JSONObject root;

    // ----------------------------------------------------------------------
    // "array_int" : [1337, 999, 666]
    // ----------------------------------------------------------------------
    {
        TestDDF::JSONValue* array_int_value = new TestDDF::JSONValue();
        TestDDF::JSONArray* array_int_items = new TestDDF::JSONArray();

        int values[] = {1337, 999, 666};
        for (int i = 0; i < 3; ++i)
        {
            TestDDF::JSONValue* item = array_int_items->add_items();
            item->set_number_value(values[i]);
        }

        array_int_value->set_allocated_array_value(array_int_items);

        TestDDF::JSONPair* array_int = root.add_pairs();
        array_int->set_key("array_int");
        array_int->set_allocated_value(array_int_value);
    }

    // ----------------------------------------------------------------------
    // "object_value" : { "object_array": [...], "nested_object": {...} }
    // ----------------------------------------------------------------------
    {
        TestDDF::JSONValue* object_value = new TestDDF::JSONValue();
        TestDDF::JSONObject* object_obj  = new TestDDF::JSONObject();

        // --- object_array ---
        {
            TestDDF::JSONPair* obj_array_pair = object_obj->add_pairs();
            obj_array_pair->set_key("object_array");

            TestDDF::JSONValue* arr_val = new TestDDF::JSONValue();
            TestDDF::JSONArray* arr = new TestDDF::JSONArray();
            int arr_vals[] = {2001, 13, 27};
            for (int i = 0; i < 3; ++i)
            {
                TestDDF::JSONValue* item = arr->add_items();
                item->set_number_value(arr_vals[i]);
            }
            arr_val->set_allocated_array_value(arr);
            obj_array_pair->set_allocated_value(arr_val);
        }

        // --- nested_object ---
        {
            TestDDF::JSONPair* nested_obj_pair = object_obj->add_pairs();
            nested_obj_pair->set_key("nested_object");

            TestDDF::JSONValue* nested_obj_value = new TestDDF::JSONValue();
            TestDDF::JSONObject* nested_obj = new TestDDF::JSONObject();

            // key1 : "value1"
            TestDDF::JSONPair* kv1 = nested_obj->add_pairs();
            kv1->set_key("key1");
            TestDDF::JSONValue* v1 = new TestDDF::JSONValue();
            v1->set_string_value("value1");
            kv1->set_allocated_value(v1);

            // key2 : false
            TestDDF::JSONPair* kv2 = nested_obj->add_pairs();
            kv2->set_key("key2");
            TestDDF::JSONValue* v2 = new TestDDF::JSONValue();
            v2->set_bool_value(false);
            kv2->set_allocated_value(v2);

            // deep_array : [ { "inner_key": 42 }, "text_in_array" ]
            TestDDF::JSONPair* kv3 = nested_obj->add_pairs();
            kv3->set_key("deep_array");
            TestDDF::JSONValue* v3 = new TestDDF::JSONValue();
            TestDDF::JSONArray* deep_arr = new TestDDF::JSONArray();

            // element 1: object { "inner_key": 42 }
            TestDDF::JSONValue* obj_in_arr = deep_arr->add_items();
            TestDDF::JSONObject* inner_obj = new TestDDF::JSONObject();
            TestDDF::JSONPair* inner_pair = inner_obj->add_pairs();
            inner_pair->set_key("inner_key");
            TestDDF::JSONValue* inner_val = new TestDDF::JSONValue();
            inner_val->set_number_value(42);
            inner_pair->set_allocated_value(inner_val);
            obj_in_arr->set_allocated_object_value(inner_obj);

            // element 2: string "text_in_array"
            TestDDF::JSONValue* text_val = deep_arr->add_items();
            text_val->set_string_value("text_in_array");

            v3->set_allocated_array_value(deep_arr);
            kv3->set_allocated_value(v3);

            nested_obj_value->set_allocated_object_value(nested_obj);
            nested_obj_pair->set_allocated_value(nested_obj_value);
        }

        object_value->set_allocated_object_value(object_obj);

        TestDDF::JSONPair* object_pair = root.add_pairs();
        object_pair->set_key("object_value");
        object_pair->set_allocated_value(object_value);
    }

    // ----------------------------------------------------------------------
    // "mixed_array" : ["string_in_array", 123.45, true, { "obj_in_array_key": "obj_in_array_val" }]
    // ----------------------------------------------------------------------
    {
        TestDDF::JSONValue* mixed_val = new TestDDF::JSONValue();
        TestDDF::JSONArray* mixed_arr = new TestDDF::JSONArray();

        TestDDF::JSONValue* str_val = mixed_arr->add_items();
        str_val->set_string_value("string_in_array");

        TestDDF::JSONValue* num_val = mixed_arr->add_items();
        num_val->set_number_value(12345);

        TestDDF::JSONValue* bool_val = mixed_arr->add_items();
        bool_val->set_bool_value(true);

        TestDDF::JSONValue* obj_val = mixed_arr->add_items();
        TestDDF::JSONObject* inner_obj = new TestDDF::JSONObject();
        TestDDF::JSONPair* inner_pair = inner_obj->add_pairs();
        inner_pair->set_key("obj_in_array_key");
        TestDDF::JSONValue* inner_pair_val = new TestDDF::JSONValue();
        inner_pair_val->set_string_value("obj_in_array_val");
        inner_pair->set_allocated_value(inner_pair_val);
        obj_val->set_allocated_object_value(inner_obj);

        mixed_val->set_allocated_array_value(mixed_arr);

        TestDDF::JSONPair* mixed_pair = root.add_pairs();
        mixed_pair->set_key("mixed_array");
        mixed_pair->set_allocated_value(mixed_val);
    }

    // ----------------------------------------------------------------------
    // Serialize + Load via DDF
    // ----------------------------------------------------------------------
    std::string msg_str   = root.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    DUMMY::TestDDF::JSONObject* message;
    dmDDF::Result e = dmDDF::LoadMessage((void*)msg_buf, msg_buf_size, &DUMMY::TestDDF_JSONObject_DESCRIPTOR, (void**)&message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ValidateComplexJSONMessage(message);

    std::string save_str;
    e = DDFSaveToString(message, &DUMMY::TestDDF_JSONObject_DESCRIPTOR, save_str);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    dmDDF::FreeMessage(message);

    DUMMY::TestDDF::JSONObject* saved_message;
    e = dmDDF::LoadMessage((void*) save_str.c_str(), save_str.size(), &DUMMY::TestDDF_JSONObject_DESCRIPTOR, (void**)&saved_message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    ValidateComplexJSONMessage(saved_message);

    dmDDF::FreeMessage(saved_message);
}

TEST(Struct, Simple)
{
    dmStructDDF::Struct struct_desc;

    // String value
    {
        dmStructDDF::Value value;
        value.set_string_value("world");

        (*struct_desc.mutable_fields())["hello"] = value;
    }

    // Number value
    {
        dmStructDDF::Value v;
        v.set_number_value(1337.0f);

        (*struct_desc.mutable_fields())["number"] = v;
    }

    // Boolean value
    {
        dmStructDDF::Value v;
        v.set_bool_value(true);

        (*struct_desc.mutable_fields())["boolean"] = v;
    }

    // Null value
    {
        dmStructDDF::Value v;
        v.set_null_value(dmStructDDF::NULL_VALUE);

        (*struct_desc.mutable_fields())["nothing"] = v;
    }

    std::string msg_str   = struct_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    // Implementation must live in test_ddf_struct.cpp
    TestStructSimple(msg_buf, msg_buf_size);
}

// { "user": { "id": 123, "name": "Mr.X" } }
TEST(Struct, Nested)
{
    dmStructDDF::Struct struct_desc;

    dmStructDDF::Value user_value;
    dmStructDDF::Struct* user_struct = user_value.mutable_struct_value();

    {
        dmStructDDF::Value id;
        id.set_number_value(123);
        (*user_struct->mutable_fields())["id"] = id;
    }

    {
        dmStructDDF::Value name;
        name.set_string_value("Mr.X");
        (*user_struct->mutable_fields())["name"] = name;
    }

    (*struct_desc.mutable_fields())["user"] = user_value;

    std::string msg_str   = struct_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    // Implementation must live in test_ddf_struct.cpp
    TestStructNested(msg_buf, msg_buf_size);
}

// { "values": [1, "two", false] }
TEST(Struct, List)
{
    dmStructDDF::Struct struct_desc;

    dmStructDDF::Value list_value;
    dmStructDDF::ListValue* list = list_value.mutable_list_value();

    // 1
    {
        dmStructDDF::Value v;
        v.set_number_value(1);
        *list->add_values() = v;
    }

    // "two"
    {
        dmStructDDF::Value v;
        v.set_string_value("two");
        *list->add_values() = v;
    }

    // false
    {
        dmStructDDF::Value v;
        v.set_bool_value(false);
        *list->add_values() = v;
    }

    (*struct_desc.mutable_fields())["values"] = list_value;

    std::string msg_str   = struct_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    // Implementation must live in test_ddf_struct.cpp
    TestStructList(msg_buf, msg_buf_size);
}

TEST(Struct, JSON)
{
    /*
    {
      "name": "engine",
      "version": 3,
      "features": ["rendering", "physics"],
      "config": {
        "debug": true
      }
    }
    */

    dmStructDDF::Struct struct_desc;

    // name
    {
        dmStructDDF::Value v;
        v.set_string_value("engine");
        (*struct_desc.mutable_fields())["name"] = v;
    }

    // version
    {
        dmStructDDF::Value v;
        v.set_number_value(3);
        (*struct_desc.mutable_fields())["version"] = v;
    }

    // features (array)
    {
        dmStructDDF::Value v;
        auto* list = v.mutable_list_value();

        {
            dmStructDDF::Value e;
            e.set_string_value("rendering");
            *list->add_values() = e;
        }
        {
            dmStructDDF::Value e;
            e.set_string_value("physics");
            *list->add_values() = e;
        }

        (*struct_desc.mutable_fields())["features"] = v;
    }

    // config (nested object)
    {
        dmStructDDF::Value v;
        auto* cfg = v.mutable_struct_value();

        dmStructDDF::Value dbg;
        dbg.set_bool_value(true);
        (*cfg->mutable_fields())["debug"] = dbg;

        (*struct_desc.mutable_fields())["config"] = v;
    }

    std::string msg_str   = struct_desc.SerializeAsString();
    const char* msg_buf   = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();

    // Implementation must live in test_ddf_struct.cpp
    TestStructJSON(msg_buf, msg_buf_size);
}

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}
