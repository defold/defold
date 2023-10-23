
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

#include "modelimporter.h"
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <string.h>


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static void* BufferResolveUri(const char* dirname, const char* uri, uint32_t* file_size)
{
    char path[512];
    dmStrlCpy(path, dirname, sizeof(path));
    dmStrlCat(path, "/", sizeof(path));
    dmStrlCat(path, uri, sizeof(path));

    return dmModelImporter::ReadFile(path, file_size);
}

static dmModelImporter::Scene* LoadScene(const char* path, dmModelImporter::Options& options)
{
    uint32_t file_size = 0;
    void* mem = dmModelImporter::ReadFile(path, &file_size);
    if (!mem)
        return 0;

    const char* suffix = strrchr(path, '.') + 1;

    char dirname[512];
    dmStrlCpy(dirname, path, sizeof(dirname));
    char* c = strrchr(dirname, '/');
    if (!c)
        c = strrchr(dirname, '\\');
    if (c)
        *c = 0;

    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, suffix, mem, file_size);

    if (dmModelImporter::NeedsResolve(scene))
    {
        for (uint32_t i = 0; i < scene->m_BuffersCount; ++i)
        {
            if (scene->m_Buffers[i].m_Buffer)
                continue;

            uint32_t buffermem_size = 0;
            void* buffermem = BufferResolveUri(dirname, scene->m_Buffers[i].m_Uri, &buffermem_size);
            dmModelImporter::ResolveBuffer(scene, scene->m_Buffers[i].m_Uri, buffermem, buffermem_size);
            free(buffermem);
        }

        assert(!dmModelImporter::NeedsResolve(scene));
    }

    bool result = dmModelImporter::LoadFinalize(scene);
    if (result)
        result = dmModelImporter::Validate(scene);

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
    bool result = dmModelImporter::LoadFinalize(scene);
    ASSERT_TRUE(result);
    result = dmModelImporter::Validate(scene);
    ASSERT_TRUE(result);

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

// See https://github.com/KhronosGroup/glTF-Asset-Generator for assets
// https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/master/2.0/VertexColorTest/glTF-Embedded/VertexColorTest.gltf

TEST(ModelGLTF, VertexColor3Float)
{
    // Model courtesy of Artsion Trubchyk (https://github.com/aglitchman), public domain
    const char* path = "./src/test/assets/primitive_vertex_color/vertexcolor_rgb3.glb";
    uint32_t file_size = 0;
    void* mem = dmModelImporter::ReadFile(path, &file_size);
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, strrchr(path, '.')+1, mem, file_size);

    dmModelImporter::Mesh* mesh = &scene->m_Models[0].m_Meshes[0];
    uint32_t vcount = mesh->m_VertexCount;
    ASSERT_EQ(4112, vcount);
    ASSERT_EQ(1.0, mesh->m_Color[vcount*4-1]); // vN.a == 1.0f
    ASSERT_EQ(1.0, mesh->m_Color[3]); // v0.a == 1.0f
    dmModelImporter::DestroyScene(scene);
    free(mem);
}

TEST(ModelGLTF, ExternalBuffer)
{
    const char* path = "./src/test/assets/triangle/gltf/Triangle.gltf";
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);

    ASSERT_EQ(1, scene->m_BuffersCount);
    ASSERT_STREQ("simpleTriangle.bin", scene->m_Buffers[0].m_Uri);

    ASSERT_EQ(1, scene->m_NodesCount);
    ASSERT_EQ(1, scene->m_ModelsCount);

    dmModelImporter::Mesh* mesh = &scene->m_Models[0].m_Meshes[0];
    uint32_t vcount = mesh->m_VertexCount;
    ASSERT_EQ(3, vcount);

    dmModelImporter::DestroyScene(scene);
}

// Some tests are simply loading the file to make sure it doesn't crash

static dmModelImporter::Scene* TestLoading(const char* path)
{
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);
    return scene;
}

// #7369 Find skin crash
TEST(ModelCrashtest, FindSkinCrash)
{
    dmModelImporter::Scene* scene = TestLoading("./src/test/assets/findskin/findskin_crash.glb");
    ASSERT_NE((dmModelImporter::Scene*)0, scene);

    ASSERT_EQ(1, scene->m_BuffersCount);
    ASSERT_STREQ("buffer_0", scene->m_Buffers[0].m_Uri);

    ASSERT_EQ(1, scene->m_SkinsCount);
    ASSERT_STREQ("skin_0", scene->m_Skins[0].m_Name);

    dmModelImporter::DestroyScene(scene);
}

// #8038 More than one skinned model
TEST(ModelSkinnedTopNodes, MultipleModels)
{
    dmModelImporter::Scene* scene = TestLoading("./src/test/assets/kay/Knight.glb");
    ASSERT_NE((dmModelImporter::Scene*)0, scene);

    ASSERT_EQ(1, scene->m_SkinsCount);
    ASSERT_STREQ("Rig", scene->m_Skins[0].m_Name);

    ASSERT_EQ(57, scene->m_NodesCount);
    ASSERT_EQ(1, scene->m_RootNodesCount);
    ASSERT_STREQ("Rig", scene->m_RootNodes[0]->m_Name);

    ASSERT_EQ(15, scene->m_ModelsCount);

    uint32_t num_non_skinned_models = 0;
    for (uint32_t i = 0; i < scene->m_ModelsCount; ++i)
    {
        dmModelImporter::Model* model = &scene->m_Models[i];
        if (model->m_ParentBone)
        {
            num_non_skinned_models++;

            ASSERT_TRUE( strcmp("handslot.r", model->m_ParentBone->m_Name) == 0 ||
                         strcmp("handslot.l", model->m_ParentBone->m_Name) == 0 ||
                         strcmp("head", model->m_ParentBone->m_Name) == 0 ||
                         strcmp("chest", model->m_ParentBone->m_Name) == 0 );
        }
    }

    ASSERT_EQ(9, num_non_skinned_models);

    dmModelImporter::DestroyScene(scene);
}


static int TestStandalone(const char* path)
{
    uint64_t tstart = dmTime::GetTime();

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);

    if (!scene)
        return 1;

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
