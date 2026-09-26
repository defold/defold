// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0. See https://www.defold.com/license

#include "modelimporter.h"
#include "modelimporter_compression.h"
#include "cgltf/cgltf.h"
#include <dlib/dstrings.h>
#include <jc_test/jc_test.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace dmModelImporter;
namespace Codec = dmModelImporter::Compression;

#include "assets/compression/meshopt/cases.h"

struct CompressionFixture
{
    Scene* m_Scene;
    void*  m_Source;
};

// Reads relative to the compression fixture directory; the caller frees the bytes.
static void* ReadCompressionFile(const char* name, uint32_t* size)
{
    char path[512];
    dmSnPrintf(path, sizeof(path), "./src/test/assets/compression/%s", name);
    return ReadFile(path, size);
}

// Loads and resolves a fixture while retaining its source bytes for GLB views.
// Failed scenes are preserved for error assertions; destroy the returned fixture.
static CompressionFixture LoadCompressionFixture(const char* name, Options* options = 0)
{
    CompressionFixture fixture = {};
    uint32_t           size;
    fixture.m_Source = ReadCompressionFile(name, &size);
    if (!fixture.m_Source)
        return fixture;
    Options defaults;
    fixture.m_Scene = LoadFromBuffer(options ? options : &defaults, strrchr(name, '.') + 1, fixture.m_Source, size);
    Scene* scene = fixture.m_Scene;
    if (!scene || scene->m_LoadError || !NeedsResolve(scene))
        return fixture;
    for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
    {
        Buffer* buffer = &scene->m_Buffers[i];
        if (buffer->m_Buffer || buffer->m_BufferCount == 0)
            continue;
        char path[512];
        dmStrlCpy(path, name, sizeof(path));
        char* slash = strrchr(path, '/');
        if (slash)
            slash[1] = 0;
        else
            path[0] = 0;
        dmStrlCat(path, buffer->m_Uri, sizeof(path));
        uint32_t buffer_size = 0;
        void*    bytes = ReadCompressionFile(path, &buffer_size);
        ResolveBuffer(scene, buffer->m_Uri, bytes, buffer_size);
        free(bytes);
    }
    LoadFinalize(scene);
    return fixture;
}

// The scene must release any borrowed GLB views before its source bytes are freed.
static void DestroyCompressionFixture(CompressionFixture* fixture)
{
    DestroyScene(fixture->m_Scene);
    free(fixture->m_Source);
}

static void AssertFloatArray(const float* expected, uint32_t count, const dmArray<float>& actual, float tolerance = 0.0f)
{
    ASSERT_EQ(count, actual.Size());
    for (uint32_t i = 0; i < count; ++i)
        ASSERT_NEAR(expected[i], actual[i], tolerance);
}

// Matches the Box's twelve triangles despite vertex/triangle reordering, while
// checking winding, normals, and bounds within codec quantization tolerances.
static void AssertTriangleGeometry(const Mesh& expected, const Mesh& actual, float position_tolerance, float normal_tolerance)
{
    ASSERT_EQ(expected.m_VertexCount, actual.m_VertexCount);
    ASSERT_EQ(expected.m_Indices.Size(), actual.m_Indices.Size());
    bool matched[12] = {};
    ASSERT_EQ(36U, expected.m_Indices.Size());
    for (uint32_t i = 0; i < expected.m_Indices.Size(); i += 3)
    {
        bool found = false;
        for (uint32_t j = 0; j < actual.m_Indices.Size() && !found; j += 3)
        {
            if (matched[j / 3])
                continue;
            // Cyclic rotation preserves triangle winding; reversing it does not.
            for (uint32_t rotation = 0; rotation < 3 && !found; ++rotation)
            {
                bool equal = true;
                for (uint32_t k = 0; k < 3; ++k)
                {
                    uint32_t a = expected.m_Indices[i + k];
                    uint32_t b = actual.m_Indices[j + (k + rotation) % 3];
                    for (uint32_t c = 0; c < 3; ++c)
                    {
                        equal &= fabsf(expected.m_Positions[a * 3 + c] - actual.m_Positions[b * 3 + c]) <= position_tolerance;
                        equal &= fabsf(expected.m_Normals[a * 3 + c] - actual.m_Normals[b * 3 + c]) <= normal_tolerance;
                    }
                }
                if (equal)
                {
                    found = true;
                    matched[j / 3] = true;
                }
            }
        }
        ASSERT_TRUE(found);
    }
    ASSERT_NEAR(expected.m_Aabb.m_Min.x, actual.m_Aabb.m_Min.x, position_tolerance);
    ASSERT_NEAR(expected.m_Aabb.m_Min.y, actual.m_Aabb.m_Min.y, position_tolerance);
    ASSERT_NEAR(expected.m_Aabb.m_Min.z, actual.m_Aabb.m_Min.z, position_tolerance);
    ASSERT_NEAR(expected.m_Aabb.m_Max.x, actual.m_Aabb.m_Max.x, position_tolerance);
    ASSERT_NEAR(expected.m_Aabb.m_Max.y, actual.m_Aabb.m_Max.y, position_tolerance);
    ASSERT_NEAR(expected.m_Aabb.m_Max.z, actual.m_Aabb.m_Max.z, position_tolerance);
}

TEST(ModelCompression, BoxVariants)
{
    const char* variants[] = {
        "box/glTF-Draco/Box.gltf",
        "box/glTF-Meshopt/Box.gltf",
        "box/glTF-Meshopt/Box-embedded.gltf",
        "box/glTF-Meshopt/Box.glb",
    };
    CompressionFixture original = LoadCompressionFixture("box/glTF/Box.gltf");
    ASSERT_NE((Scene*)0, original.m_Scene);
    ASSERT_EQ((char*)0, original.m_Scene->m_LoadError);
    for (uint32_t i = 0; i < sizeof(variants) / sizeof(variants[0]); ++i)
    {
        CompressionFixture fixture = LoadCompressionFixture(variants[i]);
        Scene*             scene = fixture.m_Scene;
        ASSERT_NE((Scene*)0, scene);
        ASSERT_EQ((char*)0, scene->m_LoadError);
        ASSERT_FALSE(NeedsResolve(scene));
        ASSERT_TRUE(Validate(scene));
        ASSERT_EQ(1U, scene->m_Models.Size());
        AssertTriangleGeometry(original.m_Scene->m_Models[0].m_Meshes[0], scene->m_Models[0].m_Meshes[0], i == 0 ? 0.001f : 0.0f, i == 0 ? 0.01f : 0.0f);
        ASSERT_EQ(original.m_Scene->m_Nodes.Size(), scene->m_Nodes.Size());
        for (uint32_t n = 0; n < scene->m_Nodes.Size(); ++n)
            ASSERT_EQ(0, memcmp(&original.m_Scene->m_Nodes[n].m_World, &scene->m_Nodes[n].m_World, sizeof(Transform)));
        ASSERT_EQ(1U, scene->m_Materials.Size());
        ASSERT_STREQ("Red", scene->m_Materials[0].m_Name);
        ASSERT_NEAR(0.8f, scene->m_Materials[0].m_PbrMetallicRoughness->m_BaseColorFactor[0], 1e-6f);
        DestroyCompressionFixture(&fixture);
    }
    DestroyCompressionFixture(&original);
}

TEST(ModelCompression, MeshoptModesAndFilters)
{
    for (uint32_t i = 0; i < sizeof(MESHOPT_CASES) / sizeof(MESHOPT_CASES[0]); ++i)
    {
        const MeshoptTestCase& test = MESHOPT_CASES[i];
        char                   error[256];
        uint8_t                actual[128], invalid[128];
        ASSERT_LE(test.m_DecodedSize, sizeof(actual));
        ASSERT_LE(test.m_EncodedSize, sizeof(invalid));
        ASSERT_TRUE(Codec::DecodeMeshoptBuffer(test.m_Buffer, test.m_Encoded, test.m_EncodedSize, actual, test.m_DecodedSize, error, sizeof(error)));
        ASSERT_EQ(0, memcmp(test.m_Decoded, actual, test.m_DecodedSize));
        ASSERT_FALSE(Codec::DecodeMeshoptBuffer(test.m_Buffer, test.m_Encoded, test.m_EncodedSize - 1, actual, test.m_DecodedSize, error, sizeof(error)));
        memcpy(invalid, test.m_Encoded, test.m_EncodedSize);
        invalid[0] = 0xff;
        ASSERT_FALSE(Codec::DecodeMeshoptBuffer(test.m_Buffer, invalid, test.m_EncodedSize, actual, test.m_DecodedSize, error, sizeof(error)));
    }
}

TEST(ModelCompression, MeshoptAnimatedSkinnedSparseMorph)
{
    CompressionFixture fixture = LoadCompressionFixture("meshopt/streams.gltf");
    ASSERT_NE((Scene*)0, fixture.m_Scene);
    ASSERT_EQ((char*)0, fixture.m_Scene->m_LoadError);
    ASSERT_TRUE(Validate(fixture.m_Scene));
    const Mesh& actual = fixture.m_Scene->m_Models[0].m_Meshes[0];
    const float positions[] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
    const float uvs[] = { 0, 1, 1, 1, 0, 0 };
    const float colors[] = { 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 0, 1 };
    const float weights[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };
    const float morph[] = { 0, 0, 0, 0, 0, 2, 0, 0, 0 };
    AssertFloatArray(positions, 9, actual.m_Positions);
    AssertFloatArray(uvs, 6, actual.m_TexCoords0);
    // COLOR filtering quantizes RGB by at most one byte value.
    AssertFloatArray(colors, 12, actual.m_Colors, 1.0f / 255.0f);
    AssertFloatArray(weights, 12, actual.m_Weights);
    ASSERT_EQ(12U, actual.m_Bones.Size());
    for (uint32_t i = 0; i < actual.m_Bones.Size(); ++i)
        ASSERT_EQ(0U, actual.m_Bones[i]);
    ASSERT_EQ(1U, actual.m_MorphTargets.Size());
    AssertFloatArray(morph, 9, actual.m_MorphTargets[0].m_Positions);
    ASSERT_EQ(1U, fixture.m_Scene->m_Skins.Size());
    ASSERT_EQ(1U, fixture.m_Scene->m_Animations.Size());
    const NodeAnimation& animation = fixture.m_Scene->m_Animations[0].m_NodeAnimations[0];
    ASSERT_EQ(2U, animation.m_RotationKeys.Size());
    ASSERT_NEAR(1.0f, animation.m_RotationKeys[0].m_Value[3], 1e-4f);
    ASSERT_NEAR(1.0f, animation.m_RotationKeys[1].m_Value[2], 1e-4f);
    ASSERT_EQ(2U, animation.m_TranslationKeys.Size());
    ASSERT_NEAR(0.25f, animation.m_TranslationKeys[0].m_Value[0], 0.0f);
    ASSERT_NEAR(8.0f, animation.m_TranslationKeys[1].m_Value[2], 0.0f);
    DestroyCompressionFixture(&fixture);
}

TEST(ModelCompression, DracoSharedAccessorsAndAttributeMapping)
{
    CompressionFixture fixture = LoadCompressionFixture("draco/shared.gltf");
    ASSERT_NE((Scene*)0, fixture.m_Scene);
    ASSERT_EQ((char*)0, fixture.m_Scene->m_LoadError);
    ASSERT_TRUE(Validate(fixture.m_Scene));
    ASSERT_EQ(2U, fixture.m_Scene->m_Models[0].m_Meshes.Size());
    for (uint32_t m = 0; m < 2; ++m)
    {
        const Mesh& mesh = fixture.m_Scene->m_Models[0].m_Meshes[m];
        ASSERT_EQ(4U, mesh.m_VertexCount);
        ASSERT_EQ(m == 0 ? 6U : 4U, mesh.m_Indices.Size());
        for (uint32_t v = 0; v < 4; ++v)
        {
            ASSERT_NEAR(float(m * 10 + v % 2), mesh.m_Positions[v * 3], 0.0f);
            ASSERT_NEAR(float(v / 2), mesh.m_Positions[v * 3 + 1], 0.0f);
            ASSERT_NEAR(1.0f, mesh.m_Colors[v * 4], 0.0f);
            ASSERT_NEAR(128.0f / 255.0f, mesh.m_Colors[v * 4 + 1], 1e-6f);
            ASSERT_NEAR(1.0f, mesh.m_Weights[v * 4], 0.0f);
            ASSERT_EQ(0U, mesh.m_Bones[v * 4]);
            ASSERT_NEAR(1.0f, mesh.m_Normals[v * 3 + 2], 0.0f);
            ASSERT_NEAR(1.0f - float(v / 2), mesh.m_TexCoords0[v * 2 + 1], 0.0f);
            ASSERT_NEAR(0.25f, mesh.m_MorphTargets[0].m_Positions[v * 3 + 2], 0.0f);
        }
    }
    DestroyCompressionFixture(&fixture);
}

TEST(ModelCompression, DracoTriangleStrip)
{
    CompressionFixture fixture = LoadCompressionFixture("draco/shared.gltf");
    ASSERT_NE((Scene*)0, fixture.m_Scene);
    ASSERT_EQ((char*)0, fixture.m_Scene->m_LoadError);
    ASSERT_TRUE(Validate(fixture.m_Scene));
    const Mesh& mesh = fixture.m_Scene->m_Models[0].m_Meshes[1];
    ASSERT_EQ(PRIMITIVE_TYPE_TRIANGLE_STRIP, mesh.m_PrimitiveType);
    ASSERT_EQ(4U, mesh.m_Indices.Size());
    for (uint32_t i = 0; i < 2; ++i)
    {
        uint32_t a = mesh.m_Indices[i + (i % 2)], b = mesh.m_Indices[i + 1 - (i % 2)], c = mesh.m_Indices[i + 2];
        float    abx = mesh.m_Positions[b * 3] - mesh.m_Positions[a * 3], aby = mesh.m_Positions[b * 3 + 1] - mesh.m_Positions[a * 3 + 1];
        float    acx = mesh.m_Positions[c * 3] - mesh.m_Positions[a * 3], acy = mesh.m_Positions[c * 3 + 1] - mesh.m_Positions[a * 3 + 1];
        ASSERT_NEAR(1.0f, abx * acy - aby * acx, 0.0f);
    }
    DestroyCompressionFixture(&fixture);
}

// Replaces the first matching JSON fragment in an embedded glTF fixture, avoiding
// extra files for malformed cases. Returns null if no match; caller destroys scene.
static Scene* LoadModifiedCompressionJson(const char* path, const char* before, const char* after, Options* options = 0)
{
    uint32_t size;
    char*    json = (char*)ReadCompressionFile(path, &size);
    if (!json)
        return 0;
    json = (char*)realloc(json, size + 1);
    json[size] = 0;
    char* match = strstr(json, before);
    if (!match)
    {
        free(json);
        return 0;
    }
    size_t prefix = match - json, old_length = strlen(before), new_length = strlen(after);
    size_t new_size = size - old_length + new_length;
    char*  modified = (char*)malloc(new_size + 1);
    memcpy(modified, json, prefix);
    memcpy(modified + prefix, after, new_length);
    memcpy(modified + prefix + new_length, match + old_length, size - prefix - old_length);
    modified[new_size] = 0;
    Options defaults;
    Scene*  scene = LoadFromBuffer(options ? options : &defaults, "gltf", modified, (uint32_t)new_size);
    free(modified);
    free(json);
    return scene;
}

TEST(ModelCompression, DracoFailures)
{
    const char* before[] = { "\"POSITION\": 42", "\"count\": 6", "\"componentType\": 5126", "\"mode\": 4", "RFJBQ08" };
    const char* after[] = { "\"POSITION\": 43", "\"count\": 3", "\"componentType\": 5122", "\"mode\": 1", "QUFBQUF" };
    for (uint32_t i = 0; i < sizeof(before) / sizeof(before[0]); ++i)
    {
        Scene* scene = LoadModifiedCompressionJson("draco/shared.gltf", before[i], after[i]);
        ASSERT_NE((Scene*)0, scene);
        ASSERT_NE((char*)0, scene->m_LoadError);
        ASSERT_NE((char*)0, strstr(scene->m_LoadError, "Draco"));
        ASSERT_EQ(0U, scene->m_Models.Size());
        DestroyScene(scene);
    }
}

TEST(ModelCompression, MeshoptFailures)
{
    struct Case
    {
        const char* m_Before;
        const char* m_After;
    };
    const Case cases[] = {
        { "\"mode\": \"ATTRIBUTES\"", "\"mode\": \"UNKNOWN\"" },
        { "\"mode\": \"TRIANGLES\"", "\"mode\": \"TRIANGLES\", \"filter\": \"OCTAHEDRAL\"" },
        { "\"mode\": \"ATTRIBUTES\"", "\"mode\": \"ATTRIBUTES\", \"filter\": \"UNKNOWN\"" },
        { "\"byteStride\": 12", "\"byteStride\": 8" },
        { "\"count\": 48", "\"count\": 18446744073709551615" },
        { "\"byteOffset\": 0", "\"byteOffset\": 999999" },
        { "\"extensionsRequired\"", "\"extensionsNotRequired\"" },
        { "EXT_meshopt_compression", "KHR_meshopt_compression" },
    };
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        Scene* scene = LoadModifiedCompressionJson("box/glTF-Meshopt/Box-embedded.gltf", cases[i].m_Before, cases[i].m_After);
        ASSERT_NE((Scene*)0, scene);
        ASSERT_NE((char*)0, scene->m_LoadError);
        ASSERT_EQ(0U, scene->m_Models.Size());
        DestroyScene(scene);
    }
}

TEST(ModelCompression, MetadataDoesNotDecodeGeometry)
{
    Options options;
    options.m_LoadMaterialsOnly = true;
    options.m_LoadMeshMetadata = true;
    options.m_SkipImageData = true;
    Scene* scene = LoadModifiedCompressionJson("draco/shared.gltf", "RFJBQ08", "QUFBQUF", &options);
    ASSERT_NE((Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_FALSE(NeedsResolve(scene));
    ASSERT_EQ(1U, scene->m_Models.Size());
    ASSERT_EQ(0U, scene->m_Models[0].m_Meshes[0].m_Positions.Size());
    DestroyScene(scene);
    CompressionFixture fixture = LoadCompressionFixture("box/glTF-Meshopt/Box.gltf", &options);
    ASSERT_NE((Scene*)0, fixture.m_Scene);
    ASSERT_EQ((char*)0, fixture.m_Scene->m_LoadError);
    ASSERT_FALSE(NeedsResolve(fixture.m_Scene));
    ASSERT_EQ(0U, fixture.m_Scene->m_Models[0].m_Meshes[0].m_Positions.Size());
    DestroyCompressionFixture(&fixture);
}

TEST(ModelCompression, DecodedIndicesAreValidated)
{
    const char* paths[] = { "meshopt/invalid-indices.gltf", "meshopt/invalid-sparse.gltf" };
    for (uint32_t i = 0; i < 2; ++i)
    {
        CompressionFixture fixture = LoadCompressionFixture(paths[i]);
        ASSERT_NE((Scene*)0, fixture.m_Scene);
        ASSERT_NE((char*)0, fixture.m_Scene->m_LoadError);
        ASSERT_NE((char*)0, strstr(fixture.m_Scene->m_LoadError, "validation failed"));
        ASSERT_EQ(0U, fixture.m_Scene->m_Models.Size());
        DestroyCompressionFixture(&fixture);
    }
}

TEST(ModelCompression, UnusedFallbackUriIsNotResolved)
{
    Scene* scene = LoadModifiedCompressionJson("box/glTF-Meshopt/Box-embedded.gltf",
                                               "\"byteLength\": 648",
                                               "\"byteLength\": 648, \"uri\": \"unused-fallback.bin\"");
    ASSERT_NE((Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_FALSE(NeedsResolve(scene));
    ASSERT_EQ(0U, scene->m_Buffers[1].m_BufferCount);
    ASSERT_EQ(1U, scene->m_Models.Size());
    DestroyScene(scene);
}

TEST(ModelCompression, MixedFallbackBufferStillRequiresOrdinaryData)
{
    // Make the first view ordinary while the second still uses the same fallback buffer.
    Scene* scene = LoadModifiedCompressionJson("box/glTF-Meshopt/Box-embedded.gltf",
                                               "\"EXT_meshopt_compression\": {",
                                               "\"unused_extension\": {");
    ASSERT_NE((Scene*)0, scene);
    ASSERT_EQ((char*)0, scene->m_LoadError);
    ASSERT_TRUE(NeedsResolve(scene));
    ASSERT_FALSE(LoadFinalize(scene));
    ASSERT_NE((char*)0, scene->m_LoadError);
    DestroyScene(scene);
}

TEST(ModelCompression, ExternalCompressedBufferFailure)
{
    uint32_t size;
    void*    json = ReadCompressionFile("box/glTF-Meshopt/Box.gltf", &size);
    ASSERT_NE((void*)0, json);
    Options options;
    Scene*  scene = LoadFromBuffer(&options, "gltf", json, size);
    ASSERT_NE((Scene*)0, scene);
    ASSERT_TRUE(NeedsResolve(scene));
    uint32_t buffer_size = scene->m_Buffers[0].m_BufferCount;
    void*    invalid = calloc(1, buffer_size);
    ResolveBuffer(scene, "Box.bin", invalid, buffer_size);
    ASSERT_FALSE(NeedsResolve(scene));
    ASSERT_FALSE(LoadFinalize(scene));
    ASSERT_NE((char*)0, strstr(scene->m_LoadError, "Meshopt"));
    ASSERT_EQ(0U, scene->m_Models.Size());
    free(invalid);
    DestroyScene(scene);
    free(json);
}

TEST(ModelCompression, MeshoptLayoutAndVersionLimits)
{
    char                 error[256];
    Codec::MeshoptBuffer buffer = { Codec::MODE_ATTRIBUTES, Codec::FILTER_NONE, Codec::KHR_MESHOPT, 3, 12 };
    ASSERT_TRUE(Codec::ValidateMeshoptBuffer(buffer, 36, error, sizeof(error)));
    buffer.m_Stride = 0;
    ASSERT_FALSE(Codec::ValidateMeshoptBuffer(buffer, 36, error, sizeof(error)));
    buffer.m_Stride = 12;
    buffer.m_Count = SIZE_MAX;
    ASSERT_FALSE(Codec::ValidateMeshoptBuffer(buffer, 36, error, sizeof(error)));
    buffer.m_Count = 3;
    buffer.m_Filter = Codec::FILTER_QUATERNION;
    ASSERT_FALSE(Codec::ValidateMeshoptBuffer(buffer, 36, error, sizeof(error)));
    buffer.m_Stride = 4;
    buffer.m_Filter = Codec::FILTER_COLOR;
    buffer.m_Extension = Codec::EXT_MESHOPT;
    ASSERT_FALSE(Codec::ValidateMeshoptBuffer(buffer, 12, error, sizeof(error)));
    buffer.m_Stride = 12;
    buffer.m_Filter = Codec::FILTER_NONE;
    const MeshoptTestCase& test = MESHOPT_CASES[0];
    float                  output[9];
    ASSERT_FALSE(Codec::DecodeMeshoptBuffer(buffer, test.m_Encoded, test.m_EncodedSize, output, sizeof(output), error, sizeof(error)));
    ASSERT_NE((char*)0, strstr(error, "bitstream version"));
}

TEST(ModelCompression, DracoParserUniqueIds)
{
    // IDs are unsigned and have no relationship to the accessor array.
    const char* json =
    "{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":1}],"
    "\"bufferViews\":[{\"buffer\":0,\"byteLength\":1}],\"meshes\":[{\"primitives\":[{"
    "\"extensions\":{\"KHR_draco_mesh_compression\":{\"bufferView\":0,\"attributes\":{\"POSITION\":4294967295}}}}]}]}";
    cgltf_options options = {};
    cgltf_data*   data = 0;
    ASSERT_EQ(cgltf_result_success, cgltf_parse(&options, json, strlen(json), &data));
    ASSERT_EQ(UINT32_MAX, data->meshes[0].primitives[0].draco_mesh_compression.attributes[0].unique_id);
    cgltf_free(data);
}

TEST(ModelCompression, DracoTruncatedStream)
{
    uint32_t size;
    void*    bytes = ReadCompressionFile("box/glTF-Draco/Box.bin", &size);
    ASSERT_NE((void*)0, bytes);
    char                 error[256];
    Codec::DracoMeshInfo info;
    const uint32_t       lengths[] = { 0, 1, 10, size / 2 };
    for (uint32_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i)
    {
        Codec::HDracoMesh mesh = Codec::DecodeDracoMesh(bytes, lengths[i], false, &info, error, sizeof(error));
        ASSERT_EQ((Codec::HDracoMesh)0, mesh);
        ASSERT_NE('\0', error[0]);
        Codec::DestroyDracoMesh(mesh);
    }
    free(bytes);
}

TEST(ModelCompression, DracoIndexComponentTypes)
{
    const char* before[] = { "\"componentType\": 5123", "\"indices\": 0," };
    const char* after[] = { "\"componentType\": 5121", "" };
    for (uint32_t i = 0; i < 2; ++i)
    {
        Scene* scene = LoadModifiedCompressionJson("draco/shared.gltf", before[i], after[i]);
        ASSERT_NE((Scene*)0, scene);
        ASSERT_EQ((char*)0, scene->m_LoadError);
        ASSERT_TRUE(Validate(scene));
        const Mesh& mesh = scene->m_Models[0].m_Meshes[0];
        ASSERT_EQ(6U, mesh.m_Indices.Size());
        ASSERT_EQ(0U, mesh.m_Indices[0]);
        ASSERT_EQ(3U, mesh.m_Indices[5]);
        DestroyScene(scene);
    }
}
