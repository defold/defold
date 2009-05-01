#include <stdio.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <fstream>
#include <gtest/gtest.h>

#ifdef _WIN32
#include <io.h>
#include <stdio.h>
#else
#include <unistd.h>
#endif

#include "ddf/ddf.h"

/*
 * TODO:
 * Tester
 * - Fält
 *   - Extra fält
 *   - Fält som fattar
 */

#define DDF_EXPOSE_DESCRIPTORS
#include "default/src/test/test_ddf_proto.h"  // TODO: Path-hack!
#include "default/src/test/test_ddf_proto.pb.h"  // TODO: Path-hack!

enum MyEnum
{
    MYENUM,
};

TEST(Misc, TestEnumSize)
{
    ASSERT_EQ(sizeof(uint32_t), sizeof(MyEnum));
}

TEST(Simple, Descriptor)
{
    // Test descriptor
    const SDDFDescriptor& d = DUMMY::TestDDF_Simple_DESCRIPTOR;
    EXPECT_STREQ("Simple", d.m_Name);
    EXPECT_EQ(4, d.m_Size);
    EXPECT_EQ(1, d.m_FieldCount);

    // Test field(s)
    const SDDFFieldDescriptor& f1 = d.m_Fields[0];
    EXPECT_STREQ("a", f1.m_Name);
    EXPECT_EQ(1, f1.m_Number);
    EXPECT_EQ(DDF_TYPE_INT32, f1.m_Type);
    EXPECT_EQ(0, f1.m_MessageDescriptor);
    EXPECT_EQ(0, f1.m_Offset);
}

TEST(Simple, Load)
{
    int32_t test_values[] = { INT32_MIN, INT32_MAX, 0 };

    for (int i = 0; i < sizeof(test_values)/sizeof(test_values[0]); ++i)
    {
        TestDDF::Simple simple;
        simple.set_a(test_values[i]);
        std::string msg_str = simple.SerializeAsString();
        const char* msg_buf = msg_str.c_str();
        uint32_t msg_buf_size = msg_str.size();
        void* message;

        DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
        ASSERT_EQ(DDF_ERROR_OK, e);

        DUMMY::TestDDF::Simple* msg = (DUMMY::TestDDF::Simple*) message;
        ASSERT_EQ(simple.a(), msg->m_a);

        DDFFreeMessage(message);
    }
}

TEST(Simple, LoadFromFile)
{
    int32_t test_values[] = { INT32_MIN, INT32_MAX, 0 };

    for (int i = 0; i < sizeof(test_values)/sizeof(test_values[0]); ++i)
    {
        TestDDF::Simple simple;
        simple.set_a(test_values[i]);

        const char* file_name = "__TEMPFILE__";
        {
            std::fstream output(file_name,  std::ios::out | std::ios::trunc | std::ios::binary);
            ASSERT_EQ(true, simple.SerializeToOstream(&output));
        }

        void* message;
        DDFError e = DDFLoadMessageFromFile(file_name, &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
        ASSERT_EQ(DDF_ERROR_OK, e);

        #ifdef _WIN32
        _unlink(file_name);
        #else
        unlink(file_name);
        #endif

        DUMMY::TestDDF::Simple* msg = (DUMMY::TestDDF::Simple*) message;
        ASSERT_EQ(simple.a(), msg->m_a);

        DDFFreeMessage(message);
    }
}

TEST(Simple, LoadFromFile2)
{
    void *message;
    DDFError e = DDFLoadMessageFromFile("DOES_NOT_EXISTS", &DUMMY::TestDDF_Simple_DESCRIPTOR, &message);
    EXPECT_EQ(DDF_ERROR_IO_ERROR, e);
}

TEST(ScalarTypes, Types)
{
    // Test descriptor
    const SDDFDescriptor& d = DUMMY::TestDDF_ScalarTypes_DESCRIPTOR;
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

    enum DDFType types[] =
    {
        DDF_TYPE_FLOAT,
        DDF_TYPE_DOUBLE,
        DDF_TYPE_INT32,
        DDF_TYPE_UINT32,
        DDF_TYPE_INT64,
        DDF_TYPE_UINT64,
        DDF_TYPE_STRING,
    };

    for (int i = 0; i < d.m_FieldCount; ++i)
    {
        const SDDFFieldDescriptor* f = &d.m_Fields[i];
        EXPECT_STREQ(names[i], f->m_Name);
        EXPECT_EQ(i+1, f->m_Number);
        EXPECT_EQ(types[i], f->m_Type);
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

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_ScalarTypes_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::ScalarTypes* msg = (DUMMY::TestDDF::ScalarTypes*) message;
    EXPECT_EQ(scalar_types.float_(), msg->m_Float);
    EXPECT_EQ(scalar_types.double_(), msg->m_Double);
    EXPECT_EQ(scalar_types.int32(), msg->m_Int32);
    EXPECT_EQ(scalar_types.uint32(), msg->m_Uint32);
    EXPECT_EQ(scalar_types.int64(), msg->m_Int64);
    EXPECT_EQ(scalar_types.uint64(), msg->m_Uint64);
    EXPECT_STREQ(scalar_types.string().c_str(), msg->m_String);

    DDFFreeMessage(message);
}

TEST(Simple01Repeated, Load)
{
    const int count = 10;

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

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Simple01Repeated_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::Simple01Repeated* msg = (DUMMY::TestDDF::Simple01Repeated*) message;
    EXPECT_EQ(count, msg->m_array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(repated.array(i).x(), msg->m_array.m_Data[i].m_x);
        EXPECT_EQ(repated.array(i).y(), msg->m_array.m_Data[i].m_y);
    }

    DDFFreeMessage(message);
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

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Simple02Repeated_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::Simple02Repeated* msg = (DUMMY::TestDDF::Simple02Repeated*) message;
    EXPECT_EQ(count, msg->m_array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(repated.array(i), msg->m_array.m_Data[i]);
    }

    DDFFreeMessage(message);
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

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_StringRepeated_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::StringRepeated* msg = (DUMMY::TestDDF::StringRepeated*) message;
    EXPECT_EQ(count, msg->m_array.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_STREQ(repated.array(i).c_str(), msg->m_array.m_Data[i]);
    }

    DDFFreeMessage(message);
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

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_NestedMessage_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);
    DUMMY::TestDDF::NestedMessage* msg = (DUMMY::TestDDF::NestedMessage*) message;

    EXPECT_EQ(n1.x(), msg->m_n1.m_x);
    EXPECT_EQ(n2.x(), msg->m_n2.m_x);

    DDFFreeMessage(message);
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

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Mesh_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::Mesh* msg = (DUMMY::TestDDF::Mesh*) message;
    EXPECT_EQ(count, msg->m_PrimitiveCount);
    EXPECT_STREQ(mesh.name().c_str(), msg->m_Name);
    EXPECT_EQ((uint32_t) mesh.primitivetype(), (uint32_t) msg->m_PrimitiveType);

    for (int i = 0; i < count * 3; ++i)
    {
        EXPECT_EQ(mesh.vertices(i), msg->m_Vertices.m_Data[i]);
        EXPECT_EQ(mesh.indices(i), msg->m_Indices.m_Data[i]);
    }

    DDFFreeMessage(message);
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

    DDFError e = DDFLoadMessage((void*) pb_msg_buf, pb_msg_buf_size, &DUMMY::TestDDF_NestedArray_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);
    DUMMY::TestDDF::NestedArray* nested = (DUMMY::TestDDF::NestedArray*) message;

    EXPECT_EQ(count1, nested->m_array1.m_Count);
    ASSERT_EQ(pb_nested.d(), nested->m_d);
    ASSERT_EQ(pb_nested.e(), nested->m_e);

    for (int i = 0; i < count1; ++i)
    {
        EXPECT_EQ(count2, nested->m_array1.m_Data[i].m_array2.m_Count);
        ASSERT_EQ(pb_nested.array1(i).b(), nested->m_array1.m_Data[i].m_b);
        ASSERT_EQ(pb_nested.array1(i).c(), nested->m_array1.m_Data[i].m_c);
        for (int j = 0; j < count2; ++j)
        {
            ASSERT_EQ(pb_nested.array1(i).array2(j).a(), nested->m_array1.m_Data[i].m_array2.m_Data[j].m_a);
        }
    }

    DDFFreeMessage(message);
}


TEST(Bytes, Load)
{
    TestDDF::Bytes bytes;
    bytes.set_data((void*) "foo", 3);
    std::string msg_str = bytes.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Bytes_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::Bytes* msg = (DUMMY::TestDDF::Bytes*) message;
    ASSERT_EQ(3, msg->m_data.m_Count);
    ASSERT_EQ('f', msg->m_data[0]);
    ASSERT_EQ('o', msg->m_data[1]);
    ASSERT_EQ('o', msg->m_data[2]);

    DDFFreeMessage(message);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
