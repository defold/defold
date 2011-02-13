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

/*
 * TODO:
 * Tester
 * - FŠlt
 *   - Extra fŠlt
 *   - FŠlt som fattar
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
    EXPECT_STREQ("Simple", d.m_Name);
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
        ASSERT_EQ(simple.a(), msg->m_a);

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

        ASSERT_EQ(simple.a(), msg->m_a);

        dmDDF::FreeMessage(msg);
    }
}

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
        ASSERT_EQ(simple.a(), msg->m_a);

        dmDDF::FreeMessage(message);
    }
}

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
    EXPECT_STREQ("ScalarTypes", d.m_Name);
    EXPECT_EQ(7, d.m_FieldCount);

    // Test field(s)
    const char* names[] =
    {
        "Float",
        "Double",
        "Int32",
        "Uint32",
        "Int64",
        "Uint64",
        "String",
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
    scalar_types.set_float_(1.0f);
    scalar_types.set_double_(2.0);
    scalar_types.set_int32(INT32_MAX);
    scalar_types.set_uint32(UINT32_MAX);
    scalar_types.set_int64(INT64_MAX);
    scalar_types.set_uint64(UINT64_MAX);
    scalar_types.set_string("foo");

    std::string msg_str = scalar_types.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_ScalarTypes_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::ScalarTypes* msg = (DUMMY::TestDDF::ScalarTypes*) message;
    EXPECT_EQ(scalar_types.float_(), msg->m_Float);
    EXPECT_EQ(scalar_types.double_(), msg->m_Double);
    EXPECT_EQ(scalar_types.int32(), msg->m_Int32);
    EXPECT_EQ(scalar_types.uint32(), msg->m_Uint32);
    EXPECT_EQ(scalar_types.int64(), msg->m_Int64);
    EXPECT_EQ(scalar_types.uint64(), msg->m_Uint64);
    EXPECT_STREQ(scalar_types.string().c_str(), msg->m_String);

    std::string msg_str2;
    e = DDFSaveToString(message, &DUMMY::TestDDF_ScalarTypes_DESCRIPTOR, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    EXPECT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

TEST(Enum, Simple)
{
    ASSERT_EQ(10, DUMMY::TestDDF::TestEnumVal1);
    ASSERT_EQ(20, DUMMY::TestDDF::TestEnumVal2);

    ASSERT_STREQ("TestEnumVal1", DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[0].m_Name);
    ASSERT_STREQ("TestEnumVal2", DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[1].m_Name);

    ASSERT_EQ(10, DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[0].m_Value);
    ASSERT_EQ(20, DUMMY::TestDDF_TestEnum_DESCRIPTOR.m_EnumValues[1].m_Value);

    ASSERT_STREQ("TestEnumVal1", GetEnumName(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, 10));
    ASSERT_STREQ("TestEnumVal2", GetEnumName(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, 20));
    ASSERT_EQ(0, GetEnumName(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, -1));

    ASSERT_EQ(10, GetEnumValue(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, "TestEnumVal1"));
    ASSERT_EQ(20, GetEnumValue(&DUMMY::TestDDF_TestEnum_DESCRIPTOR, "TestEnumVal2"));
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
    EXPECT_EQ((uint32_t) count, msg->m_array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(repated.array(i).x(), msg->m_array.m_Data[i].m_x);
        EXPECT_EQ(repated.array(i).y(), msg->m_array.m_Data[i].m_y);
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
    EXPECT_EQ((uint32_t)count, msg->m_array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(repated.array(i), msg->m_array.m_Data[i]);
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
    EXPECT_EQ((uint32_t) count, msg->m_array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_STREQ(repated.array(i).c_str(), msg->m_array.m_Data[i]);
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

    EXPECT_EQ(n1.x(), msg->m_n1.m_x);
    EXPECT_EQ(n2.x(), msg->m_n2.m_x);

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
    mesh.set_primitivecount(count);
    mesh.set_name("MyMesh");
    mesh.set_primitivetype(TestDDF::Mesh_Primitive_TRIANGLES);

    std::string msg_str = mesh.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Mesh_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Mesh* msg = (DUMMY::TestDDF::Mesh*) message;
    EXPECT_EQ((uint32_t) count, msg->m_PrimitiveCount);
    EXPECT_STREQ(mesh.name().c_str(), msg->m_Name);
    EXPECT_EQ((uint32_t) mesh.primitivetype(), (uint32_t) msg->m_PrimitiveType);

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

    EXPECT_EQ((uint32_t) count1, nested->m_array1.m_Count);
    ASSERT_EQ(pb_nested.d(), nested->m_d);
    ASSERT_EQ(pb_nested.e(), nested->m_e);

    for (int i = 0; i < count1; ++i)
    {
        EXPECT_EQ((uint32_t) count2, nested->m_array1.m_Data[i].m_array2.m_Count);
        ASSERT_EQ(pb_nested.array1(i).b(), nested->m_array1.m_Data[i].m_b);
        ASSERT_EQ(pb_nested.array1(i).c(), nested->m_array1.m_Data[i].m_c);
        for (int j = 0; j < count2; ++j)
        {
            ASSERT_EQ(pb_nested.array1(i).array2(j).a(), nested->m_array1.m_Data[i].m_array2.m_Data[j].m_a);
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
    bytes.set_data((void*) "foo", 3);
    std::string msg_str = bytes.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    dmDDF::Result e = dmDDF::LoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Bytes_DESCRIPTOR, &message);
    ASSERT_EQ(dmDDF::RESULT_OK, e);

    DUMMY::TestDDF::Bytes* msg = (DUMMY::TestDDF::Bytes*) message;
    ASSERT_EQ((uint32_t) 3, msg->m_data.m_Count);
    ASSERT_EQ('f', msg->m_data[0]);
    ASSERT_EQ('o', msg->m_data[1]);
    ASSERT_EQ('o', msg->m_data[2]);

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
    material_desc.set_fragmentprogram("simple");
    material_desc.set_vertexprogram("simple");
    TestDDF::MaterialDesc_Parameter* p =  material_desc.add_fragmentparameters();
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
    ASSERT_STREQ(material_desc.vertexprogram().c_str(), message->m_VertexProgram );
    ASSERT_STREQ(material_desc.fragmentparameters(0).name().c_str(), message->m_FragmentParameters[0].m_Name);
    ASSERT_EQ((uint32_t) material_desc.fragmentparameters(0).type(), (uint32_t) message->m_FragmentParameters[0].m_Type);
    ASSERT_EQ((uint32_t) material_desc.fragmentparameters(0).semantic(), (uint32_t) message->m_FragmentParameters[0].m_Semantic);
    ASSERT_EQ((uint32_t) material_desc.fragmentparameters(0).register_(), (uint32_t) message->m_FragmentParameters[0].m_Register);
    ASSERT_EQ(material_desc.fragmentparameters(0).value().x(), message->m_FragmentParameters[0].m_Value.m_x);
    ASSERT_EQ(material_desc.fragmentparameters(0).value().y(), message->m_FragmentParameters[0].m_Value.m_y);
    ASSERT_EQ(material_desc.fragmentparameters(0).value().z(), message->m_FragmentParameters[0].m_Value.m_z);
    ASSERT_EQ(material_desc.fragmentparameters(0).value().w(), message->m_FragmentParameters[0].m_Value.m_w);

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

    ASSERT_EQ(10.0f, defaulto.float_());
    ASSERT_EQ(20.0f, defaulto.double_());
    ASSERT_EQ(-1122, defaulto.int32());
    ASSERT_EQ(1234U, defaulto.uint32());
    ASSERT_EQ(-5678, defaulto.int64());
    ASSERT_EQ(5566U, defaulto.uint64());
    ASSERT_STREQ("a default value", defaulto.string().c_str());
    ASSERT_STREQ("", defaulto.nondefaultstring().c_str());
    ASSERT_EQ(0U, defaulto.nondefaultuint64());
    ASSERT_EQ(0.0f, defaulto.quat().x());
    ASSERT_EQ(0.0f, defaulto.quat().y());
    ASSERT_EQ(0.0f, defaulto.quat().z());
    ASSERT_EQ(1.0f, defaulto.quat().w());

    ASSERT_EQ(0.0f, defaulto.submessage().quat().x());
    ASSERT_EQ(0.0f, defaulto.submessage().quat().y());
    ASSERT_EQ(0.0f, defaulto.submessage().quat().z());
    ASSERT_EQ(1.0f, defaulto.submessage().quat().w());


    ASSERT_EQ(defaulto.float_(), message->m_Float);
    ASSERT_EQ(defaulto.double_(), message->m_Double);
    ASSERT_EQ(defaulto.int32(), message->m_Int32);
    ASSERT_EQ(defaulto.uint32(), message->m_Uint32);
    ASSERT_EQ(defaulto.int64(), message->m_Int64);
    ASSERT_EQ(defaulto.uint64(), message->m_Uint64);
    ASSERT_STREQ(defaulto.string().c_str(), message->m_String);

    //NOTE: We store non-defined strings as NULL and not ""
    //We might change this in the future
    ASSERT_EQ(0U, message->m_NonDefaultString);
    //See comment just above why this isn't active
    //ASSERT_STREQ(defaulto.nondefaultstring().c_str(), message->m_NonDefaultString);

    ASSERT_EQ(defaulto.nondefaultuint64(), message->m_NonDefaultUint64);

    ASSERT_EQ(defaulto.submessage().quat().x(), message->m_SubMessage.m_quat.m_x);
    ASSERT_EQ(defaulto.submessage().quat().y(), message->m_SubMessage.m_quat.m_y);
    ASSERT_EQ(defaulto.submessage().quat().z(), message->m_SubMessage.m_quat.m_z);
    ASSERT_EQ(defaulto.submessage().quat().w(), message->m_SubMessage.m_quat.m_w);

    ASSERT_EQ(defaulto.quat().x(), message->m_quat.m_x);

    std::string msg_str2;
    e = DDFSaveToString(message, DUMMY::TestDDF::TestDefault::m_DDFDescriptor, msg_str2);
    ASSERT_EQ(dmDDF::RESULT_OK, e);
    //NOTE: We can't compare here. We store every value, ie also optional values that have value identical to the default
    //This might change in the future
    //EXPECT_EQ(msg_str, msg_str2);

    dmDDF::FreeMessage(message);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    google::protobuf::ShutdownProtobufLibrary();
    return ret;
}
