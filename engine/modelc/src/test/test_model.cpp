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

    if (scene->m_LoadError && scene->m_LoadError[0])
    {
        dmLogError("%s", scene->m_LoadError);
        dmModelImporter::DestroyScene(scene);
        free(mem);
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

static dmModelImporter::Scene* LoadGltfJson(const char* json)
{
    dmModelImporter::Options options;
    return dmModelImporter::LoadFromBuffer(&options, "gltf", (void*)json, (uint32_t)strlen(json));
}

static void AssertGltfLoadError(const char* json, const char* expected_error)
{
    dmModelImporter::Scene* scene = LoadGltfJson(json);
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_NE((char*)0, scene->m_LoadError);
    ASSERT_NE((char*)0, strstr(scene->m_LoadError, expected_error));
    ASSERT_EQ(0U, scene->m_Models.Size());
    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, RejectsInconsistentPrimitiveAccessorCountsBeforeAllocation)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            "{\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1}}]}]"
        "}";

    AssertGltfLoadError(json, "glTF validation failed");
}

TEST(ModelGLTF, RejectsIndexEqualToVertexCount)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":42,"
            "\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAMA\""
        "}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}"
        "],"
        "\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"indices\":1}]}]"
        "}";

    AssertGltfLoadError(json, "cgltf_result_data_too_short");
}

TEST(ModelGLTF, RejectsInconsistentAnimationAccessorCountsBeforeAllocation)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"nodes\":[{}],"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"}"
        "],"
        "\"animations\":[{"
            "\"samplers\":[{\"input\":0,\"output\":1}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]"
        "}]"
        "}";

    AssertGltfLoadError(json, "glTF validation failed");
}

TEST(ModelGLTF, RejectsShortInverseBindMatricesAccessor)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"nodes\":[{},{}],"
        "\"accessors\":[{\"componentType\":5126,\"count\":1,\"type\":\"MAT4\"}],"
        "\"skins\":[{\"joints\":[0,1],\"inverseBindMatrices\":0}]"
        "}";

    AssertGltfLoadError(json, "fewer elements than the skin has joints");
}

TEST(ModelGLTF, RejectsWrongInverseBindMatricesAccessorType)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"nodes\":[{}],"
        "\"accessors\":[{\"componentType\":5126,\"count\":1,\"type\":\"VEC4\"}],"
        "\"skins\":[{\"joints\":[0],\"inverseBindMatrices\":0}]"
        "}";

    AssertGltfLoadError(json, "must contain MAT4 floats");
}

TEST(ModelGLTF, RejectsSparseInverseBindMatricesAccessor)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":68,"
            "\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=\""
        "}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":1},"
            "{\"buffer\":0,\"byteOffset\":4,\"byteLength\":64}"
        "],"
        "\"accessors\":[{"
            "\"componentType\":5126,\"count\":1,\"type\":\"MAT4\","
            "\"sparse\":{"
                "\"count\":1,"
                "\"indices\":{\"bufferView\":0,\"componentType\":5121},"
                "\"values\":{\"bufferView\":1}"
            "}"
        "}],"
        "\"nodes\":[{}],"
        "\"skins\":[{\"joints\":[0],\"inverseBindMatrices\":0}]"
        "}";

    AssertGltfLoadError(json, "inverse bind matrices accessor must not be sparse");
}

TEST(ModelGLTF, MaterialsOnlySkipsGeometryImport)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":[{\"componentType\":5126,\"count\":1,\"type\":\"VEC2\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
        "\"materials\":[{\"name\":\"Material\"}]"
        "}";

    dmModelImporter::Options options;
    options.m_LoadMaterialsOnly = true;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, "gltf", (void*)json, (uint32_t)strlen(json));
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_EQ(1U, scene->m_Materials.Size());
    ASSERT_STREQ("Material", scene->m_Materials[0].m_Name);
    ASSERT_EQ(0U, scene->m_Models.Size());
    ASSERT_EQ(0U, scene->m_Nodes.Size());
    ASSERT_EQ(0U, scene->m_Skins.Size());
    ASSERT_EQ(0U, scene->m_Animations.Size());
    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, MaterialsOnlyCanLoadMeshMetadataWithoutGeometry)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            "{\"componentType\":5126,\"count\":4,\"type\":\"VEC3\"}"
        "],"
        "\"meshes\":["
            "{\"name\":\"NamedMesh\",\"primitives\":["
                "{\"attributes\":{\"POSITION\":0},\"material\":0},"
                "{\"attributes\":{\"NORMAL\":1},\"material\":1,\"mode\":1}"
            "]},"
            "{\"primitives\":[{\"attributes\":{\"NORMAL\":1},\"mode\":0}]}"
        "],"
        "\"materials\":[{\"name\":\"First\"},{\"name\":\"Second\"}]"
        "}";

    dmModelImporter::Options options;
    options.m_LoadMaterialsOnly = true;
    options.m_LoadMeshMetadata = true;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, "gltf", (void*)json, (uint32_t)strlen(json));
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_EQ(2U, scene->m_Materials.Size());
    ASSERT_EQ(2U, scene->m_Models.Size());

    dmModelImporter::Model* named_model = &scene->m_Models[0];
    ASSERT_STREQ("NamedMesh", named_model->m_Name);
    ASSERT_EQ(0U, named_model->m_Index);
    ASSERT_FALSE(named_model->m_NameIsGenerated);
    ASSERT_EQ(2U, named_model->m_Meshes.Size());
    ASSERT_EQ(3U, named_model->m_Meshes[0].m_VertexCount);
    ASSERT_EQ(dmModelImporter::PRIMITIVE_TYPE_TRIANGLES, named_model->m_Meshes[0].m_PrimitiveType);
    ASSERT_EQ(&scene->m_Materials[0], named_model->m_Meshes[0].m_Material);
    ASSERT_EQ(0U, named_model->m_Meshes[0].m_Positions.Size());
    ASSERT_EQ(0U, named_model->m_Meshes[0].m_Indices.Size());
    ASSERT_EQ(4U, named_model->m_Meshes[1].m_VertexCount);
    ASSERT_EQ(dmModelImporter::PRIMITIVE_TYPE_LINES, named_model->m_Meshes[1].m_PrimitiveType);
    ASSERT_EQ(&scene->m_Materials[1], named_model->m_Meshes[1].m_Material);

    dmModelImporter::Model* generated_model = &scene->m_Models[1];
    ASSERT_STREQ("model_1", generated_model->m_Name);
    ASSERT_EQ(1U, generated_model->m_Index);
    ASSERT_TRUE(generated_model->m_NameIsGenerated);
    ASSERT_EQ(1U, generated_model->m_Meshes.Size());
    ASSERT_EQ(4U, generated_model->m_Meshes[0].m_VertexCount);
    ASSERT_EQ(dmModelImporter::PRIMITIVE_TYPE_POINTS, generated_model->m_Meshes[0].m_PrimitiveType);
    ASSERT_EQ((dmModelImporter::Material*)0, generated_model->m_Meshes[0].m_Material);

    ASSERT_EQ(0U, scene->m_Nodes.Size());
    ASSERT_EQ(0U, scene->m_Skins.Size());
    ASSERT_EQ(0U, scene->m_Animations.Size());
    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, MaterialsOnlyMeshMetadataIgnoresUnresolvedGeometryBuffers)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]"
        "}";

    dmModelImporter::Options options;
    options.m_LoadMaterialsOnly = true;
    options.m_LoadMeshMetadata = true;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, "gltf", (void*)json, (uint32_t)strlen(json));
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_FALSE(dmModelImporter::NeedsResolve(scene));
    ASSERT_EQ(1U, scene->m_Models.Size());
    ASSERT_EQ(1U, scene->m_Models[0].m_Meshes.Size());
    ASSERT_EQ(3U, scene->m_Models[0].m_Meshes[0].m_VertexCount);
    ASSERT_EQ(0U, scene->m_Models[0].m_Meshes[0].m_Positions.Size());
    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, RejectsSparseAnimationAccessors)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":24,"
            "\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\""
        "}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":1},"
            "{\"buffer\":0,\"byteOffset\":4,\"byteLength\":4},"
            "{\"buffer\":0,\"byteOffset\":8,\"byteLength\":1},"
            "{\"buffer\":0,\"byteOffset\":12,\"byteLength\":12}"
        "],"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":1,\"type\":\"SCALAR\",\"sparse\":{"
                "\"count\":1,"
                "\"indices\":{\"bufferView\":0,\"componentType\":5121},"
                "\"values\":{\"bufferView\":1}"
            "}},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\",\"sparse\":{"
                "\"count\":1,"
                "\"indices\":{\"bufferView\":2,\"componentType\":5121},"
                "\"values\":{\"bufferView\":3}"
            "}}"
        "],"
        "\"nodes\":[{}],"
        "\"animations\":[{"
            "\"samplers\":[{\"input\":0,\"output\":1}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]"
        "}]"
        "}";

    AssertGltfLoadError(json, "animation accessors must not be sparse");
}

TEST(ModelGLTF, RejectsUnsafePrimitiveAttributeShapes)
{
    const char* position_vec2 =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":[{\"componentType\":5126,\"count\":1,\"type\":\"VEC2\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]"
        "}";
    const char* texcoord_scalar =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"SCALAR\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1}}]}]"
        "}";
    const char* joints_vec2 =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"},"
            "{\"componentType\":5121,\"count\":1,\"type\":\"VEC2\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC4\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":2}}]}]"
        "}";
    const char* joints_uint32 =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"},"
            "{\"componentType\":5125,\"count\":1,\"type\":\"VEC4\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC4\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":2}}]}]"
        "}";

    AssertGltfLoadError(position_vec2, "POSITION accessor must be VEC3");
    AssertGltfLoadError(texcoord_scalar, "TEXCOORD accessor must be VEC2");
    AssertGltfLoadError(joints_vec2, "JOINTS accessor must be an unnormalized unsigned byte or unsigned short VEC4");
    AssertGltfLoadError(joints_uint32, "JOINTS accessor must be an unnormalized unsigned byte or unsigned short VEC4");
}

TEST(ModelGLTF, RejectsSkinnedPrimitiveWithoutJointAttributes)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":[{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
        "\"nodes\":[{\"mesh\":0,\"skin\":0},{}],"
        "\"skins\":[{\"joints\":[1]}]"
        "}";

    AssertGltfLoadError(json, "must contain JOINTS_0 and WEIGHTS_0 accessors");
}

TEST(ModelGLTF, RejectsOutOfRangeSkinJointIndex)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":32,"
            "\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAQAAAAAAgD8AAAAAAAAAAAAAAAA=\""
        "}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":12},"
            "{\"buffer\":0,\"byteOffset\":12,\"byteLength\":4},"
            "{\"buffer\":0,\"byteOffset\":16,\"byteLength\":16}"
        "],"
        "\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"},"
            "{\"bufferView\":1,\"componentType\":5121,\"count\":1,\"type\":\"VEC4\"},"
            "{\"bufferView\":2,\"componentType\":5126,\"count\":1,\"type\":\"VEC4\"}"
        "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"JOINTS_0\":1,\"WEIGHTS_0\":2}}]}],"
        "\"nodes\":[{\"mesh\":0,\"skin\":0},{}],"
        "\"skins\":[{\"joints\":[1]}]"
        "}";

    AssertGltfLoadError(json, "JOINTS_0 index exceeds the skin joint count");
}

TEST(ModelGLTF, RejectsZeroCountAccessor)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":[{\"componentType\":5126,\"count\":0,\"type\":\"SCALAR\"}]"
        "}";

    AssertGltfLoadError(json, "accessor has zero elements");
}

TEST(ModelGLTF, RejectsAccessorStrideOverflow)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":64,\"uri\":\"payload.bin\"}],"
        "\"bufferViews\":[{"
            "\"buffer\":0,\"byteLength\":64,\"byteStride\":9223372036854775804"
        "}],"
        "\"accessors\":[{"
            "\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\""
        "}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]"
        "}";

    AssertGltfLoadError(json, "accessor exceeds its referenced buffer view");
}

TEST(ModelGLTF, RejectsAccessorByteOffsetOutsideBufferView)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":12,\"uri\":\"payload.bin\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":12}],"
        "\"accessors\":[{"
            "\"bufferView\":0,\"byteOffset\":4,\"componentType\":5126,\"count\":1,\"type\":\"VEC3\""
        "}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]"
        "}";

    AssertGltfLoadError(json, "accessor exceeds its referenced buffer view");
}

TEST(ModelGLTF, RejectsMisalignedAccessorData)
{
    const char* accessor =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":8,\"uri\":\"payload.bin\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":1,\"byteLength\":7}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5123,\"count\":1,\"type\":\"SCALAR\"}]"
        "}";
    const char* sparse =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":8,\"uri\":\"payload.bin\"}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":1,\"byteLength\":2},"
            "{\"buffer\":0,\"byteOffset\":4,\"byteLength\":4}"
        "],"
        "\"accessors\":[{\"componentType\":5126,\"count\":1,\"type\":\"SCALAR\",\"sparse\":{"
            "\"count\":1,"
            "\"indices\":{\"bufferView\":0,\"componentType\":5123},"
            "\"values\":{\"bufferView\":1}"
        "}}]"
        "}";

    AssertGltfLoadError(accessor, "accessor offset or stride is not aligned");
    AssertGltfLoadError(sparse, "sparse accessor offsets are not aligned");
}

TEST(ModelGLTF, RejectsSparseAccessorExcessiveMaterialization)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":5,"
            "\"uri\":\"data:application/octet-stream;base64,AAAAAAA=\""
        "}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":1},"
            "{\"buffer\":0,\"byteOffset\":1,\"byteLength\":4}"
        "],"
        "\"accessors\":[{"
            "\"componentType\":5126,\"count\":100000000,\"type\":\"SCALAR\","
            "\"sparse\":{"
                "\"count\":1,"
                "\"indices\":{\"bufferView\":0,\"componentType\":5121},"
                "\"values\":{\"bufferView\":1}"
            "}"
        "}]"
        "}";

    AssertGltfLoadError(json, "accessor size exceeds the model importer limit");
}

TEST(ModelGLTF, SparseAccessorValuesAreTightlyPacked)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":60,"
            "\"uri\":\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAQAAAACAPwAAAEAAAEBAAACAQAAAoEAAAMBA\""
        "}],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":32,\"byteStride\":16},"
            "{\"buffer\":0,\"byteOffset\":32,\"byteLength\":2},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24}"
        "],"
        "\"accessors\":[{"
            "\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\","
            "\"sparse\":{"
                "\"count\":2,"
                "\"indices\":{\"bufferView\":1,\"componentType\":5121},"
                "\"values\":{\"bufferView\":2}"
            "}"
        "}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]"
        "}";

    dmModelImporter::Scene* scene = LoadGltfJson(json);
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_EQ(1U, scene->m_Models.Size());
    ASSERT_EQ(1U, scene->m_Models[0].m_Meshes.Size());
    ASSERT_EQ(6U, scene->m_Models[0].m_Meshes[0].m_Positions.Size());
    ASSERT_NEAR(1.0f, scene->m_Models[0].m_Meshes[0].m_Positions[0], 1e-6f);
    ASSERT_NEAR(2.0f, scene->m_Models[0].m_Meshes[0].m_Positions[1], 1e-6f);
    ASSERT_NEAR(3.0f, scene->m_Models[0].m_Meshes[0].m_Positions[2], 1e-6f);
    ASSERT_NEAR(4.0f, scene->m_Models[0].m_Meshes[0].m_Positions[3], 1e-6f);
    ASSERT_NEAR(5.0f, scene->m_Models[0].m_Meshes[0].m_Positions[4], 1e-6f);
    ASSERT_NEAR(6.0f, scene->m_Models[0].m_Meshes[0].m_Positions[5], 1e-6f);
    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, RejectsAnimationWithoutTargetNode)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":1,\"type\":\"SCALAR\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"}"
        "],"
        "\"animations\":[{"
            "\"samplers\":[{\"input\":0,\"output\":1}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"path\":\"translation\"}}]"
        "}]"
        "}";

    AssertGltfLoadError(json, "no supported target node");
}

TEST(ModelGLTF, RejectsAnimationWithZeroKeys)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"nodes\":[{}],"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":0,\"type\":\"SCALAR\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"}"
        "],"
        "\"animations\":[{"
            "\"samplers\":[{\"input\":0,\"output\":1}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]"
        "}]"
        "}";

    AssertGltfLoadError(json, "accessor has zero elements");
}

TEST(ModelGLTF, RejectsInvalidAnimationAccessorTypes)
{
    const char* integer_input =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"nodes\":[{}],"
        "\"accessors\":["
            "{\"componentType\":5123,\"count\":1,\"type\":\"SCALAR\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"}"
        "],"
        "\"animations\":[{"
            "\"samplers\":[{\"input\":0,\"output\":1}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]"
        "}]"
        "}";
    const char* wrong_output_shape =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"nodes\":[{}],"
        "\"accessors\":["
            "{\"componentType\":5126,\"count\":1,\"type\":\"SCALAR\"},"
            "{\"componentType\":5126,\"count\":1,\"type\":\"VEC2\"}"
        "],"
        "\"animations\":[{"
            "\"samplers\":[{\"input\":0,\"output\":1}],"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}]"
        "}]"
        "}";

    AssertGltfLoadError(integer_input, "animation input accessor must contain SCALAR floats");
    AssertGltfLoadError(wrong_output_shape, "animation output accessor has the wrong type");
}

TEST(ModelGLTF, RejectsImageWithAmbiguousSource)
{
    const char* both_sources =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":1,\"uri\":\"data:application/octet-stream;base64,AA==\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteLength\":1}],"
        "\"images\":[{\"uri\":\"image.png\",\"bufferView\":0,\"mimeType\":\"image/png\"}]"
        "}";
    const char* no_source =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"images\":[{\"mimeType\":\"image/png\"}]"
        "}";

    AssertGltfLoadError(both_sources, "exactly one of uri or bufferView");
    AssertGltfLoadError(no_source, "exactly one of uri or bufferView");
}

TEST(ModelGLTF, RejectsTruncatedHugeDataUriBeforeAllocation)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":2147483647,"
            "\"uri\":\"data:application/octet-stream;base64,AAAA\""
        "}]"
        "}";

    AssertGltfLoadError(json, "cgltf_result_data_too_short");
}

TEST(ModelGLTF, ResolvedBufferSizeIsBounded)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":4,\"uri\":\"payload.bin\"}]"
        "}";
    uint8_t buffer[] = { 1, 2, 3, 4, 5 };

    dmModelImporter::Scene* scene = LoadGltfJson(json);
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_TRUE(dmModelImporter::NeedsResolve(scene));

    dmModelImporter::ResolveBuffer(scene, "payload.bin", buffer, 3);
    ASSERT_NE((char*)0, scene->m_LoadError);
    ASSERT_NE((char*)0, strstr(scene->m_LoadError, "expected at least 4"));
    ASSERT_TRUE(dmModelImporter::NeedsResolve(scene));
    ASSERT_FALSE(dmModelImporter::LoadFinalize(scene));
    dmModelImporter::DestroyScene(scene);

    scene = LoadGltfJson(json);
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    dmModelImporter::ResolveBuffer(scene, "payload.bin", buffer, sizeof(buffer));
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_FALSE(dmModelImporter::NeedsResolve(scene));
    ASSERT_TRUE(dmModelImporter::LoadFinalize(scene));
    ASSERT_EQ(4U, scene->m_Buffers[0].m_BufferCount);
    ASSERT_ARRAY_EQ_LEN(buffer, scene->m_Buffers[0].m_Buffer, 4);
    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, ImageBufferContainsOnlyBufferViewBytes)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{"
            "\"byteLength\":8,"
            "\"uri\":\"data:application/octet-stream;base64,AAECAwQFBgc=\""
        "}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":2,\"byteLength\":3}],"
        "\"images\":[{\"name\":\"embedded\",\"bufferView\":0,\"mimeType\":\"image/png\"}]"
        "}";
    const uint8_t expected[] = { 2, 3, 4 };

    dmModelImporter::Scene* scene = LoadGltfJson(json);
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_EQ(1U, scene->m_Images.Size());
    ASSERT_EQ((char*)0, scene->m_Images[0].m_Uri);
    ASSERT_STREQ("image/png", scene->m_Images[0].m_MimeType);
    ASSERT_NE((dmModelImporter::Buffer*)0, scene->m_Images[0].m_Buffer);
    ASSERT_EQ(3U, scene->m_Images[0].m_Buffer->m_BufferCount);
    ASSERT_ARRAY_EQ_LEN(expected, scene->m_Images[0].m_Buffer->m_Buffer, sizeof(expected));

    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, MorphWeightsAnimationChannel)
{
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene("./src/test/assets/morph_weights_anim.gltf", options);
    ASSERT_NE((void*)0, scene);
    ASSERT_EQ(1u, scene->m_Animations.Size());
    const dmModelImporter::Animation& anim = scene->m_Animations[0];
    ASSERT_EQ(1u, anim.m_NodeAnimations.Size());
    const dmModelImporter::NodeAnimation& na = anim.m_NodeAnimations[0];
    ASSERT_EQ(2u, na.m_MorphWeightDimensions);
    ASSERT_EQ(2u, na.m_MorphWeightKeyTimes.Size());
    ASSERT_EQ(4u, na.m_MorphWeightKeyValues.Size());
    ASSERT_NEAR(0.0f, na.m_MorphWeightKeyTimes[0], 1e-6f);
    ASSERT_NEAR(1.0f, na.m_MorphWeightKeyTimes[1], 1e-6f);
    ASSERT_NEAR(0.0f, na.m_MorphWeightKeyValues[0], 1e-6f);
    ASSERT_NEAR(0.0f, na.m_MorphWeightKeyValues[1], 1e-6f);
    ASSERT_NEAR(1.0f, na.m_MorphWeightKeyValues[2], 1e-6f);
    ASSERT_NEAR(0.5f, na.m_MorphWeightKeyValues[3], 1e-6f);

    ASSERT_EQ(1u, scene->m_Models.Size());
    ASSERT_EQ(1u, scene->m_Models[0].m_Meshes.Size());
    ASSERT_EQ(2u, scene->m_Models[0].m_Meshes[0].m_MorphTargets.Size());
    ASSERT_EQ(2u, scene->m_Models[0].m_Meshes[0].m_MorphBaseWeights.Size());
    ASSERT_NEAR(0.0f, scene->m_Models[0].m_Meshes[0].m_MorphBaseWeights[0], 1e-6f);
    ASSERT_NEAR(0.0f, scene->m_Models[0].m_Meshes[0].m_MorphBaseWeights[1], 1e-6f);

    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, NormalizedIntegerMorphWeightsAnimationChannel)
{
    const char* json =
        "{"
        "\"asset\":{\"version\":\"2.0\"},"
        "\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"nodes\":[{\"mesh\":0}],"
        "\"meshes\":[{"
            "\"primitives\":[{"
                "\"attributes\":{\"POSITION\":0},"
                "\"targets\":[{\"POSITION\":1},{\"POSITION\":2}]"
            "}],"
            "\"weights\":[0,0]"
        "}],"
        "\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":1,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[0,0,0]},"
            "{\"bufferView\":1,\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"},"
            "{\"bufferView\":2,\"componentType\":5126,\"count\":1,\"type\":\"VEC3\"},"
            "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},"
            "{\"bufferView\":4,\"componentType\":5121,\"normalized\":true,\"count\":4,\"type\":\"SCALAR\"}"
        "],"
        "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":12},"
            "{\"buffer\":0,\"byteOffset\":12,\"byteLength\":12},"
            "{\"buffer\":0,\"byteOffset\":24,\"byteLength\":12},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":8},"
            "{\"buffer\":0,\"byteOffset\":44,\"byteLength\":4}"
        "],"
        "\"buffers\":[{\"uri\":\"payload.bin\",\"byteLength\":48}],"
        "\"animations\":[{"
            "\"channels\":[{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"weights\"}}],"
            "\"samplers\":[{\"input\":3,\"output\":4}]"
        "}]"
        "}";
    uint8_t buffer[48];
    memset(buffer, 0, sizeof(buffer));
    const float times[] = { 0.0f, 1.0f };
    memcpy(buffer + 36, times, sizeof(times));
    buffer[44] = 0;
    buffer[45] = 0;
    buffer[46] = 255;
    buffer[47] = 128;

    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, "gltf", (void*)json, (uint32_t)strlen(json));
    ASSERT_NE((dmModelImporter::Scene*)0, scene);
    ASSERT_TRUE(dmModelImporter::NeedsResolve(scene));
    dmModelImporter::ResolveBuffer(scene, "payload.bin", buffer, sizeof(buffer));
    ASSERT_TRUE(dmModelImporter::LoadFinalize(scene));
    ASSERT_EQ((char*)0, scene->m_LoadError);

    const dmModelImporter::NodeAnimation& animation = scene->m_Animations[0].m_NodeAnimations[0];
    ASSERT_EQ(2U, animation.m_MorphWeightDimensions);
    ASSERT_EQ(4U, animation.m_MorphWeightKeyValues.Size());
    ASSERT_NEAR(0.0f, animation.m_MorphWeightKeyValues[0], 1e-6f);
    ASSERT_NEAR(0.0f, animation.m_MorphWeightKeyValues[1], 1e-6f);
    ASSERT_NEAR(1.0f, animation.m_MorphWeightKeyValues[2], 1e-6f);
    ASSERT_NEAR(128.0f / 255.0f, animation.m_MorphWeightKeyValues[3], 1e-6f);

    dmModelImporter::DestroyScene(scene);
}

TEST(ModelGLTF, SparseMorphTarget)
{
    dmModelImporter::Options options;
    dmModelImporter::Scene* scene = LoadScene("./src/test/assets/sparse_morph_target.gltf", options);
    ASSERT_NE((void*)0, scene);

    ASSERT_EQ(1u, scene->m_Models.Size());
    ASSERT_EQ(1u, scene->m_Models[0].m_Meshes.Size());

    dmModelImporter::Mesh* mesh = &scene->m_Models[0].m_Meshes[0];
    ASSERT_EQ(1u, mesh->m_MorphTargets.Size());
    ASSERT_EQ(9u, mesh->m_MorphTargets[0].m_Positions.Size());

    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[0], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[1], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[2], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[3], 1e-6f);
    ASSERT_NEAR(1.0f, mesh->m_MorphTargets[0].m_Positions[4], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[5], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[6], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[7], 1e-6f);
    ASSERT_NEAR(0.0f, mesh->m_MorphTargets[0].m_Positions[8], 1e-6f);

    dmModelImporter::DestroyScene(scene);
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
