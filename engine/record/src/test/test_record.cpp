#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <gtest/gtest.h>
#include "../record/record.h"

TEST(dmRecord, InvalidWidth1)
{
    dmRecord::NewParams params;
    params.m_Width = 1281;
    params.m_Height = 720;
    params.m_Filename = "tmp/test.ivf";
    dmRecord::HRecorder recorder = 0;
    dmRecord::Result r = dmRecord::New(&params, &recorder);
    ASSERT_EQ(dmRecord::RESULT_INVAL_ERROR, r);
    ASSERT_EQ(0, recorder);
}

TEST(dmRecord, InvalidWidth2)
{
    dmRecord::NewParams params;
    params.m_Width = 0;
    params.m_Height = 720;
    params.m_Filename = "tmp/test.ivf";
    dmRecord::HRecorder recorder = 0;
    dmRecord::Result r = dmRecord::New(&params, &recorder);
    ASSERT_EQ(dmRecord::RESULT_INVAL_ERROR, r);
    ASSERT_EQ(0, recorder);
}

TEST(dmRecord, InvalidHeight1)
{
    dmRecord::NewParams params;
    params.m_Width = 1280;
    params.m_Height = 721;
    params.m_Filename = "tmp/test.ivf";
    dmRecord::HRecorder recorder = 0;
    dmRecord::Result r = dmRecord::New(&params, &recorder);
    ASSERT_EQ(dmRecord::RESULT_INVAL_ERROR, r);
    ASSERT_EQ(0, recorder);
}

TEST(dmRecord, InvalidHeight2)
{
    dmRecord::NewParams params;
    params.m_Width = 1280;
    params.m_Height = 0;
    params.m_Filename = "tmp/test.ivf";
    dmRecord::HRecorder recorder = 0;
    dmRecord::Result r = dmRecord::New(&params, &recorder);
    ASSERT_EQ(dmRecord::RESULT_INVAL_ERROR, r);
    ASSERT_EQ(0, recorder);
}

TEST(dmRecord, EmptyRecording)
{
    dmRecord::NewParams params;
    params.m_Width = 1280;
    params.m_Height = 720;
    params.m_Filename = "tmp/test.ivf";
    dmRecord::HRecorder recorder = 0;
    dmRecord::Result r = dmRecord::New(&params, &recorder);
    ASSERT_EQ(dmRecord::RESULT_OK, r);

    r = dmRecord::Delete(recorder);
    ASSERT_EQ(dmRecord::RESULT_OK, r);
}

TEST(dmRecord, Simple)
{
    dmRecord::NewParams params;
    params.m_Width = 1280;
    params.m_Height = 720;
    params.m_Filename = "tmp/simple.ivf";
    dmRecord::HRecorder recorder = 0;
    dmRecord::Result r = dmRecord::New(&params, &recorder);
    ASSERT_EQ(dmRecord::RESULT_OK, r);

    uint32_t buffer_size_ints = params.m_Width * params.m_Height;
    uint32_t buffer_size_bytes = params.m_Width * params.m_Height * sizeof(uint32_t);
    uint32_t *buffer = new uint32_t[buffer_size_ints];
    for (uint32_t i = 0; i < 64; ++i)
    {
        // Rectangle two pixel per frame to the right
        for (uint32_t y = 0; y < params.m_Height; ++y)
        {
            for (uint32_t x = 0; x < params.m_Width; ++x)
            {
                if (x >= i*2 && x < (i*2 + 128) && y < 128)
                {
                    buffer[y * params.m_Width + x] = 0x0000ff00;
                }
                else
                {
                    buffer[y * params.m_Width + x] = 0;
                }
            }
        }
        r = dmRecord::RecordFrame(recorder, buffer, buffer_size_bytes,
                dmRecord::BUFFER_FORMAT_BGRA);
        ASSERT_EQ(dmRecord::RESULT_OK, r);
    }

    delete[] buffer;
    r = dmRecord::Delete(recorder);
    ASSERT_EQ(dmRecord::RESULT_OK, r);
}

int main(int argc, char **argv)
{
#if !defined(DM_NO_SYSTEM)
    system("python src/test/test_record.py");
#endif
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

