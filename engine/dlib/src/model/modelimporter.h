
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

#pragma once

#include <stdint.h>

namespace dmModelImporter
{
    struct Material
    {
        const char* m_Name;
    };

    struct Mesh
    {
        float*      m_Positions;    // 3 floats per vertex
        float*      m_Normals;      // 3 floats per vertex
        float*      m_Tangents;     // 3 floats per vertex
        float*      m_Color;        // 4 floats per vertex
        float*      m_Weights;      // 4 weights per vertex
        uint32_t*   m_Bones;        // 4 bones per vertex
        uint32_t    m_TexCoord0NumComponents; // e.g 2 or 3
        float*      m_TexCoord0;              // m_TexCoord0NumComponents floats per vertex
        uint32_t    m_TexCoord1NumComponents; // e.g 2 or 3
        float*      m_TexCoord1;              // m_TexCoord1NumComponents floats per vertex
        uint32_t    m_VertexCount;
        const char* m_Material;
    };

    struct Model
    {
        const char* m_Name;
        Mesh*       m_Meshes;
        uint32_t    m_MeshesCount;
    };

    // struct Skin
    // {
    // };

    struct Bone
    {
    };

    struct Node
    {
        const char* m_Name;
        Node*       m_Parent;
        Node**      m_Children;
        uint32_t    m_ChildrenCount;
        Model*      m_Model; // not all nodes have a mesh
        //Skin*   m_Skin; // not all nodes have a skin
    };

    struct Scene
    {
        void*       m_OpaqueSceneData;
        void        (*m_DestroyFn)(void* opaque_scene_data);

        // There may be more than one root node
        Node*       m_Nodes;
        uint32_t    m_NodesCount;

        Model*      m_Models;
        uint32_t    m_ModelsCount;

        Node*       m_Skeleton; // The skeleton top node

        Node**      m_RootNodes;
        uint32_t    m_RootNodesCount;
    };

    struct Options
    {
        Options();
    };

    Scene* LoadGltf(Options* options, void* data, uint32_t file_size);

    void DestroyScene(Scene* scene);

    void DebugScene(Scene* scene);

    // For tests. User needs to call free() on the returned memory
    void* ReadFile(const char* path, uint32_t* file_size);

    // // Scene

    // struct Scene;
    // struct Node;

    // typedef Scene* HScene;
    // typedef Node*   HNode;

    // HScene LoadScene(uint8_t* data, uint32_t file_size);

    // uint32_t GetNumNodes(HScene scene);
    // HNode    GetNode(HScene scene, uint32_t index);

    // // Nodes
    // const char* GetNodeName(HNode node);
    // uint32_t    GetNumChildren(HNode node);


}
