
// Copyright 2020-2024 The Defold Foundation
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

#ifndef DM_MODELIMPORTER_H
#define DM_MODELIMPORTER_H

#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/shared_library.h>
#include <stdint.h>

namespace dmModelImporter
{
    #pragma pack(push,8)

    static const uint32_t INVALID_INDEX = 0xFFFFFFFF;

    struct Aabb
    {
        float m_Min[3];
        float m_Max[3];
    };

    struct Material
    {
        const char* m_Name;
        uint32_t    m_Index;        // The index into the scene.materials array
        uint8_t     m_IsSkinned;    // If a skinned mesh is using this
    };

    struct Mesh
    {
        const char* m_Name;
        Material*   m_Material;
        // loop using m_VertexCount * stride
        float*      m_Positions;    // 3 floats per vertex
        float*      m_Normals;      // 3 floats per vertex
        float*      m_Tangents;     // 4 floats per vertex
        float*      m_Color;        // 4 floats per vertex
        float*      m_Weights;      // 4 weights per vertex
        uint32_t*   m_Bones;        // 4 bones per vertex
        uint32_t    m_TexCoord0NumComponents; // e.g 2 or 3
        float*      m_TexCoord0;              // m_TexCoord0NumComponents floats per vertex
        uint32_t    m_TexCoord1NumComponents; // e.g 2 or 3
        float*      m_TexCoord1;              // m_TexCoord1NumComponents floats per vertex

        Aabb        m_Aabb; // The min/max of the positions data

        uint32_t*   m_Indices;
        uint32_t    m_VertexCount;
        uint32_t    m_IndexCount;
    };

    struct Model
    {
        const char*     m_Name;
        Mesh*           m_Meshes;
        uint32_t        m_MeshesCount;
        uint32_t        m_Index;        // The index into the scene.models array
        struct Bone*    m_ParentBone;   // If the model is not skinned, but a child of a bone
    };

    struct DM_ALIGNED(16) Bone
    {
        dmTransform::Transform  m_InvBindPose; // inverse(world_transform)
        const char*             m_Name;
        struct Node*            m_Node;
        uint32_t                m_ParentIndex;  // Index into skin.bones. INVALID_INDEX if not set
        uint32_t                m_Index;        // Index into skin.bones

        // internal
        dmArray<Bone*>*         m_Children;
    };

    struct Skin
    {
        const char*             m_Name;
        Bone*                   m_Bones;
        uint32_t                m_BonesCount;
        uint32_t                m_Index;        // The index into the scene.skins array

        // internal
        uint32_t*               m_BoneRemap;    // old index -> new index: for sorting the bones
    };

    struct DM_ALIGNED(16) Node
    {
        dmTransform::Transform  m_Local;        // The local transform
        dmTransform::Transform  m_World;        // The world transform
        const char*             m_Name;
        Model*                  m_Model;        // not all nodes have a mesh
        Skin*                   m_Skin;         // not all nodes have a skin
        Node*                   m_Parent;
        Node**                  m_Children;
        uint32_t                m_ChildrenCount;
        uint32_t                m_Index;        // The index into the scene.nodes array

        // internal
        uint64_t                m_NameHash;
    };

    struct KeyFrame
    {
        float m_Value[4];   // 3 for translation/scale, 4 for rotation
        float m_Time;
    };

    struct NodeAnimation
    {
        Node*     m_Node;

        KeyFrame* m_TranslationKeys;
        KeyFrame* m_RotationKeys;
        KeyFrame* m_ScaleKeys;

        uint32_t  m_TranslationKeysCount;
        uint32_t  m_RotationKeysCount;
        uint32_t  m_ScaleKeysCount;
        float     m_StartTime;
        float     m_EndTime;
    };

    struct Animation
    {
        const char*     m_Name;
        NodeAnimation*  m_NodeAnimations;
        uint32_t        m_NodeAnimationsCount;
        float           m_Duration;
    };

    struct Buffer // GLTF format
    {
        const char*     m_Uri;
        void*           m_Buffer;
        uint32_t        m_BufferSize;
    };

    struct Scene
    {
        void*       m_OpaqueSceneData;
        bool        (*m_LoadFinalizeFn)(Scene*);
        bool        (*m_ValidateFn)(Scene*);
        void        (*m_DestroyFn)(Scene*);

        // There may be more than one root node
        Node*       m_Nodes;
        uint32_t    m_NodesCount;

        Model*      m_Models;
        uint32_t    m_ModelsCount;

        Skin*       m_Skins;
        uint32_t    m_SkinsCount;

        Node**      m_RootNodes;
        uint32_t    m_RootNodesCount;

        Animation*  m_Animations;
        uint32_t    m_AnimationsCount;

        Material*   m_Materials;
        uint32_t    m_MaterialsCount;

        Buffer*     m_Buffers;
        uint32_t    m_BuffersCount;

        // When we need to dynamically create materials
        Material**  m_DynamicMaterials;
        uint32_t    m_DynamicMaterialsCount;
    };

    struct Options
    {
        Options();

        int dummy; // for the java binding to not be zero size
    };


    #pragma pack(pop)

    extern "C" DM_DLLEXPORT Scene* LoadGltfFromBuffer(Options* options, void* data, uint32_t data_size);

    // GLTF: Returns true if there are unresolved data buffers
    extern "C" DM_DLLEXPORT bool NeedsResolve(Scene* scene);

    // GLTF: Loop over the buffers, and for each missing one, supply the data here
    extern "C" DM_DLLEXPORT void ResolveBuffer(Scene* scene, const char* uri, void* data, uint32_t data_size);

    extern "C" DM_DLLEXPORT Scene* LoadFromBuffer(Options* options, const char* suffix, void* data, uint32_t file_size);

    // GLTF: Finalize the load and create the actual scene structure
    extern "C" DM_DLLEXPORT bool LoadFinalize(Scene* scene);

    // GLTF: Validate after all buffers are resolved
    extern "C" DM_DLLEXPORT bool Validate(Scene* scene);

    extern "C" DM_DLLEXPORT Scene* LoadFromPath(Options* options, const char* path);

    extern "C" DM_DLLEXPORT void DestroyScene(Scene* scene);

    // Used by the editor to create a standalone data blob suitable for reading
    // Caller owns the memory
    extern "C" DM_DLLEXPORT void* ConvertToProtobufMessage(Scene* scene, size_t* length);

    // Switches between warning and debug level
    extern "C" DM_DLLEXPORT void EnableDebugLogging(bool enable);

    void DebugScene(Scene* scene);
    void DebugStructScene(Scene* scene);

    // For tests. User needs to call free() on the returned memory
    void* ReadFile(const char* path, uint32_t* file_size);
    void* ReadFileToBuffer(const char* path, uint32_t buffer_size, void* buffer);
}


#endif // DM_MODELIMPORTER_H
