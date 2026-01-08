// Copyright 2020-2026 The Defold Foundation
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
#include <dlib/log.h>
#include <dlib/time.h>
#include <string.h>


#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

int g_AssertMode = 1; // 1 for unit tests

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
    if (!scene)
    {
        dmLogError("Failed to load scene '%s'", path);
        return 0;
    }

    if (dmModelImporter::NeedsResolve(scene))
    {
        for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
        {
            if (scene->m_Buffers[i].m_Buffer)
                continue;
            if (!scene->m_Buffers[i].m_Uri)
                continue;

            uint32_t buffermem_size = 0;
            void* buffermem = BufferResolveUri(dirname, scene->m_Buffers[i].m_Uri, &buffermem_size);
            dmModelImporter::ResolveBuffer(scene, scene->m_Buffers[i].m_Uri, buffermem, buffermem_size);
            free(buffermem);
        }

        bool still_needs_resolve = dmModelImporter::NeedsResolve(scene);
        if (still_needs_resolve)
        {
            dmLogError("There are still unresolved buffers");
        }

        if (g_AssertMode)
        {
            assert(!still_needs_resolve);
        }

        if (still_needs_resolve)
        {
            dmModelImporter::DestroyScene(scene);
            free(mem);
            return 0;
        }
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
    ASSERT_FALSE(dmModelImporter::NeedsResolve(scene));
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
    ASSERT_EQ(1, scene->m_Skins.Size());

    dmModelImporter::Skin* skin = &scene->m_Skins[0];
    ASSERT_EQ(46, skin->m_Bones.Size());

    // The first bone is generated
    uint32_t num_root_bones = 1; // Since we're skipping the first root node
    for (uint32_t i = 1; i < skin->m_Bones.Size(); ++i)
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
    ASSERT_EQ(1.0, mesh->m_Colors[vcount*4-1]); // vN.a == 1.0f
    ASSERT_EQ(1.0, mesh->m_Colors[3]); // v0.a == 1.0f
    dmModelImporter::DestroyScene(scene);
    free(mem);
}

TEST(ModelGLTF, ExternalBuffer)
{
    const char* path = "./src/test/assets/triangle/gltf/Triangle.gltf";
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);

    ASSERT_EQ(1, scene->m_Buffers.Size());
    ASSERT_STREQ("simpleTriangle.bin", scene->m_Buffers[0].m_Uri);

    ASSERT_EQ(1, scene->m_Nodes.Size());
    ASSERT_EQ(1, scene->m_Models.Size());

    dmModelImporter::Mesh* mesh = &scene->m_Models[0].m_Meshes[0];
    uint32_t vcount = mesh->m_VertexCount;
    ASSERT_EQ(3, vcount);

    dmModelImporter::DestroyScene(scene);
}

static void CheckChildren(dmModelImporter::Node* n, uint32_t num_children, const char** child_names)
{
    ASSERT_EQ(num_children, n->m_Children.Size());

    for (uint32_t i = 0; i < num_children; ++i)
    {
        dmModelImporter::Node* child = n->m_Children[i];
        ASSERT_STREQ(child_names[i], child->m_Name);
    }
}

TEST(ModelGLTF, GeneratedBone01)
{
    const char* path = "./src/test/assets/generatedbone01.glb";
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);

    ASSERT_EQ(5, scene->m_Nodes.Size());
    ASSERT_EQ(1, scene->m_Models.Size());
    ASSERT_EQ(1, scene->m_RootNodes.Size());

    ASSERT_STREQ("cube", scene->m_RootNodes[0]->m_Name);
    ASSERT_EQ(3, scene->m_RootNodes[0]->m_Children.Size());

    {
        const char* names[] = {
            "Cube",
            "root1",
            "root2"
        };
        CheckChildren(scene->m_RootNodes[0], 3, names);
    }

    ASSERT_STREQ("Cube", scene->m_Models[0].m_Name);

    ASSERT_STREQ("root1", scene->m_Nodes[0].m_Name);
    ASSERT_STREQ("root2", scene->m_Nodes[1].m_Name);
    ASSERT_STREQ("Cube", scene->m_Nodes[2].m_Name);
    ASSERT_STREQ("cube", scene->m_Nodes[3].m_Name);
    ASSERT_STREQ("_generated_node_4", scene->m_Nodes[4].m_Name);

    ASSERT_EQ(1, scene->m_Skins.Size());

    dmModelImporter::Skin* skin = &scene->m_Skins[0];
    ASSERT_STREQ("cube", skin->m_Name);

    ASSERT_EQ(3, skin->m_Bones.Size());
    ASSERT_STREQ("_generated_root", skin->m_Bones[0].m_Name);
    ASSERT_STREQ("root1", skin->m_Bones[1].m_Name);
    ASSERT_STREQ("root2", skin->m_Bones[2].m_Name);

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

    ASSERT_EQ(1, scene->m_Buffers.Size());
    ASSERT_EQ(0U, scene->m_Buffers[0].m_Uri);

    ASSERT_EQ(1, scene->m_Skins.Size());
    ASSERT_STREQ("skin_0", scene->m_Skins[0].m_Name);

    dmModelImporter::DestroyScene(scene);
}

// #8038 More than one skinned model
TEST(ModelSkinnedTopNodes, MultipleModels)
{
    dmModelImporter::Scene* scene = TestLoading("./src/test/assets/kay/Knight.glb");
    ASSERT_NE((dmModelImporter::Scene*)0, scene);

    ASSERT_EQ(1, scene->m_Skins.Size());
    ASSERT_STREQ("Rig", scene->m_Skins[0].m_Name);

    ASSERT_EQ(57, scene->m_Nodes.Size());
    ASSERT_EQ(1, scene->m_RootNodes.Size());
    ASSERT_STREQ("Rig", scene->m_RootNodes[0]->m_Name);

    ASSERT_EQ(15, scene->m_Models.Size());

    uint32_t num_non_skinned_models = 0;
    for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
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

    ASSERT_EQ(1u, scene->m_Materials.Size());
    ASSERT_EQ(1u, scene->m_DynamicMaterials.Size());

    dmModelImporter::Material* material = &scene->m_Materials[0];
    ASSERT_STREQ("knight_texture", material->m_Name);
    ASSERT_TRUE(material->m_IsSkinned);

    dmModelImporter::Material* dynmaterial = scene->m_DynamicMaterials[0];
    ASSERT_STREQ("knight_texture_no_skin", dynmaterial->m_Name);
    ASSERT_FALSE(dynmaterial->m_IsSkinned);

#define CHECKPROP(DNAME) \
    if ((material->m_ ## DNAME && !dynmaterial->m_ ## DNAME) || (!material->m_ ## DNAME && dynmaterial->m_ ## DNAME)) { \
        ASSERT_FALSE(true); \
    } \
    if (material->m_ ## DNAME && dynmaterial->m_ ## DNAME) { \
        printf("Testing m_" # DNAME "\n"); \
        ASSERT_ARRAY_EQ_LEN((uint8_t*)material->m_ ## DNAME, (uint8_t*)dynmaterial->m_ ## DNAME, sizeof(dmModelImporter:: DNAME)); \
    }

    CHECKPROP(PbrMetallicRoughness);
    CHECKPROP(PbrSpecularGlossiness);
    CHECKPROP(Clearcoat);
    CHECKPROP(Ior);
    CHECKPROP(Specular);
    CHECKPROP(Sheen);
    CHECKPROP(Transmission);
    CHECKPROP(Volume);
    CHECKPROP(EmissiveStrength);
    CHECKPROP(Iridescence);

#undef CHECKPROP

    ASSERT_ARRAY_EQ(material->m_EmissiveFactor, dynmaterial->m_EmissiveFactor);

    ASSERT_EQ(material->m_AlphaCutoff, dynmaterial->m_AlphaCutoff);
    ASSERT_EQ(material->m_AlphaMode, dynmaterial->m_AlphaMode);
    ASSERT_EQ(material->m_DoubleSided, dynmaterial->m_DoubleSided);
    ASSERT_EQ(material->m_Unlit, dynmaterial->m_Unlit);

    dmModelImporter::DestroyScene(scene);
}


static int TestStandalone(const char* path)
{
    g_AssertMode = 0;
    uint64_t tstart = dmTime::GetMonotonicTime();

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene(path, options);

    if (!scene)
        return 1;

    uint64_t tend = dmTime::GetMonotonicTime();
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
