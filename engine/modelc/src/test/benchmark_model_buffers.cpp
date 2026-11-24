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

#include "modelimporter_buffers.h"

#include <gamesys/modelbuffers.h>
#include <dlib/hash.h>
#include <dlib/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Explicit benchmark of the production native compiler, without JNI/Java metadata.
namespace
{
    volatile uint64_t g_Checksum;

    struct Input
    {
        char*   m_Path;
        int32_t m_MeshIndex;
    };

    void MarkModels(const dmModelImporter::Node* node, uint8_t* used)
    {
        if (node->m_Model)
        {
            used[node->m_Model->m_Index] = 1;
        }
        for (uint32_t i = 0; i < node->m_Children.Size(); ++i)
        {
            MarkModels(node->m_Children[i], used);
        }
    }

    void SelectMeshes(dmModelImporter::Scene* scene, int32_t mesh_index, dmArray<dmModelImporter::Mesh*>& meshes)
    {
        dmArray<uint8_t> used;
        used.SetCapacity(scene->m_Models.Size());
        used.SetSize(scene->m_Models.Size());
        memset(used.Begin(), 0, used.Size());
        if (mesh_index >= 0)
        {
            used[mesh_index] = 1;
        }
        else
        {
            for (uint32_t i = 0; i < scene->m_RootNodes.Size(); ++i)
            {
                MarkModels(scene->m_RootNodes[i], used.Begin());
            }
        }
        uint32_t count = 0;
        for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
        {
            if (used[i])
            {
                count += scene->m_Models[i].m_Meshes.Size();
            }
        }
        meshes.SetCapacity(count);
        for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
        {
            if (!used[i])
            {
                continue;
            }
            for (uint32_t j = 0; j < scene->m_Models[i].m_Meshes.Size(); ++j)
            {
                meshes.Push(&scene->m_Models[i].m_Meshes[j]);
            }
        }
    }

    // GLB scene data may borrow the source bytes until DestroyScene. External
    // buffers are copied by ResolveBuffer and can be freed immediately.
    dmModelImporter::Scene* LoadScene(const char* path, void** source_data)
    {
        uint32_t size = 0;
        void* data = dmModelImporter::ReadFile(path, &size);
        *source_data = data;
        if (!data)
        {
            return 0;
        }
        dmModelImporter::Options options;
        dmModelImporter::Scene* scene = dmModelImporter::LoadFromBuffer(&options, strrchr(path, '.') + 1, data, size);
        if (!scene || scene->m_LoadError)
        {
            return scene;
        }

        bool needs_resolve = dmModelImporter::NeedsResolve(scene);
        for (uint32_t i = 0; needs_resolve && i < scene->m_Buffers.Size(); ++i)
        {
            const dmModelImporter::Buffer& buffer = scene->m_Buffers[i];
            if (buffer.m_Buffer || !buffer.m_BufferCount)
            {
                continue;
            }
            char buffer_path[4096];
            const char* slash = strrchr(path, '/');
            if (!slash)
            {
                slash = strrchr(path, '\\');
            }
            snprintf(buffer_path, sizeof(buffer_path), "%.*s/%s", (int)(slash - path), path, buffer.m_Uri);
            data = dmModelImporter::ReadFile(buffer_path, &size);
            if (!data)
            {
                dmModelImporter::DestroyScene(scene);
                return 0;
            }
            dmModelImporter::ResolveBuffer(scene, buffer.m_Uri, data, size);
            free(data);
        }
        if (needs_resolve && !dmModelImporter::LoadFinalize(scene))
        {
            dmModelImporter::DestroyScene(scene);
            return 0;
        }
        return scene;
    }
}

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: benchmark_model_buffers manifest.tsv warmups samples [output-directory]\n");
        return 1;
    }
    FILE* manifest = fopen(argv[1], "rb");
    if (!manifest)
    {
        return 1;
    }
    dmArray<Input> inputs;
    char line[8192];
    while (fgets(line, sizeof(line), manifest))
    {
        char* path = strchr(line, '\t');
        char* selection = path ? strchr(path + 1, '\t') : 0;
        if (!selection)
        {
            fclose(manifest);
            return 1;
        }
        *path++ = 0;
        *selection++ = 0;
        Input input = { strdup(path), atoi(selection) };
        if (inputs.Full())
        {
            inputs.OffsetCapacity(64);
        }
        inputs.Push(input);
    }
    fclose(manifest);

    int warmups = atoi(argv[2]);
    int samples = atoi(argv[3]);
    printf("sample\tmodel\tload_us\tprepare_us\tpack_us\tbytes\tranges\n");
    for (int sample = -warmups; sample < samples; ++sample)
    {
        for (uint32_t i = 0; i < inputs.Size(); ++i)
        {
            uint64_t start = dmTime::GetMonotonicTime();
            void* source_data;
            dmModelImporter::Scene* scene = LoadScene(inputs[i].m_Path, &source_data);
            uint64_t loaded = dmTime::GetMonotonicTime();
            if (!scene || scene->m_LoadError || dmModelImporter::NeedsResolve(scene))
            {
                fprintf(stderr, "Failed to load %s: %s\n", inputs[i].m_Path, scene && scene->m_LoadError ? scene->m_LoadError : "unresolved input");
                return 1;
            }
            if (inputs[i].m_MeshIndex >= (int32_t)scene->m_Models.Size())
            {
                fprintf(stderr, "Invalid mesh selection for %s\n", inputs[i].m_Path);
                return 1;
            }

            dmArray<dmModelImporter::Mesh*> meshes;
            SelectMeshes(scene, inputs[i].m_MeshIndex, meshes);
            uint64_t prepared = dmTime::GetMonotonicTime();
            dmArray<uint8_t> output;
            if (!dmModelImporter::CompileModelBuffers(meshes.Begin(), meshes.Size(), output))
            {
                fprintf(stderr, "Packed model exceeds DMMB size limits: %s\n", inputs[i].m_Path);
                return 1;
            }
            uint64_t packed = dmTime::GetMonotonicTime();
            // Consume the bytes outside timing so warmup work cannot disappear.
            g_Checksum = dmHashBuffer64(output.Begin(), output.Size());
            if (sample >= 0)
            {
                printf("%d\t%u\t%llu\t%llu\t%llu\t%u\t%u\n", sample, i, (unsigned long long)(loaded - start), (unsigned long long)(prepared - loaded), (unsigned long long)(packed - prepared), output.Size(), ((const dmGameSystem::ModelBuffersHeader*)output.Begin())->m_RangeCount);
            }
            if (argc > 4 && sample == 0)
            {
                char output_path[4096];
                snprintf(output_path, sizeof(output_path), "%s/%u.modelbuffersc", argv[4], i);
                FILE* file = fopen(output_path, "wb");
                if (!file || fwrite(output.Begin(), 1, output.Size(), file) != output.Size())
                {
                    return 1;
                }
                fclose(file);
            }
            dmModelImporter::DestroyScene(scene);
            free(source_data);
        }
    }
    for (uint32_t i = 0; i < inputs.Size(); ++i)
    {
        free(inputs[i].m_Path);
    }
    return 0;
}
