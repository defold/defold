
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

// SPEC:
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

#include "modelimporter.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <algorithm> // std::sort
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/log.h>

// Terminology:
// Defold: Model, GLTF: Mesh
// Defold: Mesh,  GLTF: Primitive

namespace dmModelImporter
{
struct GltfData
{
    cgltf_data* m_Data;
};

static dmTransform::Transform& ToTransform(const dmModelImporter::Transform& in, dmTransform::Transform& out)
{
    out = dmTransform::Transform(dmVMath::Vector3(in.m_Translation.x, in.m_Translation.y, in.m_Translation.z),
                                 dmVMath::Quat(in.m_Rotation.x, in.m_Rotation.y, in.m_Rotation.z, in.m_Rotation.w),
                                 dmVMath::Vector3(in.m_Scale.x, in.m_Scale.y, in.m_Scale.z));
    return out;
}

static dmModelImporter::Transform& FromTransform(const dmTransform::Transform& in, dmModelImporter::Transform& out)
{
    memcpy(&out.m_Translation.x, in.GetPositionPtr(), sizeof(out.m_Translation));
    memcpy(&out.m_Rotation.x, in.GetRotationPtr(), sizeof(out.m_Rotation));
    memcpy(&out.m_Scale.x, in.GetScalePtr(), sizeof(out.m_Scale));
    return out;
}

static dmModelImporter::Transform& FromMatrix4x4(const float* m, dmModelImporter::Transform& out)
{
    dmVMath::Matrix4 mat = dmVMath::Matrix4(dmVMath::Vector4(m[0], m[1], m[2], m[3]),
                                            dmVMath::Vector4(m[4], m[5], m[6], m[7]),
                                            dmVMath::Vector4(m[8], m[9], m[10], m[11]),
                                            dmVMath::Vector4(m[12], m[13], m[14], m[15]));
    dmTransform::Transform transform = dmTransform::ToTransform(mat);
    return FromTransform(transform, out);
}

static dmModelImporter::Transform& SetIdentity(dmModelImporter::Transform& out)
{
    dmTransform::Transform identity;
    identity.SetIdentity();
    return FromTransform(identity, out);
}

static dmModelImporter::Transform& TransformMul(const dmModelImporter::Transform& _a, const dmModelImporter::Transform& _b, dmModelImporter::Transform& out)
{
    dmTransform::Transform a;
    dmTransform::Transform b;
    ToTransform(_a, a);
    ToTransform(_b, b);

    dmTransform::Transform result = dmTransform::Mul(a, b);
    return FromTransform(result, out);
}

// static void OutputTransform(const dmTransform::Transform& transform)
// {
//     printf("    t: %f, %f, %f\n", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
//     printf("    r: %f, %f, %f, %f\n", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
//     printf("    s: %f, %f, %f\n", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
// }

// static void printVector4(const dmVMath::Vector4& v)
// {
//     printf("%f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
// }

// static void OutputMatrix(const dmTransform::Transform& transform)
// {
//     dmVMath::Matrix4 m = dmTransform::ToMatrix4(transform);
//     printf("        "); printVector4(m.getRow(0));
//     printf("        "); printVector4(m.getRow(1));
//     printf("        "); printVector4(m.getRow(2));
//     printf("        "); printVector4(m.getRow(3));
// }

// static const char* GetPrimitiveTypeStr(cgltf_primitive_type type)
// {
//     switch(type)
//     {
//     case cgltf_primitive_type_points: return "cgltf_primitive_type_points";
//     case cgltf_primitive_type_lines: return "cgltf_primitive_type_lines";
//     case cgltf_primitive_type_line_loop: return "cgltf_primitive_type_line_loop";
//     case cgltf_primitive_type_line_strip: return "cgltf_primitive_type_line_strip";
//     case cgltf_primitive_type_triangles: return "cgltf_primitive_type_triangles";
//     case cgltf_primitive_type_triangle_strip: return "cgltf_primitive_type_triangle_strip";
//     case cgltf_primitive_type_triangle_fan: return "cgltf_primitive_type_triangle_fan";
//     default: return "unknown";
//     }
// }

// static const char* GetAttributeTypeStr(cgltf_attribute_type type)
// {
//     switch(type)
//     {
//     case cgltf_attribute_type_invalid: return "cgltf_attribute_type_invalid";
//     case cgltf_attribute_type_position: return "cgltf_attribute_type_position";
//     case cgltf_attribute_type_normal: return "cgltf_attribute_type_normal";
//     case cgltf_attribute_type_tangent: return "cgltf_attribute_type_tangent";
//     case cgltf_attribute_type_texcoord: return "cgltf_attribute_type_texcoord";
//     case cgltf_attribute_type_color: return "cgltf_attribute_type_color";
//     case cgltf_attribute_type_joints: return "cgltf_attribute_type_joints";
//     case cgltf_attribute_type_weights: return "cgltf_attribute_type_weights";
//     default: return "unknown";
//     }
// }

// static const char* GetTypeStr(cgltf_type type)
// {
//     switch(type)
//     {
//     case cgltf_type_invalid: return "cgltf_type_invalid";
//     case cgltf_type_scalar: return "cgltf_type_scalar";
//     case cgltf_type_vec2: return "cgltf_type_vec2";
//     case cgltf_type_vec3: return "cgltf_type_vec3";
//     case cgltf_type_vec4: return "cgltf_type_vec4";
//     case cgltf_type_mat2: return "cgltf_type_mat2";
//     case cgltf_type_mat3: return "cgltf_type_mat3";
//     case cgltf_type_mat4: return "cgltf_type_mat4";
//     default: return "unknown";
//     }
// }

static const char* GetResultStr(cgltf_result result)
{
    switch(result)
    {
    case cgltf_result_success: return "cgltf_result_success";
    case cgltf_result_data_too_short: return "cgltf_result_data_too_short";
    case cgltf_result_unknown_format: return "cgltf_result_unknown_format";
    case cgltf_result_invalid_json: return "cgltf_result_invalid_json";
    case cgltf_result_invalid_gltf: return "cgltf_result_invalid_gltf";
    case cgltf_result_invalid_options: return "cgltf_result_invalid_options";
    case cgltf_result_file_not_found: return "cgltf_result_file_not_found";
    case cgltf_result_io_error: return "cgltf_result_io_error";
    case cgltf_result_out_of_memory: return "cgltf_result_out_of_memory";
    case cgltf_result_legacy_gltf: return "cgltf_result_legacy_gltf";
    default: return "unknown";
    }
}

// static const char* GetAnimationPathTypeStr(cgltf_animation_path_type type)
// {
//     switch(type)
//     {
//     case cgltf_animation_path_type_invalid: return "cgltf_animation_path_type_invalid";
//     case cgltf_animation_path_type_translation: return "cgltf_animation_path_type_translation";
//     case cgltf_animation_path_type_rotation: return "cgltf_animation_path_type_rotation";
//     case cgltf_animation_path_type_scale: return "cgltf_animation_path_type_scale";
//     case cgltf_animation_path_type_weights: return "cgltf_animation_path_type_weights";
//     default: return "unknown";
//     }
// }

// static const char* GetInterpolationTypeStr(cgltf_interpolation_type type)
// {
//  switch(type)
//  {
//  case cgltf_interpolation_type_linear: return "cgltf_interpolation_type_linear";
//  case cgltf_interpolation_type_step: return "cgltf_interpolation_type_step";
//  case cgltf_interpolation_type_cubic_spline: return "cgltf_interpolation_type_cubic_spline";
//  default: return "unknown";
//  }
// }


static float* ReadAccessorFloat(cgltf_accessor* accessor, uint32_t desired_num_components, float default_value, uint32_t* out_count)
{
    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);

    if (desired_num_components == 0)
        desired_num_components = num_components;

    uint32_t size = accessor->count * num_components;
    if (desired_num_components > num_components)
        size = accessor->count * desired_num_components;

    *out_count = size;
    float* out = new float[size]; // Now the buffer will fit the max num components
    float* writeptr = out;

    if (desired_num_components > num_components)
    {
        for (uint32_t i = 0; i < accessor->count; ++i)
        {
            for (uint32_t j = 0; j < desired_num_components; ++j)
                *writeptr++ = default_value;
        }
    }

    writeptr = out;

    for (uint32_t i = 0; i < accessor->count; ++i)
    {
        bool result = cgltf_accessor_read_float(accessor, i, writeptr, num_components);

        if (!result)
        {
            printf("Couldn't read floats!\n");
            delete[] out;
            return 0;
        }

        writeptr += desired_num_components;
    }

    return out;
}

static uint32_t* ReadAccessorUint32ToMem(cgltf_accessor* accessor, uint32_t num_components, uint32_t* out)
{
    uint32_t* writeptr = out;
    for (uint32_t i = 0; i < accessor->count; ++i)
    {
        bool result = cgltf_accessor_read_uint(accessor, i, writeptr, num_components);

        if (!result)
        {
            printf("couldnt read uints!\n");
            return 0;
        }

        writeptr += num_components;
    }
    return out;
}

static uint32_t* ReadAccessorUint32(cgltf_accessor* accessor, uint32_t desired_num_components, uint32_t* out_size)
{
    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);

    if (desired_num_components == 0)
        desired_num_components = num_components;

    *out_size = accessor->count * desired_num_components;
    uint32_t* out = new uint32_t[*out_size];
    return ReadAccessorUint32ToMem(accessor, desired_num_components, out);
}

static void ReadAccessorUint32ToArray(cgltf_accessor* accessor, uint32_t desired_num_components, dmArray<uint32_t>& out)
{
    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);

    if (desired_num_components == 0)
        desired_num_components = num_components;

    out.SetCapacity(accessor->count * desired_num_components);
    out.SetSize(out.Capacity());
    ReadAccessorUint32ToMem(accessor, desired_num_components, out.Begin());
}


static float* ReadAccessorMatrix4(cgltf_accessor* accessor, uint32_t index, float* out)
{
    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);
    assert(num_components == 16);

    bool result = cgltf_accessor_read_float(accessor, index, out, 16);

    if (!result)
    {
        printf("Couldn't read floats!\n");
        return 0;
    }

    return out;
}

static uint32_t FindNodeIndex(cgltf_node* node, uint32_t nodes_count, cgltf_node* nodes)
{
    for (uint32_t i = 0; i < nodes_count; ++i)
    {
        if (&nodes[i] == node)
            return i;
    }
    assert(false && "Failed to find node in list of nodes");
    return INVALID_INDEX;
}

static Node* TranslateNode(cgltf_node* node, cgltf_data* gltf_data, Scene* scene)
{
    uint32_t index = FindNodeIndex(node, gltf_data->nodes_count, gltf_data->nodes);
    return &scene->m_Nodes[index];
}


static Skin* FindSkin(Scene* scene, cgltf_data* gltf_data, cgltf_skin* gltf_skin)
{
    for (uint32_t i = 0; i < gltf_data->skins_count; ++i)
    {
        if (gltf_skin == &gltf_data->skins[i])
            return &scene->m_Skins[i];
    }
    return 0;
}

template<typename T>
static uint32_t FindIndex(T* base_pointer, T* pointer)
{
    if (!pointer)
        return INVALID_INDEX;
    return (uint32_t)(pointer - base_pointer);
}

static char* CreateNameFromHash(const char* prefix, uint32_t hash)
{
    char buffer[128];
    dmSnPrintf(buffer, sizeof(buffer), "%s_%X", prefix, hash);
    return strdup(buffer);
}

template <typename T>
static char* CreateObjectName(T* object, const char* prefix, uint32_t index)
{
    if (object && object->name)
        return strdup(object->name);
    return CreateNameFromHash(prefix, index);
}

template <>
char* CreateObjectName<>(cgltf_primitive* object, const char* prefix, uint32_t index)
{
    (void)object;
    return CreateNameFromHash("mesh", index);
}

template <>
char* CreateObjectName<>(cgltf_buffer* object, const char* prefix, uint32_t index)
{
    if (object && object->name)
        return strdup(object->name);
    if (object && object->uri)
        return strdup(object->uri);
    return CreateNameFromHash("buffer", index);
}

static void UpdateWorldTransforms(Node* node)
{
    if (node->m_Parent)
        TransformMul(node->m_Parent->m_World, node->m_Local, node->m_World);
    else
        node->m_World = node->m_Local;

    for (uint32_t c = 0; c < node->m_Children.Size(); ++c)
    {
        UpdateWorldTransforms(node->m_Children[c]);
    }
}

template <typename T>
static void InitSize(dmArray<T>& arr, uint32_t cap, uint32_t size)
{
    arr.SetCapacity(cap);
    arr.SetSize(size);
    memset(arr.Begin(), 0, sizeof(T)*cap);
}

static void LoadNodes(Scene* scene, cgltf_data* gltf_data)
{
    // We allocate one extra node, in case we need it for later
    // In the case we generate a root bone, we want a valid node for it.
    InitSize(scene->m_Nodes, gltf_data->nodes_count+1, gltf_data->nodes_count);

    for (size_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node* gltf_node = &gltf_data->nodes[i];

        Node* node = &scene->m_Nodes[i];
        node->m_Name = CreateObjectName(gltf_node, "node", i);
        node->m_NameHash = dmHashString64(node->m_Name);
        node->m_Index = i;

        // We link them together later
        // node->m_Model = ...

        dmVMath::Vector3 scale = dmVMath::Vector3(1,1,1);
        dmVMath::Vector3 translation = dmVMath::Vector3(0,0,0);
        dmVMath::Quat rotation = dmVMath::Quat(0,0,0,1);

        node->m_Skin = 0;
        if (gltf_node->skin)
        {
            node->m_Skin = FindSkin(scene, gltf_data, gltf_node->skin);
        }

        if (gltf_node->has_translation)
            translation = dmVMath::Vector3(gltf_node->translation[0], gltf_node->translation[1], gltf_node->translation[2]);
        if (gltf_node->has_scale)
            scale = dmVMath::Vector3(gltf_node->scale[0], gltf_node->scale[1], gltf_node->scale[2]);
        if (gltf_node->has_rotation)
            rotation = dmVMath::Quat(gltf_node->rotation[0], gltf_node->rotation[1], gltf_node->rotation[2], gltf_node->rotation[3]);

        if (gltf_node->has_matrix)
        {
            FromMatrix4x4(gltf_node->matrix, node->m_Local);
        }
        else
        {
            FromTransform(dmTransform::Transform(translation, rotation, scale), node->m_Local);
        }
    }

    // find all the parents and all the children

    for (size_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node* gltf_node = &gltf_data->nodes[i];
        Node* node = &scene->m_Nodes[i];

        node->m_Parent = gltf_node->parent ? TranslateNode(gltf_node->parent, gltf_data, scene) : 0;

        InitSize(node->m_Children, gltf_node->children_count, gltf_node->children_count);
        for (uint32_t c = 0; c < gltf_node->children_count; ++c)
            node->m_Children[c] = TranslateNode(gltf_node->children[c], gltf_data, scene);
    }

    // Find root nodes
    InitSize(scene->m_RootNodes, 0, 0);
    for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
    {
        Node* node = &scene->m_Nodes[i];
        if (node->m_Parent == 0)
        {
            if (scene->m_RootNodes.Full())
                scene->m_RootNodes.OffsetCapacity(32);
            scene->m_RootNodes.Push(node);
            UpdateWorldTransforms(node);
        }
    }
}

static void LoadSamplers(Scene* scene, cgltf_data* gltf_data)
{
    InitSize(scene->m_Samplers, gltf_data->samplers_count, gltf_data->samplers_count);

    for (uint32_t i = 0; i < gltf_data->samplers_count; ++i)
    {
        cgltf_sampler* gltf_sampler = &gltf_data->samplers[i];
        Sampler* sampler = &scene->m_Samplers[i];
        memset(sampler, 0, sizeof(*sampler));
        sampler->m_Name = CreateObjectName(gltf_sampler, "sampler", i);

        sampler->m_MagFilter = gltf_sampler->mag_filter;
        sampler->m_MinFilter = gltf_sampler->min_filter;
        sampler->m_WrapS = gltf_sampler->wrap_s;
        sampler->m_WrapT = gltf_sampler->wrap_t;
    }
}

static void LoadMaterials(Scene* scene, cgltf_data* gltf_data)
{
    InitSize(scene->m_Materials, gltf_data->materials_count, gltf_data->materials_count);

    for (uint32_t i = 0; i < gltf_data->materials_count; ++i)
    {
        cgltf_material* gltf_material = &gltf_data->materials[i];
        Material* material = &scene->m_Materials[i];
        material->m_Name = CreateObjectName(gltf_material, "material", i);
        material->m_Index = i;

        // todo: load properties
        // todo: what is "material mappings"?
        // todo: and how is the material variant used?

// typedef struct cgltf_pbr_metallic_roughness
// {
//     cgltf_texture_view base_color_texture;
//     cgltf_texture_view metallic_roughness_texture;

//     cgltf_float base_color_factor[4];
//     cgltf_float metallic_factor;
//     cgltf_float roughness_factor;
// } cgltf_pbr_metallic_roughness;

// typedef struct cgltf_pbr_specular_glossiness
// {
//     cgltf_texture_view diffuse_texture;
//     cgltf_texture_view specular_glossiness_texture;

//     cgltf_float diffuse_factor[4];
//     cgltf_float specular_factor[3];
//     cgltf_float glossiness_factor;
// } cgltf_pbr_specular_glossiness;

// typedef struct cgltf_clearcoat
// {
//     cgltf_texture_view clearcoat_texture;
//     cgltf_texture_view clearcoat_roughness_texture;
//     cgltf_texture_view clearcoat_normal_texture;

//     cgltf_float clearcoat_factor;
//     cgltf_float clearcoat_roughness_factor;
// } cgltf_clearcoat;

// typedef struct cgltf_transmission
// {
//     cgltf_texture_view transmission_texture;
//     cgltf_float transmission_factor;
// } cgltf_transmission;

// typedef struct cgltf_ior
// {
//     cgltf_float ior;
// } cgltf_ior;

//         cgltf_bool double_sided;
//         cgltf_bool unlit;


//         // cgltf_bool has_pbr_metallic_roughness;
//         // cgltf_bool has_pbr_specular_glossiness;
//         // cgltf_bool has_clearcoat;
//         // cgltf_bool has_transmission;
//         // cgltf_bool has_volume;
//         // cgltf_bool has_ior;
//         // cgltf_bool has_specular;
//         // cgltf_bool has_sheen;
//         // cgltf_bool has_emissive_strength;

//         // cgltf_pbr_metallic_roughness pbr_metallic_roughness;
//         // cgltf_pbr_specular_glossiness pbr_specular_glossiness;
//         // cgltf_clearcoat clearcoat;
//         // cgltf_ior ior;
//         // cgltf_specular specular;
//         // cgltf_sheen sheen;
//         // cgltf_transmission transmission;
//         // cgltf_volume volume;
//         // cgltf_emissive_strength emissive_strength;
//         // cgltf_texture_view normal_texture;
//         // cgltf_texture_view occlusion_texture;
//         // cgltf_texture_view emissive_texture;
//         // cgltf_float emissive_factor[3];
//         // cgltf_alpha_mode alpha_mode;
//         // cgltf_float alpha_cutoff;
//         // cgltf_bool double_sided;
//         // cgltf_bool unlit;

//         // todo: extensions

//         // cgltf_size extensions_count;
//         // cgltf_extension* extensions;

// typedef struct cgltf_material
// {
//     char* name;
//     cgltf_bool has_pbr_metallic_roughness;
//     cgltf_bool has_pbr_specular_glossiness;
//     cgltf_bool has_clearcoat;
//     cgltf_bool has_transmission;
//     cgltf_bool has_volume;
//     cgltf_bool has_ior;
//     cgltf_bool has_specular;
//     cgltf_bool has_sheen;
//     cgltf_bool has_emissive_strength;
//     cgltf_bool has_iridescence;
//     cgltf_pbr_metallic_roughness pbr_metallic_roughness;
//     cgltf_pbr_specular_glossiness pbr_specular_glossiness;
//     cgltf_clearcoat clearcoat;
//     cgltf_ior ior;
//     cgltf_specular specular;
//     cgltf_sheen sheen;
//     cgltf_transmission transmission;
//     cgltf_volume volume;
//     cgltf_emissive_strength emissive_strength;
//     cgltf_iridescence iridescence;
//     cgltf_texture_view normal_texture;
//     cgltf_texture_view occlusion_texture;
//     cgltf_texture_view emissive_texture;
//     cgltf_float emissive_factor[3];
//     cgltf_alpha_mode alpha_mode;
//     cgltf_float alpha_cutoff;
//     cgltf_bool double_sided;
//     cgltf_bool unlit;
//     cgltf_extras extras;
//     cgltf_size extensions_count;
//     cgltf_extension* extensions;
// } cgltf_material;

    }
}

static void CalcAABB(uint32_t count, float* positions, Aabb* aabb)
{
    aabb->m_Min.x = aabb->m_Min.y = aabb->m_Min.z = FLT_MAX;
    aabb->m_Max.x = aabb->m_Max.y = aabb->m_Max.z = -FLT_MAX;
    for (uint32_t j = 0; j < count; j += 3, positions += 3)
    {
        aabb->m_Min.x = dmMath::Min(aabb->m_Min.x, positions[0]);
        aabb->m_Min.y = dmMath::Min(aabb->m_Min.y, positions[1]);
        aabb->m_Min.z = dmMath::Min(aabb->m_Min.z, positions[2]);

        aabb->m_Max.x = dmMath::Max(aabb->m_Max.x, positions[0]);
        aabb->m_Max.y = dmMath::Max(aabb->m_Max.y, positions[1]);
        aabb->m_Max.z = dmMath::Max(aabb->m_Max.z, positions[2]);
    }
}

static void AddDynamicMaterial(Scene* scene, Material* material)
{
    if (scene->m_DynamicMaterials.Full())
        scene->m_DynamicMaterials.OffsetCapacity(8);
    scene->m_DynamicMaterials.Push(material);
}

static void LoadPrimitives(Scene* scene, Model* model, cgltf_data* gltf_data, cgltf_mesh* gltf_mesh)
{
    InitSize(model->m_Meshes, gltf_mesh->primitives_count, gltf_mesh->primitives_count);

    for (size_t i = 0; i < gltf_mesh->primitives_count; ++i)
    {
        cgltf_primitive* prim = &gltf_mesh->primitives[i];
        Mesh* mesh = &model->m_Meshes[i];
        mesh->m_Name = CreateObjectName(prim, "mesh", i);

        uint32_t material_index = FindIndex(gltf_data->materials, prim->material);
        if (material_index != INVALID_INDEX)
        {
            mesh->m_Material = &scene->m_Materials[material_index];
            mesh->m_VertexCount = 0;
        }

        //printf("primitive_type: %s\n", getPrimitiveTypeStr(prim->type));

        ReadAccessorUint32ToArray(prim->indices, 1, mesh->m_Indices);

        for (uint32_t a = 0; a < prim->attributes_count; ++a)
        {
            cgltf_attribute* attribute = &prim->attributes[a];
            cgltf_accessor* accessor = attribute->data;

            mesh->m_VertexCount = accessor->count;

            uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);
            uint32_t desired_num_components = num_components;

            //printf("  attributes: %s   index: %u   type: %s  count: %u desired_num_components: %u\n", attribute->name, attribute->index, GetAttributeTypeStr(attribute->type), (uint32_t)accessor->count, desired_num_components);

            float default_value_f = 0.0f;
            if (attribute->type == cgltf_attribute_type_tangent)
            {
                desired_num_components = 4; // xyz + handedness
            }
            else if (attribute->type == cgltf_attribute_type_color)
            {
                desired_num_components = 4; // We currently always store vec4
                default_value_f = 1.0f; // for the alpha channel
            }

            float* fdata = 0;
            uint32_t* udata = 0;
            uint32_t data_count = 0;

            if (attribute->type == cgltf_attribute_type_joints)
            {
                udata = ReadAccessorUint32(accessor, desired_num_components, &data_count);
            }
            else
            {
                fdata = ReadAccessorFloat(accessor, desired_num_components, default_value_f, &data_count);
            }

            if (fdata || udata)
            {
                if (attribute->type == cgltf_attribute_type_position)
                {
                    mesh->m_Positions.Set(fdata, data_count, data_count, false);
                    if (accessor->has_min && accessor->has_max) {
                        memcpy(&mesh->m_Aabb.m_Min.x, accessor->min, sizeof(float)*3);
                        memcpy(&mesh->m_Aabb.m_Max.x, accessor->max, sizeof(float)*3);
                    }
                    else
                    {
                        CalcAABB(mesh->m_VertexCount*3, fdata, &mesh->m_Aabb);
                    }
                }
                else if (attribute->type == cgltf_attribute_type_normal) {
                    mesh->m_Normals.Set(fdata, data_count, data_count, false);
                }
                else if (attribute->type == cgltf_attribute_type_tangent) {
                    mesh->m_Tangents.Set(fdata, data_count, data_count, false);
                }
                else if (attribute->type == cgltf_attribute_type_texcoord)
                {
                    bool flip_v = true; // Possibly move to the option
                    if (flip_v)
                    {
                        float* coords = fdata;
                        float* coords_end = fdata + (mesh->m_VertexCount * num_components);
                        while (coords < coords_end)
                        {
                            coords[1] = 1.0f - coords[1];
                            coords += num_components;
                        }
                    }

                    if (attribute->index == 0)
                    {
                        mesh->m_TexCoords0.Set(fdata, data_count, data_count, false);
                        mesh->m_TexCoords0NumComponents = num_components;
                    }
                    else if (attribute->index == 1)
                    {
                        mesh->m_TexCoords1.Set(fdata, data_count, data_count, false);
                        mesh->m_TexCoords1NumComponents = num_components;
                    }
                }

                else if (attribute->type == cgltf_attribute_type_color)
                    mesh->m_Colors.Set(fdata, data_count, data_count, false);

                else if (attribute->type == cgltf_attribute_type_joints)
                    mesh->m_Bones.Set(udata, data_count, data_count, false);

                else if (attribute->type == cgltf_attribute_type_weights)
                    mesh->m_Weights.Set(fdata, data_count, data_count, false);
            }
        }

        if (mesh->m_Weights.Size() && mesh->m_Material)
        {
            mesh->m_Material->m_IsSkinned = 1;
        }

        if (mesh->m_TexCoords0.Empty())
        {
            mesh->m_TexCoords0NumComponents = 2;

            uint32_t size = mesh->m_VertexCount * mesh->m_TexCoords0NumComponents;
            InitSize(mesh->m_TexCoords0, size, size);
        }
    }
}

static void LoadMeshes(Scene* scene, cgltf_data* gltf_data)
{
    InitSize(scene->m_Models, gltf_data->meshes_count, gltf_data->meshes_count);

    for (uint32_t i = 0; i < gltf_data->meshes_count; ++i)
    {
        cgltf_mesh* gltf_mesh = &gltf_data->meshes[i]; // our "Model"
        Model* model = &scene->m_Models[i];
        model->m_Name = CreateObjectName(gltf_mesh, "model", i);
        model->m_Index = i;

        LoadPrimitives(scene, model, gltf_data, gltf_mesh); // Our "Meshes"
    }
}


// If the mesh wasn't skinned, but uses same material as other skinned meshes, we need to create a duplicate
// We also insert a bone index for the mesh (i.e. the parent)
static void FixupNonSkinnedModels(Scene* scene, Bone* parent, Node* node)
{
    Model* model = node->m_Model;

    model->m_ParentBone = parent;

    for (uint32_t j = 0; j < model->m_Meshes.Size(); ++j)
    {
        Mesh* mesh = &model->m_Meshes[j];
        if (!mesh->m_Weights.Empty())
            continue;

        if (!mesh->m_Material->m_IsSkinned)
            continue;

        // We duplicate the material, and create a non skinned version
        {
            char name[128];
            dmSnPrintf(name, sizeof(name), "%s_no_skin", mesh->m_Material->m_Name);
            Material* non_skinned = 0;
            for (uint32_t m = 0; m < scene->m_DynamicMaterials.Size(); ++m)
            {
                Material* material = scene->m_DynamicMaterials[m];
                if (strcmp(material->m_Name, name) == 0)
                {
                    non_skinned = material;
                    break;
                }
            }
            if (!non_skinned)
            {
                non_skinned = new Material;
                non_skinned->m_IsSkinned = 0;
                non_skinned->m_Index = scene->m_Materials.Size() + scene->m_DynamicMaterials.Size();
                non_skinned->m_Name = strdup(name);
                AddDynamicMaterial(scene, non_skinned);
            }
            mesh->m_Material = non_skinned;
        }
    }
}

static void FixupNonSkinnedModels(Scene* scene, Skin* skin)
{
    for (uint32_t i = 0; i < skin->m_Bones.Size(); ++i)
    {
        Bone* bone = &skin->m_Bones[i];
        if (!bone->m_Node)
            continue;

        for (uint32_t c = 0; c < bone->m_Node->m_Children.Size(); ++c)
        {
            Node* child = bone->m_Node->m_Children[c];
            if (!child->m_Model)
                continue;
            FixupNonSkinnedModels(scene, bone, child);
        }
    }
}

static void FixupNonSkinnedModels(Scene* scene, cgltf_data* gltf_data)
{
    for (uint32_t i = 0; i < scene->m_Skins.Size(); ++i)
    {
        FixupNonSkinnedModels(scene, &scene->m_Skins[i]);
    }
}

static uint32_t FindBoneIndex(cgltf_skin* gltf_skin, cgltf_node* joint)
{
    if (joint == 0)
        return INVALID_INDEX;

    for (uint32_t i = 0; i < gltf_skin->joints_count; ++i)
    {
        cgltf_node* gltf_joint = gltf_skin->joints[i];
        if (joint == gltf_joint)
            return i;
    }
    return INVALID_INDEX;
}

struct BoneSortInfo
{
    uint32_t m_Index;     // The index in the hierarchy (depth first!)
    uint32_t m_OldIndex;
};

struct BoneInfoSortPred
{
    bool operator()(const BoneSortInfo& a, const BoneSortInfo& b) const
    {
        return a.m_Index < b.m_Index;
    }
};

static void CopyBone(Bone* src, Bone* dst)
{
    // While it's not a POD type, we copy all data as-is
    // We aren't running a destructor on the stale data
    memcpy(dst, src, sizeof(Bone));
}

static void CalcIndicesDepthFirst(BoneSortInfo* infos, const Bone* bone, uint32_t* index)
{
    infos[bone->m_Index].m_OldIndex = bone->m_Index;
    infos[bone->m_Index].m_Index = (*index)++;

    for (uint32_t i = 0; i < bone->m_Children.Size(); ++i)
    {
        CalcIndicesDepthFirst(infos, bone->m_Children[i], index);
    }
}

static void SortSkinBones(Skin* skin)
{
    uint32_t bones_count = skin->m_Bones.Size();
    BoneSortInfo* infos = new BoneSortInfo[bones_count];

    uint32_t index_iter = 0;
    for (uint32_t i = 0; i < bones_count; ++i)
    {
        Bone* bone = &skin->m_Bones[i];
        if (bone->m_ParentIndex == INVALID_INDEX)
        {
            CalcIndicesDepthFirst(infos, bone, &index_iter);
        }
    }

    std::sort(infos, infos + bones_count, BoneInfoSortPred());

    // build the remap array
    InitSize(skin->m_BoneRemap, bones_count, bones_count);

    bool indices_differ = false;
    for (uint32_t i = 0; i < bones_count; ++i)
    {
        uint32_t index_old = infos[i].m_OldIndex;
        uint32_t index_new = infos[i].m_Index;
        skin->m_BoneRemap[index_old] = index_new;

        indices_differ |= index_old != index_new;
    }
    // If the indices don't differ, then we don't need to update the meshes bone indices either
    if (!indices_differ)
    {
        skin->m_BoneRemap.SetCapacity(0);
    }

    // do the remapping, i.e. flattern the hierarchy
    if (!skin->m_BoneRemap.Empty())
    {
        dmArray<uint32_t> bone_order;
        dmArray<Bone>     sorted_bones;
        InitSize(bone_order, bones_count, bones_count);
        InitSize(sorted_bones, bones_count, bones_count);

        for (uint32_t i = 0; i < bones_count; ++i)
        {
            bone_order[i] = i;
            Bone& bone = skin->m_Bones[i];
            bone.m_Index = skin->m_BoneRemap[bone.m_Index];
            bone.m_ParentIndex = bone.m_ParentIndex != INVALID_INDEX ? skin->m_BoneRemap[bone.m_ParentIndex] : INVALID_INDEX;
        }

        for (uint32_t i = 0; i < bones_count; ++i)
        {
            uint32_t new_index = skin->m_BoneRemap[i];
            CopyBone(&sorted_bones[new_index], &skin->m_Bones[i]);
        }

        skin->m_Bones.Swap(sorted_bones);
    }

    delete[] infos;
}

static void LoadSkins(Scene* scene, cgltf_data* gltf_data)
{
    if (gltf_data->skins_count == 0)
        return;

    InitSize(scene->m_Skins, gltf_data->skins_count, gltf_data->skins_count);

    for (uint32_t i = 0; i < gltf_data->skins_count; ++i)
    {
        cgltf_skin* gltf_skin = &gltf_data->skins[i];

        Skin* skin = &scene->m_Skins[i];
        skin->m_Name = CreateObjectName(gltf_skin, "skin", i);
        skin->m_Index = i;

        InitSize(skin->m_Bones, gltf_skin->joints_count+1, gltf_skin->joints_count);

        cgltf_accessor* accessor = gltf_skin->inverse_bind_matrices;
        for (uint32_t j = 0; j < gltf_skin->joints_count; ++j)
        {
            cgltf_node* gltf_joint = gltf_skin->joints[j];
            Bone* bone = &skin->m_Bones[j];
            bone->m_Name = CreateObjectName(gltf_joint, "bone", j);
            bone->m_Index = j;
            bone->m_ParentIndex = FindBoneIndex(gltf_skin, gltf_joint->parent);

            if (bone->m_ParentIndex != INVALID_INDEX)
            {
                Bone* parent = &skin->m_Bones[bone->m_ParentIndex];
                if (parent->m_Children.Full())
                    parent->m_Children.OffsetCapacity(4);
                parent->m_Children.Push(bone);
            }

            // Cannot translate the bones here, since they're not created yet
            // bone->m_Node = ...
            if (accessor)
            {
                float matrix[16];
                if (ReadAccessorMatrix4(accessor, j, matrix))
                {
                    FromMatrix4x4(matrix, bone->m_InvBindPose);
                } else
                {
                    assert(false);
                }
            }
            else
            {
                SetIdentity(bone->m_InvBindPose);
            }
        }

        SortSkinBones(skin);

        // LOAD SKELETON?
    }
}

static void GenerateRootBone(Scene* scene)
{
    Node* generated_node = 0;

    // For each skin, we must only have one root bone
    for (uint32_t i = 0; i < scene->m_Skins.Size(); ++i)
    {
        Skin* skin = &scene->m_Skins[i];

        uint32_t num_root_nodes = 0;

        for (uint32_t j = 0; j < skin->m_Bones.Size(); ++j)
        {
            dmModelImporter::Bone* bone = &skin->m_Bones[j];
            if (bone->m_ParentIndex == INVALID_INDEX)
                ++num_root_nodes;
        }

        if (num_root_nodes <= 1)
            continue;

        // We require 1 root bone

        // First, initialize the dummy node
        if (!generated_node)
        {
            // We've already pre allocated in LoadNodes()
            Node* node = &scene->m_Nodes[scene->m_Nodes.Size()-1];
            memset(node, 0, sizeof(Node));
            node->m_Index = scene->m_Nodes.Size();
            scene->m_Nodes.SetSize(scene->m_Nodes.Size()+1);
            node->m_Name = CreateNameFromHash("_generated_node", node->m_Index);
            SetIdentity(node->m_Local);
            SetIdentity(node->m_World);

            generated_node = node;
        }

        // Create the new array
        if (skin->m_Bones.Full())
            skin->m_Bones.OffsetCapacity(1);

        // Shift all elements one step to the right
        skin->m_Bones.SetSize(skin->m_Bones.Size()+1);
        memmove(skin->m_Bones.Begin()+1, skin->m_Bones.Begin(), sizeof(Bone)*(skin->m_Bones.Size()-1));

        Bone* bone = &skin->m_Bones[0];
        memset(bone, 0, sizeof(Bone));
        bone->m_Index = 0;
        bone->m_ParentIndex = INVALID_INDEX;
        bone->m_Name = strdup("_generated_root");
        bone->m_Node = generated_node;

        FromTransform(dmTransform::Transform(dmVMath::Vector3(0,0,0),
                                             dmVMath::Quat(0,0,0,1),
                                             dmVMath::Vector3(1,1,1)), bone->m_InvBindPose);

        // Update the indices
        for (uint32_t j = 1; j < skin->m_Bones.Size(); ++j)
        {
            Bone* bone = &skin->m_Bones[j];
            bone->m_Index++;
            if (bone->m_ParentIndex == INVALID_INDEX)
                bone->m_ParentIndex = 0;
            else
                bone->m_ParentIndex++;
        }

        // Update the vertex influences
        for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
        {
            Model* model = &scene->m_Models[i];
            for (uint32_t j = 0; j < model->m_Meshes.Size(); ++j)
            {
                Mesh* mesh = &model->m_Meshes[j];

                if (mesh->m_Bones.Empty())
                    continue; // The mesh doesn't have any bone indices to update

                for (uint32_t v = 0; v < mesh->m_VertexCount; ++v)
                {
                    mesh->m_Bones[v*4+0]++;
                    mesh->m_Bones[v*4+1]++;
                    mesh->m_Bones[v*4+2]++;
                    mesh->m_Bones[v*4+3]++;
                }
            }
        }
    }
}

static void LinkNodesWithBones(Scene* scene, cgltf_data* gltf_data)
{
    for (uint32_t i = 0; i < gltf_data->skins_count; ++i)
    {
        cgltf_skin* gltf_skin = &gltf_data->skins[i];
        Skin* skin = &scene->m_Skins[i];

        for (uint32_t j = 0; j < gltf_skin->joints_count; ++j)
        {
            cgltf_node* gltf_joint = gltf_skin->joints[j];

            Bone* bone = 0;
            for (uint32_t b = 0; b < gltf_skin->joints_count; ++b)
            {
                if (strcmp(skin->m_Bones[b].m_Name, gltf_joint->name) == 0)
                {
                    bone = &skin->m_Bones[b];
                    break;
                }
            }
            bone->m_Node = TranslateNode(gltf_joint, gltf_data, scene);
        }
    }
}

static void RemapMeshBoneIndices(Skin* skin, Mesh* mesh)
{
    uint32_t* remap_table = skin->m_BoneRemap.Begin();
    for (uint32_t i = 0; i < mesh->m_VertexCount; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            uint32_t old_index = mesh->m_Bones[i*4+j];
            mesh->m_Bones[i*4+j] = remap_table[old_index];
        }
    }
}

static void LinkMeshesWithNodes(Scene* scene, cgltf_data* gltf_data)
{
    for (uint32_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node* gltf_node = &gltf_data->nodes[i];
        Node* node = &scene->m_Nodes[i];

        if (gltf_node->mesh == 0)
            continue;

        uint32_t index = gltf_node->mesh - gltf_data->meshes;
        assert(index < gltf_data->meshes_count);

        node->m_Model = &scene->m_Models[index];

        // We need to compensate for any bone index remapping
        if (node->m_Skin && !node->m_Skin->m_BoneRemap.Empty())
        {
            for (uint32_t j = 0; j < node->m_Model->m_Meshes.Size(); ++j)
            {
                RemapMeshBoneIndices(node->m_Skin, &node->m_Model->m_Meshes[j]);
            }
        }
    }
}


static bool AreEqual(const float* a, const float* b, uint32_t num_components, float epsilon)
{
    for (uint32_t i = 0; i < num_components; ++i)
    {
        if (fabsf(a[i] - b[i]) > epsilon)
            return false;
    }
    return true;
}

static void LoadChannel(NodeAnimation* node_animation, cgltf_animation_channel* channel)
{
    cgltf_accessor* accessor_times = channel->sampler->input;
    cgltf_accessor* accessor = channel->sampler->output;

    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);

    bool all_identical = true;

    float time_min = FLT_MAX;
    float time_max = -FLT_MAX;

    KeyFrame* key_frames = new KeyFrame[accessor_times->count];
    memset(key_frames, 0, sizeof(KeyFrame)*accessor_times->count);
    for (uint32_t i = 0; i < accessor->count; ++i)
    {
        cgltf_accessor_read_float(accessor_times, i, &key_frames[i].m_Time, 1);
        cgltf_accessor_read_float(accessor, i, key_frames[i].m_Value, num_components);

        if (all_identical && !AreEqual(key_frames[0].m_Value, key_frames[i].m_Value, num_components, 0.0001f))
        {
            all_identical = false;
        }

        time_min = dmMath::Min(time_min, key_frames[i].m_Time);
        time_max = dmMath::Max(time_max, key_frames[i].m_Time);
    }
    node_animation->m_StartTime = 0.0f;
    node_animation->m_EndTime = time_max - time_min;

    uint32_t key_count = accessor->count;
    if (all_identical)
    {
        key_count = 1;
    }

    for (uint32_t i = 0; i < key_count; ++i)
    {
        key_frames[i].m_Time -= time_min;
    }

    if (channel->target_path == cgltf_animation_path_type_translation)
    {
        node_animation->m_TranslationKeys.Set(key_frames, key_count, key_count, false);
    }
    else if(channel->target_path == cgltf_animation_path_type_rotation)
    {
        node_animation->m_RotationKeys.Set(key_frames, key_count, key_count, false);
    }
    else if(channel->target_path == cgltf_animation_path_type_scale)
    {
        node_animation->m_ScaleKeys.Set(key_frames, key_count, key_count, false);
    } else
    {
        // Unsupported type
        delete[] key_frames;
    }
}

static uint32_t CountAnimatedNodes(cgltf_animation* animation, dmHashTable64<uint32_t>& node_to_index)
{
    node_to_index.SetCapacity((32*2)/3, 32);

    for (size_t i = 0; i < animation->channels_count; ++i)
    {
        if (node_to_index.Full())
        {
            uint32_t new_capacity = node_to_index.Capacity() + 32;
            node_to_index.SetCapacity((new_capacity*2)/3, new_capacity);
        }

        const char* node_name = animation->channels[i].target_node->name;
        dmhash_t node_name_hash = dmHashString64(node_name);

        uint32_t* prev_index = node_to_index.Get(node_name_hash);
        if (prev_index == 0)
        {
            node_to_index.Put(node_name_hash, node_to_index.Size());
        }
    }

    return node_to_index.Size();
}

static void LoadAnimations(Scene* scene, cgltf_data* gltf_data)
{
    if (gltf_data->animations_count == 0)
        return;

    InitSize(scene->m_Animations, gltf_data->animations_count, gltf_data->animations_count);

    // first, count number of animated nodes we have
    for (uint32_t a = 0; a < gltf_data->animations_count; ++a)
    {
        cgltf_animation* gltf_animation = &gltf_data->animations[a];
        Animation* animation = &scene->m_Animations[a];

        animation->m_Name = CreateObjectName(gltf_animation, "animation", a);

        // Here we want to create a many individual tracks for different bones (name.type): "a.rot", "b.rot", "a.pos", "b.scale"...
        // into a list of tracks that holds all 3 types: [a: {rot, pos, scale}, b: {rot, pos, scale}...]
        dmHashTable64<uint32_t> node_name_to_index;
        uint32_t node_animations_count = CountAnimatedNodes(gltf_animation, node_name_to_index);

        InitSize(animation->m_NodeAnimations, node_animations_count, node_animations_count);

        for (size_t i = 0; i < gltf_animation->channels_count; ++i)
        {
            cgltf_animation_channel* channel = &gltf_animation->channels[i];

            Node* node = TranslateNode(channel->target_node, gltf_data, scene);
            dmhash_t node_name_hash = node->m_NameHash;

            uint32_t* node_index = node_name_to_index.Get(node_name_hash);
            assert(node_index != 0);

            NodeAnimation* node_animation = &animation->m_NodeAnimations[*node_index];
            node_animation->m_Node = node;

            LoadChannel(node_animation, channel);

            animation->m_Duration = node_animation->m_EndTime - node_animation->m_StartTime;
        }
    }
}

// Based on cgltf.h: cgltf_load_buffers(...)
static cgltf_result ResolveBuffers(const cgltf_options* options, cgltf_data* data, const char* gltf_path)
{
    if (options == NULL)
    {
        return cgltf_result_invalid_options;
    }

    if (data->buffers_count && data->buffers[0].data == NULL && data->buffers[0].uri == NULL && data->bin)
    {
        if (data->bin_size < data->buffers[0].size)
        {
            return cgltf_result_data_too_short;
        }

        data->buffers[0].data = (void*)data->bin;
        data->buffers[0].data_free_method = cgltf_data_free_method_none;
    }

    for (cgltf_size i = 0; i < data->buffers_count; ++i)
    {
        if (data->buffers[i].data)
        {
            continue;
        }

        const char* uri = data->buffers[i].uri;

        if (uri == NULL)
        {
            continue;
        }

        if (strncmp(uri, "data:", 5) == 0)
        {
            const char* comma = strchr(uri, ',');

            if (comma && comma - uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
            {
                cgltf_result res = cgltf_load_buffer_base64(options, data->buffers[i].size, comma + 1, &data->buffers[i].data);
                data->buffers[i].data_free_method = cgltf_data_free_method_memory_free;

                if (res != cgltf_result_success)
                {
                    return res;
                }
            }
            else
            {
                return cgltf_result_unknown_format;
            }
        }
        else if (strstr(uri, "://") == NULL && gltf_path)
        {
            cgltf_result res = cgltf_load_buffer_file(options, data->buffers[i].size, uri, gltf_path, &data->buffers[i].data);
            data->buffers[i].data_free_method = cgltf_data_free_method_file_release;

            if (res != cgltf_result_success)
            {
                return res;
            }
        }
        // DEFOLD
        else if (!gltf_path)
        {
            continue; // we allow the code to supply the buffers later
        }
        // END DEFOLD
        else
        {
            return cgltf_result_unknown_format;
        }
    }

    return cgltf_result_success;
}

static bool HasUnresolvedBuffersInternal(cgltf_data* data)
{
    for (cgltf_size i = 0; i < data->buffers_count; ++i)
    {
        if (!data->buffers[i].data)
            return true;
    }
    return false;
}

bool HasUnresolvedBuffers(Scene* scene)
{
    return HasUnresolvedBuffersInternal((cgltf_data*)scene->m_OpaqueSceneData);
}

static void LoadScene(Scene* scene, cgltf_data* data)
{
    LoadSkins(scene, data);
    LoadNodes(scene, data);
    LoadSamplers(scene, data);
    LoadMaterials(scene, data);
    LoadMeshes(scene, data);
    LinkNodesWithBones(scene, data);
    LinkMeshesWithNodes(scene, data);
    LoadAnimations(scene, data);

    FixupNonSkinnedModels(scene, data);

    // At this point, all the nodes reference each other

    // Make sure the skins have only one root node
    GenerateRootBone(scene);
}

static bool LoadFinalizeGltf(Scene* scene)
{
    GltfData* data = (GltfData*)scene->m_OpaqueSceneData;
    LoadScene(scene, data->m_Data);
    return true;
}

static bool ValidateGltf(Scene* scene)
{
    GltfData* data = (GltfData*)scene->m_OpaqueSceneData;
    cgltf_result result = cgltf_validate(data->m_Data);
    if (result != cgltf_result_success)
    {
        printf("Failed to validate gltf file: %s (%d)\n", GetResultStr(result), result);
    }
    return result == cgltf_result_success;
}

static void DestroyGltf(Scene* scene)
{
    GltfData* data = (GltfData*)scene->m_OpaqueSceneData;
    cgltf_free(data->m_Data);
    delete data;
}

Scene* LoadGltfFromBuffer(Options* importeroptions, void* mem, uint32_t file_size)
{
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));

    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, (uint8_t*)mem, file_size, &data);

    if (result != cgltf_result_success)
    {
        printf("Failed to load gltf file: %s (%d)\n", GetResultStr(result), result);
        return 0;
    }

    // resolve as many buffers as possible
    result = ResolveBuffers(&options, data, 0);
    if (result != cgltf_result_success)
    {
        printf("Failed to load gltf buffers: %s (%d)\n", GetResultStr(result), result);
        return 0;
    }

    Scene* scene = new Scene;
    memset(scene, 0, sizeof(Scene));
    GltfData* scenedata = new GltfData;
    scenedata->m_Data = data;

    scene->m_OpaqueSceneData = scenedata;
    scene->m_LoadFinalizeFn = LoadFinalizeGltf;
    scene->m_ValidateFn = ValidateGltf;
    scene->m_DestroyFn = DestroyGltf;

    InitSize(scene->m_Buffers, data->buffers_count, data->buffers_count);

    for (cgltf_size i = 0; i < data->buffers_count; ++i)
    {
        scene->m_Buffers[i].m_Uri = CreateObjectName(data->buffers, "buffer", i);
        scene->m_Buffers[i].m_Buffer = (uint8_t*)data->buffers[i].data;
        scene->m_Buffers[i].m_BufferCount = data->buffers[i].size;
    }

    if (!NeedsResolve(scene))
    {
        LoadFinalizeGltf(scene);
        ValidateGltf(scene);
    }

    return scene;
}

void ResolveBuffer(Scene* scene, const char* uri, void* bufferdata, uint32_t bufferdata_size)
{
    GltfData* scenedata = (GltfData*)scene->m_OpaqueSceneData;
    cgltf_data* data = scenedata->m_Data;

    cgltf_options _options;
    cgltf_options* options = &_options;
    memset(options, 0, sizeof(cgltf_options));

    void* (*memory_alloc)(void*, cgltf_size) = options->memory.alloc_func ? options->memory.alloc_func : &cgltf_default_alloc;

    for (cgltf_size i = 0; i < scene->m_Buffers.Size(); ++i)
    {
        Buffer* scenebuffer = &scene->m_Buffers[i];
        if (strcmp(scenebuffer->m_Uri, uri) == 0)
        {
            cgltf_buffer* buffer = &data->buffers[i];

            buffer->data = memory_alloc(options->memory.user_data, buffer->size);
            buffer->data_free_method = cgltf_data_free_method_memory_free;

            memcpy(buffer->data, bufferdata, bufferdata_size);
            scenebuffer->m_Buffer = (uint8_t*)buffer->data;
            return;
        }
    }
}

}
