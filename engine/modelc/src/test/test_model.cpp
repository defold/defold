
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

#include "modelimporter.h"
#include <dlib/time.h>
#include <string.h>


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static dmModelImporter::Scene* LoadScene(const char* path, dmModelImporter::Options& options)
{
    uint32_t file_size = 0;
    void* mem = dmModelImporter::ReadFile(path, &file_size);
    if (!mem)
        return 0;

    const char* suffix = strrchr(path, '.') + 1;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, mem, file_size);

    free(mem);

    return scene;
}

TEST(ModelGLTF, Load)
{
    const char* path = "./src/test/assets/car01.glb";
    uint32_t file_size = 0;
    void* mem = dmModelImporter::ReadFile(path, &file_size);

    const char* suffix = strrchr(path, '.') + 1;

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, mem, file_size);

    ASSERT_NE((void*)0, scene);

    //dmModelImporter::DebugScene(scene);

    dmModelImporter::DestroyScene(scene);

    free(mem);
}

TEST(ModelGLTF, LoadSkeleton)
{
    const char* path = "./src/test/assets/skeleton1.gltf";
    uint32_t file_size = 0;
    void* mem = dmModelImporter::ReadFile(path, &file_size);

    const char* suffix = strrchr(path, '.') + 1;

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, mem, file_size);

    ASSERT_NE((void*)0, scene);
    ASSERT_EQ(1, scene->m_SkinsCount);

    dmModelImporter::Skin* skin = &scene->m_Skins[0];
    ASSERT_EQ(46, skin->m_BonesCount);

    // The first bone is generated, and doesn't have a node
    uint32_t num_root_bones = 1; // Since we're skipping the first root node
    for (uint32_t i = 1; i < skin->m_BonesCount; ++i)
    {
        dmModelImporter::Bone* bone = &skin->m_Bones[i];
        ASSERT_STREQ(bone->m_Name, bone->m_Node->m_Name);

        if (bone->m_ParentIndex == dmModelImporter::INVALID_INDEX)
            ++num_root_bones;
    }

    ASSERT_EQ(1U, num_root_bones);

    //dmModelImporter::DebugScene(scene);

    dmModelImporter::DestroyScene(scene);

    free(mem);
}


static int TestStandalone(const char* path)
{
    uint64_t tstart = dmTime::GetTime();

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);

    uint64_t tend = dmTime::GetTime();
    printf("Model %s loaded in %.3f seconds.\n", path, float(tend-tstart)/1000000.0f);

    dmModelImporter::DebugScene(scene);

    dmModelImporter::DestroyScene(scene);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strstr(argv[1], ".gltf") != 0 ||
                     strstr(argv[1], ".glb") != 0))
    {
        return TestStandalone(argv[1]);
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

