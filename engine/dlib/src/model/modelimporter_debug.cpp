
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
#include <dmsdk/dlib/dstrings.h>
#include <stdio.h>
#include <stdlib.h>

namespace dmModelImporter
{

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

static void OutputTransform(dmTransform::Transform& transform)
{
    printf("t: %f, %f, %f  ", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
    printf("r: %f, %f, %f, %f  ", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
    printf("s: %f, %f, %f  ", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
}

static void OutputBone(Bone* bone, int indent)
{
    OutputIndent(indent);
    printf("%s  ", bone->m_Name);
    OutputTransform(bone->m_InvBindPose);
    printf("\n");
}

static void OutputSkin(Skin* skin, int indent)
{
    OutputIndent(indent);
    printf("Skin name: %s\n", skin->m_Name);

    printf("  Bones: count: %u\n", skin->m_BonesCount);
    for (uint32_t i = 0; i < skin->m_BonesCount; ++i)
    {
        OutputBone(&skin->m_Bones[i], indent+1);
    }
}

static void OutputNodeTree(Node* node, int indent)
{
    OutputIndent(indent);
    printf("%s: ", node->m_Name);
    if (node->m_Skin)
        printf("skin: %s", node->m_Skin->m_Name);
    printf("\n");

    for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
    {
        OutputNodeTree(node->m_Children[i], indent+1);
    }
}

static void OutputMesh(Mesh* mesh, int indent)
{
    OutputIndent(indent);
    printf("mesh  vertices: %u  mat: %s  weights: %s\n", mesh->m_VertexCount, mesh->m_Material, mesh->m_Weights?"yes":"no");

    // if (mesh->m_Weights)
    // {
    //     for (uint32_t i = 0; i < mesh->m_VertexCount; ++i)
    //     {
    //         printf("  %4u  B: %2u, %2u, %2u, %2u\t", i, mesh->m_Bones[i*4+0], mesh->m_Bones[i*4+1], mesh->m_Bones[i*4+2], mesh->m_Bones[i*4+3]);
    //         printf("  W: %f, %f, %f, %f\n", mesh->m_Weights[i*4+0], mesh->m_Weights[i*4+1], mesh->m_Weights[i*4+2], mesh->m_Weights[i*4+3]);
    //     }
    // }
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

static void OutputNodeAnimation(NodeAnimation* node_animation, int indent)
{
    OutputIndent(indent);
    printf("node: %s\n", node_animation->m_Node->m_Name);

    // indent++;
    // OutputIndent(indent);
    // printf("# translation keys: %u\n", node_animation->m_TranslationKeysCount);
    // for (uint32_t i = 0; i < node_animation->m_TranslationKeysCount; ++i)
    // {
    //     OutputIndent(indent+1);
    //     KeyFrame* key = &node_animation->m_TranslationKeys[i];
    //     printf("%.3f:  %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2]);
    // }

    // OutputIndent(indent);
    // printf("# rotation keys: %u\n", node_animation->m_RotationKeysCount);
    // for (uint32_t i = 0; i < node_animation->m_RotationKeysCount; ++i)
    // {
    //     OutputIndent(indent+1);
    //     KeyFrame* key = &node_animation->m_RotationKeys[i];
    //     printf("%.3f:  %.3f, %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2], key->m_Value[3]);
    // }

    // OutputIndent(indent);
    // printf("# scale keys: %u\n", node_animation->m_ScaleKeysCount);
    // for (uint32_t i = 0; i < node_animation->m_ScaleKeysCount; ++i)
    // {
    //     OutputIndent(indent+1);
    //     KeyFrame* key = &node_animation->m_ScaleKeys[i];
    //     printf("%.3f:  %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2]);
    // }
}

static void OutputAnimation(Animation* animation, int indent)
{
    OutputIndent(indent);
    printf("%s\n", animation->m_Name);

    for (uint32_t i = 0; i < animation->m_NodeAnimationsCount; ++i)
    {
        NodeAnimation* node_animation = &animation->m_NodeAnimations[i];
        OutputNodeAnimation(node_animation, indent+1);
    }
}

void DebugScene(Scene* scene)
{
    printf("Output model importer scene:\n");

    printf("------------------------------\n");
    for (uint32_t i = 0; i < scene->m_NodesCount; ++i)
    {
        const Node& node = scene->m_Nodes[i];
        printf("Node: %s\n", node.m_Name);
    }
    printf("------------------------------\n");

    printf("Skins: count: %u\n", scene->m_SkinsCount);
    for (uint32_t i = 0; i < scene->m_SkinsCount; ++i)
    {
        printf("------------------------------\n");
        OutputSkin(&scene->m_Skins[i], 1);
        printf("------------------------------\n");
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

    printf("Animations: count: %u\n", scene->m_AnimationsCount);
    for (uint32_t i = 0; i < scene->m_AnimationsCount; ++i)
    {
        printf("------------------------------\n");
        OutputAnimation(&scene->m_Animations[i], 1);
        printf("------------------------------\n");
    }
}

}
