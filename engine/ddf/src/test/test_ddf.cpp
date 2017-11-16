#include <stdio.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <gtest/gtest.h>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

#include "../ddf/ddf.h"
#include <dlib/memory.h>

/*
 * TODO:
 * Tester
 * - Fält
 *   - Extra fält
 *   - Fält som fattas
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
    EXPECT_STREQ("simple", d.m_Name);
    EXPECT_EQ((uint32_t) 4, d.m_Size);
    EXPECT_EQ((uint32_t) 1, d.m_FieldCount);

    // Test field(s)
    const dmDDF::FieldDescriptor& f1 = d.m_Fields[0];
    EXPECT_STREQ("a", f1.m_Name);
    EXPECT_EQ((uint32_t)1, f1.m_Number);
    EXPECT_EQ((uint32_t) dmDDF::TYPE_INT32, f1.m_Type);
    EXPECT_EQ(0, f1.m_MessageDescriptor);
    EXPECT_EQ((uint32_t)0, f1.m_Offset);
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
        EXPECT_EQ(msg_str, msg_str2);

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

#if !(defined(__arm__) || defined(__arm64__))
// TODO: Disabled on iOS
// We have add functionality to located tmp-dir on iOS. See issue #624
TEST(Simple, LoadFromFile)
{
    int32_t test_values[] = { INT32_MIN, INT32_MAX, 0 };

    for (uint32_t i = 0; i < sizeof(test_values)/sizeof(test_values[0]); ++i)
    {
        TestDDF::Simple simple;
        simple.set_a(test_values[i]);

        const char* file_name = "__TEMPFILE__";
        {
            std::fstream output(file_name,  std::ios::out | std::ios::trunc | std::ios::binary);
            ASSERT_EQ(true, simple.SerializeToOstream(&output));
        }

        void* message;
        dmDDF::Result e = dmDDF::LoadMessageFromFile(file_name, &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
        ASSERT_EQ(dmDDF::RESULT_OK, e);

        #ifdef _WIN32
        _unlink(file_name);
        #else
        unlink(file_name);
        #endif

        DUMMY::TestDDF::Simple* msg = (DUMMY::TestDDF::Simple*) message;
        ASSERT_EQ(simple.a(), msg->m_A);

        dmDDF::FreeMessage(message);
    }
}
#endif

TEST(Simple, LoadFromFile2)
{
    void *message;
    dmDDF::Result e = dmDDF::LoadMessageFromFile("DOES_NOT_EXISTS", &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
    EXPECT_EQ(dmDDF::RESULT_IO_ERROR, e);
}

TEST(ScalarTypes, Types)
{
    // Test descriptor
    const dmDDF::Descriptor& d = DUMMY::TestDDF_ScalarTypes_DESCRIPTOR;
    EXPECT_STREQ("scalar_types", d.m_Name);
    EXPECT_EQ(8, d.m_FieldCount);

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
        EXPECT_STREQ(names[i], f->m_Name);
        EXPECT_EQ(i+1, f->m_Number);
        EXPECT_EQ((uint32_t) types[i], f->m_Type);
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
    EXPECT_EQ(scalar_types.float_val(), msg->m_FloatVal);
    EXPECT_EQ(scalar_types.double_val(), msg->m_DoubleVal);
    EXPECT_EQ(scalar_types.int32_val(), msg->m_Int32Val);
    EXPECT_EQ(scalar_types.uint32_val(), msg->m_Uint32Val);
    EXPECT_EQ(scalar_types.int64_val(), msg->m_Int64Val);
    EXPECT_EQ(scalar_types.uint64_val(), msg->m_Uint64Val);
    EXPECT_STREQ(scalar_types.string_val().c_str(), msg->m_StringVal);
    EXPECT_EQ(scalar_types.bool_val(), msg->m_BoolVal);

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_ScalarTypes_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);

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
    EXPECT_EQ((uint32_t) count, msg->m_Array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(repated.array(i).x(), msg->m_Array.m_Data[i].m_X);
        EXPECT_EQ(repated.array(i).y(), msg->m_Array.m_Data[i].m_Y);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Simple01Repeated_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);

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
    EXPECT_EQ((uint32_t)count, msg->m_Array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(repated.array(i), msg->m_Array.m_Data[i]);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Simple02Repeated_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);


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
    EXPECT_EQ((uint32_t) count, msg->m_Array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_STREQ(repated.array(i).c_str(), msg->m_Array.m_Data[i]);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_StringRepeated_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);

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

    EXPECT_EQ(n1.x(), msg->m_N1.m_X);
    EXPECT_EQ(n2.x(), msg->m_N2.m_X);

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_NestedMessage_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);

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
    EXPECT_EQ((uint32_t) count, msg->m_PrimitiveCount);
    EXPECT_STREQ(mesh.name().c_str(), msg->m_Name);
    EXPECT_EQ((uint32_t) mesh.primitive_type(), (uint32_t) msg->m_PrimitiveType);

    for (int i = 0; i < count * 3; ++i)
    {
        EXPECT_EQ(mesh.vertices(i), msg->m_Vertices.m_Data[i]);
        EXPECT_EQ(mesh.indices(i), msg->m_Indices.m_Data[i]);
    }

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_Mesh_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);

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

    EXPECT_EQ((uint32_t) count1, nested->m_Array1.m_Count);
    ASSERT_EQ(pb_nested.d(), nested->m_D);
    ASSERT_EQ(pb_nested.e(), nested->m_E);

    for (int i = 0; i < count1; ++i)
    {
        EXPECT_EQ((uint32_t) count2, nested->m_Array1.m_Data[i].m_Array2.m_Count);
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
    EXPECT_EQ(pb_msg_str, msg_str2);

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
    EXPECT_EQ(msg_str, msg_str2);

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
    EXPECT_EQ(msg_str, msg_str2);

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
    //EXPECT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Descriptor, GetDescriptor)
{
    ASSERT_EQ(&DUMMY::TestDDF_Simple_DESCRIPTOR, dmDDF::GetDescriptor("simple"));
    ASSERT_EQ(&DUMMY::TestDDF_NestedType_DESCRIPTOR, dmDDF::GetDescriptor("nested_type"));
    ASSERT_EQ(&DUMMY::TestDDF_NestedType_TheNestedType_DESCRIPTOR, dmDDF::GetDescriptor("the_nested_type"));
    ASSERT_EQ(&DUMMY::TestDDF_MaterialDesc_DESCRIPTOR, dmDDF::GetDescriptor("material_desc"));
}

TEST(StringOffset, Load)
{
    TestDDF::StringOffset repeated;
    repeated.set_str("a string");
    repeated.add_str_arr("foo");
    repeated.add_str_arr("bar");

    std::string msg_str = repeated.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    uint32_t msg_size;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_StringOffset_DESCRIPTOR, &message, dmDDF::OPTION_OFFSET_STRINGS, &msg_size);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::StringOffset* msg = (DUMMY::TestDDF::StringOffset*) message;

    ASSERT_STREQ(repeated.str().c_str(), (const char*) (uintptr_t(msg->m_Str) + uintptr_t(msg)));
    ASSERT_STREQ(repeated.str_arr(0).c_str(), (const char*) (uintptr_t(msg->m_StrArr[0]) + uintptr_t(msg)));
    ASSERT_STREQ(repeated.str_arr(1).c_str(), (const char*) (uintptr_t(msg->m_StrArr[1]) + uintptr_t(msg)));

    // NOTE: We don't save the message again as we do in most tests
    // Currently no support to save messages with offset strings

    dmDDF::FreeMessage(message);
}

TEST(BytesOffset, Load)
{
    const char* values = "The quick brown fox";
    const char* pad_string = "pad string";
    TestDDF::Bytes bytes;
    bytes.set_pad(pad_string);
    bytes.set_data((uint8_t*)values, strlen(values)+1);

    std::string msg_str = bytes.SerializeAsString();

    void* message;
    uint32_t msg_size;
    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_str.c_str(), msg_str.size(), &DUMMY::TestDDF_Bytes_DESCRIPTOR, &message, dmDDF::OPTION_OFFSET_STRINGS, &msg_size);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Bytes* msg = (DUMMY::TestDDF::Bytes*) message;

    ASSERT_EQ(strlen(values)+1, msg->m_Data.m_Count);
    ASSERT_TRUE( uintptr_t(msg->m_Data.m_Data) <= (msg_size - (strlen(values)+1)) ); // the offset should be between [0, sizeof message - value length]
    ASSERT_STREQ( values, (const char*)((uintptr_t)msg + (uintptr_t)msg->m_Data.m_Data));
    ASSERT_STREQ( pad_string, (const char*)((uintptr_t)msg + (uintptr_t)msg->m_Pad));

    //test the resolving of pointers
    e = dmDDF::ResolvePointers(&DUMMY::TestDDF_Bytes_DESCRIPTOR, message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    ASSERT_STREQ( values, (const char*)msg->m_Data.m_Data);
    ASSERT_STREQ( pad_string, (const char*)msg->m_Pad);

    dmDDF::FreeMessage(message);
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

int main(int argc, char **argv)
{
    dmDDF::RegisterAllTypes();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}
