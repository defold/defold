// Copyright 2020-2023 The Defold Foundation
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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dlib/array.h>
#include <dlib/sys.h>

#include "../atlasc.h"
#include "../atlasc_private.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

static void DestroyImage(dmAtlasc::SourceImage* image)
{
    free((void*)image->m_Path);
    free((void*)image->m_Data);
}

static int LoadImage(const char* path, dmAtlasc::SourceImage* image)
{
    memset(image, 0, sizeof(dmAtlasc::SourceImage));
    image->m_Data = stbi_load(path, &image->m_Size.x, &image->m_Size.y, &image->m_NumChannels, 0);
    if (!image->m_Data)
    {
        DestroyImage(image);
        printf("Failed to load %s\n", path);
        return 0;
    }
    image->m_Path = strdup(path);
    return 1;
}

static void TreeIterCallback(void* ctx, const char* path, bool isdir)
{
    if (isdir)
        return;

    const char* ext = strrchr(path, '.');
    if (!ext)
        return;

    bool is_image = strcmp(ext, ".png") == 0;
    if (!is_image)
        return;

    dmArray<dmAtlasc::SourceImage>* images = (dmArray<dmAtlasc::SourceImage>*)ctx;
    dmAtlasc::SourceImage image;
    int result = LoadImage(path, &image);
    if (result)
    {
        if (images->Full())
            images->OffsetCapacity(16);
        images->Push(image);
    }
}

struct CompileInfo
{
    const char*                 m_DirPath;
    dmAtlasc::PackingAlgorithm  m_PackingAlgorithm;
    int                         m_PageSize;

    // expected results
    uint32_t                    m_ExpectedWidth;
    uint32_t                    m_ExpectedHeight;
    uint32_t                    m_ExpectedPageCount;
};

static CompileInfo compile_info[] =
{
    {"src/test/data/atlas01", dmAtlasc::PA_BINPACK_SKYLINE_BL, 0, 512, 256},
    {"src/test/data/atlas01", dmAtlasc::PA_TILEPACK_AUTO, 0, 512, 256},
    {"src/test/data/atlas01", dmAtlasc::PA_TILEPACK_CONVEXHULL, 0, 512, 256},
};

class AtlascCompileTest : public jc_test_params_class<CompileInfo>
{
protected:
    virtual void SetUp()
    {
        const CompileInfo& info = GetParam();

        dmSys::IterateTree(info.m_DirPath, true, true, &m_Images, TreeIterCallback);
        ASSERT_NE(0U, m_Images.Size());
    }

    virtual void TearDown()
    {
        for (uint32_t i = 0; i < m_Images.Size(); ++i)
        {
            DestroyImage(&m_Images[i]);
        }
    }

    dmArray<dmAtlasc::SourceImage> m_Images;
    int m_Width;
    int m_Height;
};

TEST_P(AtlascCompileTest, Pack)
{
    const CompileInfo& info = GetParam();

    dmAtlasc::Options options;
    options.m_Algorithm = (dmAtlasc::PackingAlgorithm)info.m_PackingAlgorithm;
    options.m_PageSize = info.m_PageSize;
    dmAtlasc::Atlas* atlas = dmAtlasc::CreateAtlas(options, m_Images.Begin(), m_Images.Size());

    dmAtlasc::DestroyAtlas(atlas);

    //ASSERT_TRUE(dmTexc::Encode(m_Texture, info.m_OutputFormat, info.m_ColorSpace, dmTexc::CL_BEST, info.m_CompressionType, true, 1));
}

INSTANTIATE_TEST_CASE_P(AtlascCompileTest, AtlascCompileTest, jc_test_values_in(compile_info));

static int TestStandAlone(const char* dirpath)
{
    dmArray<dmAtlasc::SourceImage> images;
    dmSys::IterateTree(dirpath, true, true, &images, TreeIterCallback);

    if (images.Empty())
    {
        printf("Found no images in %s\n", dirpath);
        return 1;
    }

    dmAtlasc::Options options;
    options.m_Algorithm = dmAtlasc::PA_TILEPACK_AUTO;
    options.m_PageSize = 1024;
    dmAtlasc::Atlas* atlas = dmAtlasc::CreateAtlas(options, images.Begin(), images.Size());
    if (!atlas)
    {
        printf("Failed to create atlas from path %s\n", dirpath);
        return 1;
    }

    DebugPrintAtlas(atlas);

    dmAtlasc::DestroyAtlas(atlas);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        return TestStandAlone(argv[1]);
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
