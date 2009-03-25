#include <stdint.h>
#include <string>
#include <gtest/gtest.h>

#include "ddf.h"

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
//#define TEST_FILES_ROOT "build/default/src/test/data"

#define BUFFER_SIZE_MAX (1024*1024)

uint32_t g_BufferSize = 0;
char *g_Buffer[BUFFER_SIZE_MAX];

static bool LoadFile(const char* file_name)
{
    FILE* f = fopen(file_name, "rb");

    if (f)
    {
        g_BufferSize = fread(g_Buffer, 1, BUFFER_SIZE_MAX, f);
        fclose(f);
        return true;
    }
    else
    {

        return false;
    }
}

TEST(Simple, Descriptor)
{
    // Test descriptor
    const SDDFDescriptor& d = DUMMY::TestDDF_Simple_DESCRIPTOR;
    EXPECT_STREQ("Simple", d.m_Name);
    EXPECT_EQ(4, d.m_Size);
    EXPECT_EQ(4, d.m_Align);
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

TEST(Mesh, Load)
{
    const int count = 10;

    TestDDF::Mesh mesh;
    for (int i = 0; i < count; ++i)
    {
        mesh.add_vertices((float) i*10);
    }
    mesh.set_stride(3);

    std::string msg_str = mesh.SerializeAsString();
    const char* msg_buf = msg_str.c_str();
    uint32_t msg_buf_size = msg_str.size();
    void* message;

    DDFError e = DDFLoadMessage((void*) msg_buf, msg_buf_size, &DUMMY::TestDDF_Mesh_DESCRIPTOR, &message);
    ASSERT_EQ(DDF_ERROR_OK, e);

    DUMMY::TestDDF::Mesh* msg = (DUMMY::TestDDF::Mesh*) message;
    EXPECT_EQ(count, msg->m_vertices.m_Count);

    for (int i = 0; i < count; ++i)
    {
        EXPECT_EQ(mesh.vertices(i), msg->m_vertices.m_Data[i]);
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
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
