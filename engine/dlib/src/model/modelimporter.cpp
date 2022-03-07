
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
#include <stdio.h>
#include <stdlib.h>

namespace dmModelImporter
{

Options::Options()
{
}

static void DestroyMesh(Mesh* mesh)
{
    delete[] mesh->m_Positions;
    delete[] mesh->m_Normals;
    delete[] mesh->m_Tangents;
    delete[] mesh->m_Color;
    delete[] mesh->m_Weights;
    delete[] mesh->m_Bones;
    delete[] mesh->m_TexCoord0;
    delete[] mesh->m_TexCoord1;
    free((void*)mesh->m_Material);
}

static void DestroyModel(Model* model)
{
    free((void*)model->m_Name);
    for (uint32_t i = 0; i < model->m_MeshesCount; ++i)
        DestroyMesh(&model->m_Meshes[i]);
    delete[] model->m_Meshes;
}

static void DestroyNode(Node* node)
{
    free((void*)node->m_Name);
}

void DestroyScene(Scene* scene)
{
    scene->m_DestroyFn(scene->m_OpaqueSceneData);

    for (uint32_t i = 0; i < scene->m_NodesCount; ++i)
        DestroyNode(&scene->m_Nodes[i]);

    delete[] scene->m_Nodes;
    delete[] scene->m_RootNodes;

    for (uint32_t i = 0; i < scene->m_ModelsCount; ++i)
        DestroyModel(&scene->m_Models[i]);
    delete[] scene->m_Models;
}


void* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        printf("Failed to load %s\n", path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    void* mem = malloc(size);

    size_t nread = fread(mem, 1, size, file);
    fclose(file);

    if (nread != size)
    {
        printf("Failed to read %u bytes from %s\n", (uint32_t)size, path);
        free(mem);
        return 0;
    }

    if (file_size)
        *file_size = size;

    return mem;
}

static void OutputIndent(int indent)
{
    for (int i = 0; i < indent; ++i) {
        printf("  ");
    }
}

static void OutputNodeTree(Node* node, int indent)
{
    OutputIndent(indent);
    printf("%s\n", node->m_Name);
    for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
    {
        OutputNodeTree(node->m_Children[i], indent+1);
    }
}

static void OutputMesh(Mesh* mesh, int indent)
{
    OutputIndent(indent);
    printf("mesh  vertices: %u  mat: %s\n", mesh->m_VertexCount, mesh->m_Material);
    //printf("tris: %u  material: %s", mesh->)
}

static void OutputModel(Model* model, int indent)
{
    OutputIndent(indent);
    printf("%s   meshes count: %u\n", model->m_Name, model->m_MeshesCount);
    for (uint32_t i = 0; i < model->m_MeshesCount; ++i)
    {
        Mesh* mesh = &model->m_Meshes[i];
        OutputMesh(mesh, indent+1);
    }
}

void DebugScene(Scene* scene)
{
    printf("Output model importer scene:\n");

    for (uint32_t i = 0; i < scene->m_NodesCount; ++i)
    {
        const Node& node = scene->m_Nodes[i];
        printf("Node: %s\n", node.m_Name);
    }

    printf("Subscenes: count: %u\n", scene->m_RootNodesCount);
    for (uint32_t i = 0; i < scene->m_RootNodesCount; ++i)
    {
        printf("------------------------------\n");
        OutputNodeTree(scene->m_RootNodes[i], 1);
        printf("------------------------------\n");
    }

    printf("Models: count: %u\n", scene->m_ModelsCount);
    for (uint32_t i = 0; i < scene->m_ModelsCount; ++i)
    {
        printf("------------------------------\n");
        OutputModel(&scene->m_Models[i], 1);
        printf("------------------------------\n");
    }
}

}
