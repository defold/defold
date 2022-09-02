
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

static void OutputTransform(const dmTransform::Transform& transform)
{
    printf("t: %f, %f, %f  ", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
    printf("r: %f, %f, %f, %f  ", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
    printf("s: %f, %f, %f  ", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
}

static void OutputVector4(const dmVMath::Vector4& v)
{
    printf("%f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
}

static void OutputMatrix(const dmTransform::Transform& transform)
{
    dmVMath::Matrix4 mat = dmTransform::ToMatrix4(transform);
    printf("    "); OutputVector4(mat.getRow(0));
    printf("    "); OutputVector4(mat.getRow(1));
    printf("    "); OutputVector4(mat.getRow(2));
    printf("    "); OutputVector4(mat.getRow(3));
}

static void OutputBone(Bone* bone, int indent)
{
    OutputIndent(indent);
    printf("%s  idx: %u parent: %u node: %s inv_bind_pose:\n", bone->m_Name, bone->m_Index, bone->m_ParentIndex, bone->m_Node?bone->m_Node->m_Name:"null");
    OutputMatrix(bone->m_InvBindPose);
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

static void OutputNode(Node* node)
{
    printf("Node: %s : \n", node->m_Name);
    printf("  local\n");
    OutputMatrix(node->m_Local);
    printf("\n  world\n");
    OutputMatrix(node->m_World);
    printf("\n");
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
    printf("mesh  %s  vertices: %u  indices: %u mat: %s  weights: %s\n", mesh->m_Name, mesh->m_VertexCount, mesh->m_IndexCount, mesh->m_Material, mesh->m_Weights?"yes":"no");

    // if (mesh->m_Weights)
    // {
    //     for (uint32_t i = 0; i < mesh->m_VertexCount; ++i)
    //     {
    //         printf("  %4u  B: %2u, %2u, %2u, %2u\t", i, mesh->m_Bones[i*4+0], mesh->m_Bones[i*4+1], mesh->m_Bones[i*4+2], mesh->m_Bones[i*4+3]);
    //         printf("  W: %f, %f, %f, %f\n", mesh->m_Weights[i*4+0], mesh->m_Weights[i*4+1], mesh->m_Weights[i*4+2], mesh->m_Weights[i*4+3]);
    //     }
    // }
    //printf("tris: %u  material: %s", mesh->)

    // for (uint32_t i = 0; i < mesh->m_IndexCount; ++i)
    // {
    //     printf("%3d\t", mesh->m_Indices[i]);
    //     if (((i+1) % 16) == 0)
    //         printf("\n");
    // }
    // printf("\n");
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

    indent++;
    OutputIndent(indent);
    printf("# translation keys: %u\n", node_animation->m_TranslationKeysCount);
    for (uint32_t i = 0; i < node_animation->m_TranslationKeysCount; ++i)
    {
        OutputIndent(indent+1);
        KeyFrame* key = &node_animation->m_TranslationKeys[i];
        printf("%.3f:  %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2]);
    }

    OutputIndent(indent);
    printf("# rotation keys: %u\n", node_animation->m_RotationKeysCount);
    for (uint32_t i = 0; i < node_animation->m_RotationKeysCount; ++i)
    {
        OutputIndent(indent+1);
        KeyFrame* key = &node_animation->m_RotationKeys[i];
        printf("%.3f:  %.3f, %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2], key->m_Value[3]);
    }

    OutputIndent(indent);
    printf("# scale keys: %u\n", node_animation->m_ScaleKeysCount);
    for (uint32_t i = 0; i < node_animation->m_ScaleKeysCount; ++i)
    {
        OutputIndent(indent+1);
        KeyFrame* key = &node_animation->m_ScaleKeys[i];
        printf("%.3f:  %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2]);
    }
}

static void OutputAnimation(Animation* animation, int indent)
{
    OutputIndent(indent);
    printf("%s duration: %f\n", animation->m_Name, animation->m_Duration);

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
        OutputNode(&scene->m_Nodes[i]);
    }
    printf("------------------------------\n");

    printf("Subscenes: count: %u\n", scene->m_RootNodesCount);
    for (uint32_t i = 0; i < scene->m_RootNodesCount; ++i)
    {
        printf("------------------------------\n");
        OutputNodeTree(scene->m_RootNodes[i], 1);
        printf("------------------------------\n");
    }

    printf("Skins: count: %u\n", scene->m_SkinsCount);
    for (uint32_t i = 0; i < scene->m_SkinsCount; ++i)
    {
        printf("------------------------------\n");
        OutputSkin(&scene->m_Skins[i], 1);
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
    printf("Output model importer scene done\n");
}

static void DebugStructNode(Node* node, int indent)
{
    OutputIndent(indent); printf("Node: %p\n", node);
    assert(node->m_Name);
    OutputIndent(indent); printf("  m_Local: .\n");
    OutputIndent(indent); printf("  m_World: .\n");
    OutputIndent(indent); printf("  m_Name: %p (%s)\n", node->m_Name, node->m_Name);
    OutputIndent(indent); printf("  m_Model: %p\n", node->m_Model);
    OutputIndent(indent); printf("  m_Skin: %p\n", node->m_Skin);
    OutputIndent(indent); printf("  m_Parent: %p\n", node->m_Parent);
    OutputIndent(indent); printf("  m_Children: %p\n", node->m_Children);
    OutputIndent(indent); printf("  m_ChildrenCount: %u\n", node->m_ChildrenCount);
}

static void DebugStructNodeTree(Node* node, int indent)
{
    DebugStructNode(node, indent);

    for (uint32_t i = 0; i < node->m_ChildrenCount; ++i)
    {
        DebugStructNode(node->m_Children[i], indent+1);
    }
}

static void DebugStructMesh(Mesh* mesh, int indent)
{
    OutputIndent(indent); printf("Mesh: %p\n", mesh);
    assert(mesh->m_Name);
    assert(mesh->m_Material);
    OutputIndent(indent); printf("  m_Name: %p (%s)\n", mesh->m_Name, mesh->m_Name);
    OutputIndent(indent); printf("  m_Material: %p (%s)\n", mesh->m_Material, mesh->m_Material);

    OutputIndent(indent); printf("  m_Positions: %p\n", mesh->m_Positions);
    OutputIndent(indent); printf("  m_Normals: %p\n", mesh->m_Normals);
    OutputIndent(indent); printf("  m_Tangents: %p\n", mesh->m_Tangents);
    OutputIndent(indent); printf("  m_Color: %p\n", mesh->m_Color);
    OutputIndent(indent); printf("  m_Weights: %p\n", mesh->m_Weights);
    OutputIndent(indent); printf("  m_Bones: %p\n", mesh->m_Bones);

    OutputIndent(indent); printf("  m_TexCoord0: %p\n", mesh->m_TexCoord0);
    OutputIndent(indent); printf("  m_TexCoord0NumComponents: %u\n", mesh->m_TexCoord0NumComponents);
    OutputIndent(indent); printf("  m_TexCoord1: %p\n", mesh->m_TexCoord1);
    OutputIndent(indent); printf("  m_TexCoord1NumComponents: %u\n", mesh->m_TexCoord1NumComponents);

    OutputIndent(indent); printf("  m_Indices: %p\n", mesh->m_Indices);
    OutputIndent(indent); printf("  m_VertexCount: %u\n", mesh->m_VertexCount);
    OutputIndent(indent); printf("  m_IndexCount: %u\n", mesh->m_IndexCount);
}

static void DebugStructModel(Model* model, int indent)
{
    OutputIndent(indent); printf("Model: %p\n", model);
    assert(model->m_Name);
    OutputIndent(indent); printf("  m_Name: %p (%s)\n", model->m_Name, model->m_Name);
    OutputIndent(indent); printf("  m_Meshes: %p\n", model->m_Meshes);
    OutputIndent(indent); printf("  m_MeshesCount: %u\n", model->m_MeshesCount);

    for (uint32_t i = 0; i < model->m_MeshesCount; ++i) {
        DebugStructMesh(&model->m_Meshes[i], indent+1);
        OutputIndent(indent+1); printf("-------------------------------\n");
    }
}

static void DebugStructSkin(Skin* skin, int indent)
{
    OutputIndent(indent); printf("Skin: %p\n", skin);
    assert(skin->m_Name);
    OutputIndent(indent); printf("  m_Name: %p (%s)\n", skin->m_Name, skin->m_Name);
    // OutputIndent(indent); printf("  m_Model: %p\n", node->m_Model);
    // OutputIndent(indent); printf("  m_Skin: %p\n", node->m_Skin);
    // OutputIndent(indent); printf("  m_Parent: %p\n", node->m_Parent);
    // OutputIndent(indent); printf("  m_Children: %p\n", node->m_Children);
    // OutputIndent(indent); printf("  m_ChildrenCount: %u\n", node->m_ChildrenCount);
}

void DebugStructScene(Scene* scene)
{
    printf("Scene: %p\n", scene);
    printf("  m_OpaqueSceneData: %p\n", scene->m_OpaqueSceneData);
    printf("  m_DestroyFn: %p\n", scene->m_DestroyFn);
    printf("  m_Nodes: %p\n", scene->m_Nodes);
    printf("  m_NodesCount: %u\n", scene->m_NodesCount);
    printf("  m_Models: %p\n", scene->m_Models);
    printf("  m_ModelsCount: %u\n", scene->m_ModelsCount);
    printf("  m_Skins: %p\n", scene->m_Skins);
    printf("  m_SkinsCount: %u\n", scene->m_SkinsCount);
    printf("  m_RootNodes: %p\n", scene->m_RootNodes);
    printf("  m_RootNodesCount: %u\n", scene->m_RootNodesCount);
    printf("  m_Animations: %p\n", scene->m_Animations);
    printf("  m_AnimationsCount: %u\n", scene->m_AnimationsCount);

    printf("-------------------------------\n");
    for (uint32_t i = 0; i < scene->m_NodesCount; ++i) {
        DebugStructNode(&scene->m_Nodes[i], 1);
        printf("-------------------------------\n");
    }
    for (uint32_t i = 0; i < scene->m_ModelsCount; ++i) {
        DebugStructModel(&scene->m_Models[i], 1);
        printf("-------------------------------\n");
    }
    for (uint32_t i = 0; i < scene->m_SkinsCount; ++i) {
        DebugStructSkin(&scene->m_Skins[i], 1);
        printf("-------------------------------\n");
    }
    for (uint32_t i = 0; i < scene->m_RootNodesCount; ++i) {
        DebugStructNodeTree(scene->m_RootNodes[i], 1);
        printf("-------------------------------\n");
    }
}

}
