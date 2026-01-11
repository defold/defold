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

#ifndef DM_MODELIMPORTER_H
#define DM_MODELIMPORTER_H

#include <dmsdk/dlib/align.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/shared_library.h>
#include <stdint.h>

namespace dmModelImporter
{
    // Start of JNI struct api

    static const int32_t INVALID_INDEX = 2147483647; // INT_MAX

    enum AlphaMode // same values as in cgltf.h
    {
        ALPHA_MODE_OPAQUE,
        ALPHA_MODE_MASK,
        ALPHA_MODE_BLEND,
        ALPHA_MODE_MAX_ENUM
    };

    struct Vector3
    {
        float x, y, z;
    };

    struct Quat
    {
        float x, y, z, w;
    };

    struct Transform
    {
        Quat    m_Rotation;
        Vector3 m_Translation;
        Vector3 m_Scale;
    };

    struct Aabb
    {
        Vector3 m_Min;
        Vector3 m_Max;
    };

    struct Buffer;

    // Embedded image
    struct Image // gltf
    {
        const char* m_Name;
        const char* m_Uri;      // not set if buffer is set
        const char* m_MimeType; // valid when buffer is set
        Buffer*     m_Buffer;
        uint32_t    m_Index;    // The index into the scene.images array
    };

    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-sampler
    struct Sampler
    {
        const char* m_Name;
        uint32_t    m_Index;        // The index into the scene.samplers array

        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_sampler_magfilter
        int32_t     m_MagFilter; // Optional. 0 == not set. No default
        int32_t     m_MinFilter; // Optional. 0 == not set. No default
        int32_t     m_WrapS;     // Optional. Default == 10497 (REPEAT)
        int32_t     m_WrapT;     // Optional. Default == 10497 (REPEAT)
    };

    struct Texture
    {
        const char* m_Name;
        Image*      m_Image;
        Sampler*    m_Sampler;
        Image*      m_BasisuImage;
        uint32_t    m_Index;        // The index into the scene.textures array
    };

    struct TextureTransform
    {
        float   m_Offset[2];
        float   m_Rotation;
        float   m_Scale[2];
        int     m_Texcoord;
    };

    struct TextureView
    {
        Texture*         m_Texture;
        int              m_Texcoord;
        float            m_Scale; // equivalent to strength for occlusion_texture
        bool             m_HasTransform;
        TextureTransform m_Transform;
    };

    struct PbrMetallicRoughness
    {
        TextureView     m_BaseColorTexture;
        TextureView     m_MetallicRoughnessTexture;
        float           m_BaseColorFactor[4];
        float           m_MetallicFactor;
        float           m_RoughnessFactor;
    };

    struct PbrSpecularGlossiness
    {
        TextureView     m_DiffuseTexture;
        TextureView     m_SpecularGlossinessTexture;
        float           m_DiffuseFactor[4];
        float           m_SpecularFactor[3];
        float           m_GlossinessFactor;
    };

    struct Clearcoat
    {
        TextureView     m_ClearcoatTexture;
        TextureView     m_ClearcoatRoughnessTexture;
        TextureView     m_ClearcoatNormalTexture;
        float           m_ClearcoatFactor;
        float           m_ClearcoatRoughnessFactor;
    };

    struct Transmission
    {
        TextureView     m_TransmissionTexture;
        float           m_TransmissionFactor;
    };

    struct Ior
    {
        float           m_Ior;
    };

    struct Specular
    {
        TextureView     m_SpecularTexture;
        TextureView     m_SpecularColorTexture;
        float           m_SpecularColorFactor[3];
        float           m_SpecularFactor;
    };

    struct Volume
    {
        TextureView     m_ThicknessTexture;
        float           m_ThicknessFactor;
        float           m_AttenuationColor[3];
        float           m_AttenuationDistance;
    };

    struct Sheen
    {
        TextureView     m_SheenColorTexture;
        TextureView     m_SheenRoughnessTexture;
        float           m_SheenColorFactor[3];
        float           m_SheenRoughnessFactor;
    };

    struct EmissiveStrength
    {
        float m_EmissiveStrength;
    };

    struct Iridescence
    {
        float         m_IridescenceFactor;
        TextureView   m_IridescenceTexture;
        float         m_IridescenceIor;
        float         m_IridescenceThicknessMin;
        float         m_IridescenceThicknessMax;
        TextureView   m_IridescenceThicknessTexture;
    };

    struct Material
    {
        const char* m_Name;

        // Defold
        uint32_t    m_Index;        // The index into the scene.materials array
        uint8_t     m_IsSkinned;    // If a skinned mesh is using this

        // Gltf variables
        PbrMetallicRoughness*   m_PbrMetallicRoughness;
        PbrSpecularGlossiness*  m_PbrSpecularGlossiness;
        Clearcoat*              m_Clearcoat;
        Ior*                    m_Ior;
        Specular*               m_Specular;
        Sheen*                  m_Sheen;
        Transmission*           m_Transmission;
        Volume*                 m_Volume;
        EmissiveStrength*       m_EmissiveStrength;
        Iridescence*            m_Iridescence;

        TextureView             m_NormalTexture;
        TextureView             m_OcclusionTexture;
        TextureView             m_EmissiveTexture;

        float                   m_EmissiveFactor[3];
        float                   m_AlphaCutoff;
        AlphaMode               m_AlphaMode;
        bool                    m_DoubleSided;
        bool                    m_Unlit;
    };

    struct Mesh
    {
        const char*         m_Name;
        Material*           m_Material;
        // loop using m_VertexCount * stride
        dmArray<float>      m_Positions;    // 3 floats per vertex
        dmArray<float>      m_Normals;      // 3 floats per vertex
        dmArray<float>      m_Tangents;     // 4 floats per vertex
        dmArray<float>      m_Colors;       // 4 floats per vertex
        dmArray<float>      m_Weights;      // 4 weights per vertex
        dmArray<uint32_t>   m_Bones;        // 4 bones per vertex
        uint32_t            m_TexCoords0NumComponents; // e.g 2 or 3
        dmArray<float>      m_TexCoords0;              // m_TexCoord0NumComponents floats per vertex
        uint32_t            m_TexCoords1NumComponents; // e.g 2 or 3
        dmArray<float>      m_TexCoords1;              // m_TexCoord1NumComponents floats per vertex

        Aabb                m_Aabb; // The min/max of the positions data

        dmArray<uint32_t>   m_Indices;
        uint32_t            m_VertexCount;
    };

    // forward declaration for jni generation
    struct Bone;
    struct Node;

    struct Model
    {
        const char*     m_Name;
        dmArray<Mesh>   m_Meshes;
        uint32_t        m_Index;        // The index into the scene.models array
        Bone*           m_ParentBone;   // If the model is not skinned, but a child of a bone
    };

    struct Bone
    {
        Transform       m_InvBindPose; // inverse(world_transform)
        const char*     m_Name;
        Node*           m_Node;
        Bone*           m_Parent;
        uint32_t        m_ParentIndex;  // Index into skin.bones. INVALID_INDEX if not set
        uint32_t        m_Index;        // Index into skin.bones

        // internal
        dmArray<Bone*>  m_Children;
    };

    struct Skin
    {
        const char*     m_Name;
        dmArray<Bone>   m_Bones;
        uint32_t        m_Index;        // The index into the scene.skins array

        // internal
        dmArray<uint32_t> m_BoneRemap;    // old index -> new index: for sorting the bones
    };

    struct Node
    {
        Transform       m_Local;        // The local transform
        Transform       m_World;        // The world transform
        const char*     m_Name;
        Model*          m_Model;        // not all nodes have a mesh
        Skin*           m_Skin;         // not all nodes have a skin
        Node*           m_Parent;
        dmArray<Node*>  m_Children;
        uint32_t        m_Index;        // The index into the scene.nodes array

        // internal
        uint64_t        m_NameHash;
    };

    struct KeyFrame
    {
        float m_Value[4];   // 3 for translation/scale, 4 for rotation
        float m_Time;
    };

    struct NodeAnimation
    {
        Node*               m_Node;
        dmArray<KeyFrame>   m_TranslationKeys;
        dmArray<KeyFrame>   m_RotationKeys;
        dmArray<KeyFrame>   m_ScaleKeys;
        float               m_StartTime;
        float               m_EndTime;
    };

    struct Animation
    {
        const char*             m_Name;
        dmArray<NodeAnimation>  m_NodeAnimations;
        float                   m_Duration;
    };

    struct Buffer // GLTF format
    {
        const char*     m_Uri;
        uint8_t*        m_Buffer;
        uint32_t        m_BufferCount;
    };

    struct Scene
    {
        void*       m_OpaqueSceneData;
        bool        (*m_LoadFinalizeFn)(Scene*);
        bool        (*m_ValidateFn)(Scene*);
        void        (*m_DestroyFn)(Scene*);

        // There may be more than one root node
        dmArray<Node>       m_Nodes;
        dmArray<Model>      m_Models;
        dmArray<Skin>       m_Skins;
        dmArray<Node*>      m_RootNodes;
        dmArray<Animation>  m_Animations;
        dmArray<Material>   m_Materials;
        dmArray<Sampler>    m_Samplers;
        dmArray<Image>      m_Images;
        dmArray<Texture>    m_Textures;
        dmArray<Buffer>     m_Buffers;

        // When we need to dynamically create materials
        dmArray<Material*>  m_DynamicMaterials;
    };

    struct Options
    {
        Options();

        int dummy; // for the java binding to not be zero size
    };

    // End of JNI struct api


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
