
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

// SPEC:
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html

#include "modelimporter.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf/cgltf.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/transform.h>

namespace dmModelImporter
{

static void OutputTransform(const dmTransform::Transform& transform)
{
    printf("    t: %f, %f, %f\n", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
    printf("    r: %f, %f, %f, %f\n", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
    printf("    s: %f, %f, %f\n", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
}

static void printVector4(const dmVMath::Vector4& v)
{
    printf("%f, %f, %f, %f\n", v.getX(), v.getY(), v.getZ(), v.getW());
}

static void OutputMatrix(const dmTransform::Transform& transform)
{
    dmVMath::Matrix4 m = dmTransform::ToMatrix4(transform);
    printf("        "); printVector4(m.getRow(0));
    printf("        "); printVector4(m.getRow(1));
    printf("        "); printVector4(m.getRow(2));
    printf("        "); printVector4(m.getRow(3));
}

static const char* getPrimitiveTypeStr(cgltf_primitive_type type)
{
    switch(type)
    {
    case cgltf_primitive_type_points: return "cgltf_primitive_type_points";
    case cgltf_primitive_type_lines: return "cgltf_primitive_type_lines";
    case cgltf_primitive_type_line_loop: return "cgltf_primitive_type_line_loop";
    case cgltf_primitive_type_line_strip: return "cgltf_primitive_type_line_strip";
    case cgltf_primitive_type_triangles: return "cgltf_primitive_type_triangles";
    case cgltf_primitive_type_triangle_strip: return "cgltf_primitive_type_triangle_strip";
    case cgltf_primitive_type_triangle_fan: return "cgltf_primitive_type_triangle_fan";
    default: return "unknown";
    }
}

static const char* GetAttributeTypeStr(cgltf_attribute_type type)
{
    switch(type)
    {
    case cgltf_attribute_type_invalid: return "cgltf_attribute_type_invalid";
    case cgltf_attribute_type_position: return "cgltf_attribute_type_position";
    case cgltf_attribute_type_normal: return "cgltf_attribute_type_normal";
    case cgltf_attribute_type_tangent: return "cgltf_attribute_type_tangent";
    case cgltf_attribute_type_texcoord: return "cgltf_attribute_type_texcoord";
    case cgltf_attribute_type_color: return "cgltf_attribute_type_color";
    case cgltf_attribute_type_joints: return "cgltf_attribute_type_joints";
    case cgltf_attribute_type_weights: return "cgltf_attribute_type_weights";
    default: return "unknown";
    }
}

static const char* GetTypeStr(cgltf_type type)
{
    switch(type)
    {
    case cgltf_type_invalid: return "cgltf_type_invalid";
    case cgltf_type_scalar: return "cgltf_type_scalar";
    case cgltf_type_vec2: return "cgltf_type_vec2";
    case cgltf_type_vec3: return "cgltf_type_vec3";
    case cgltf_type_vec4: return "cgltf_type_vec4";
    case cgltf_type_mat2: return "cgltf_type_mat2";
    case cgltf_type_mat3: return "cgltf_type_mat3";
    case cgltf_type_mat4: return "cgltf_type_mat4";
    default: return "unknown";
    }
}

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

static const char* GetAnimationPathTypeStr(cgltf_animation_path_type type)
{
    switch(type)
    {
    case cgltf_animation_path_type_invalid: return "cgltf_animation_path_type_invalid";
    case cgltf_animation_path_type_translation: return "cgltf_animation_path_type_translation";
    case cgltf_animation_path_type_rotation: return "cgltf_animation_path_type_rotation";
    case cgltf_animation_path_type_scale: return "cgltf_animation_path_type_scale";
    case cgltf_animation_path_type_weights: return "cgltf_animation_path_type_weights";
    default: return "unknown";
    }
}

static const char* GetInterpolationTypeStr(cgltf_interpolation_type type)
{
 switch(type)
 {
 case cgltf_interpolation_type_linear: return "cgltf_interpolation_type_linear";
 case cgltf_interpolation_type_step: return "cgltf_interpolation_type_step";
 case cgltf_interpolation_type_cubic_spline: return "cgltf_interpolation_type_cubic_spline";
 default: return "unknown";
 }
}


static float* ReadAccessorFloat(cgltf_accessor* accessor, uint32_t desired_num_components)
{
    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);

    if (desired_num_components == 0)
        desired_num_components = num_components;

    float* out = new float[accessor->count * num_components];
    float* writeptr = out;

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

static uint32_t* ReadAccessorUint32(cgltf_accessor* accessor, uint32_t desired_num_components)
{
    uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);

    if (desired_num_components == 0)
        desired_num_components = num_components;

    uint32_t* out = new uint32_t[accessor->count * desired_num_components];
    uint32_t* writeptr = out;

    for (uint32_t i = 0; i < accessor->count; ++i)
    {
        bool result = cgltf_accessor_read_uint(accessor, i, writeptr, num_components);

        if (!result)
        {
            printf("couldnt read uints!\n");
            delete[] out;
            return 0;;
        }

        writeptr += desired_num_components;
    }

    return out;
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



static void DestroyGltf(void* opaque_scene_data);


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

static dmTransform::Transform ToTransform(const float* m)
{
    dmVMath::Matrix4 mat = dmVMath::Matrix4(dmVMath::Vector4(m[0], m[1], m[2], m[3]),
                                            dmVMath::Vector4(m[4], m[5], m[6], m[7]),
                                            dmVMath::Vector4(m[8], m[9], m[10], m[11]),
                                            dmVMath::Vector4(m[12], m[13], m[14], m[15]));
    return dmTransform::ToTransform(mat);
}

static Skin* FindSkin(Scene* scene, const char* name)
{
    for (uint32_t i = 0; i < scene->m_SkinsCount; ++i)
    {
        Skin* skin = &scene->m_Skins[i];
        if (strcmp(name, skin->m_Name) == 0)
            return skin;
    }
    return 0;
}

static void UpdateWorldTransforms(Node* node)
{
    if (node->m_Parent)
        node->m_World = dmTransform::Mul(node->m_Parent->m_World, node->m_Local);
    else
        node->m_World = node->m_Local;

    for (uint32_t c = 0; c < node->m_ChildrenCount; ++c)
    {
        UpdateWorldTransforms(node->m_Children[c]);
    }
}

static void LoadNodes(Scene* scene, cgltf_data* gltf_data)
{
    scene->m_NodesCount = gltf_data->nodes_count;
    // We allocate one extra node, in case we need it for later
    // In the case we generate a root bone, we want a valid node for it.
    scene->m_Nodes = new Node[scene->m_NodesCount+1];
    memset(scene->m_Nodes, 0, sizeof(Node)*scene->m_NodesCount+1);

    for (size_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node* gltf_node = &gltf_data->nodes[i];

        Node* node = &scene->m_Nodes[i];
        node->m_Name = strdup(gltf_node->name ? gltf_node->name : "unknown");
        node->m_Index = i;

        // We link them together later
        // node->m_Model = ...

        dmVMath::Vector3 scale = dmVMath::Vector3(1,1,1);
        dmVMath::Vector3 translation = dmVMath::Vector3(0,0,0);
        dmVMath::Quat rotation = dmVMath::Quat(0,0,0,1);

        node->m_Skin = 0;
        if (gltf_node->skin)
        {
            node->m_Skin = FindSkin(scene, gltf_node->skin->name);
        }

        if (gltf_node->has_translation)
            translation = dmVMath::Vector3(gltf_node->translation[0], gltf_node->translation[1], gltf_node->translation[2]);
        if (gltf_node->has_scale)
            scale = dmVMath::Vector3(gltf_node->scale[0], gltf_node->scale[1], gltf_node->scale[2]);
        if (gltf_node->has_rotation)
            rotation = dmVMath::Quat(gltf_node->rotation[0], gltf_node->rotation[1], gltf_node->rotation[2], gltf_node->rotation[3]);

        if (gltf_node->has_matrix)
        {
            node->m_Local = ToTransform(gltf_node->matrix);
        }
        else {
            node->m_Local = dmTransform::Transform(translation, rotation, scale);
        }
    }

    // find all the parents and all the children

    for (size_t i = 0; i < gltf_data->nodes_count; ++i)
    {
        cgltf_node* gltf_node = &gltf_data->nodes[i];
        Node* node = &scene->m_Nodes[i];

        node->m_Parent = gltf_node->parent ? TranslateNode(gltf_node->parent, gltf_data, scene) : 0;

        node->m_ChildrenCount = gltf_node->children_count;
        node->m_Children = new Node*[node->m_ChildrenCount];

        for (uint32_t c = 0; c < gltf_node->children_count; ++c)
            node->m_Children[c] = TranslateNode(gltf_node->children[c], gltf_data, scene);
    }

    // Find root nodes
    scene->m_RootNodesCount = 0;
    for (uint32_t i = 0; i < scene->m_NodesCount; ++i)
    {
        Node* node = &scene->m_Nodes[i];
        if (node->m_Parent == 0)
            scene->m_RootNodesCount++;
    }
    scene->m_RootNodes = new Node*[scene->m_RootNodesCount];

    scene->m_RootNodesCount = 0;
    for (uint32_t i = 0; i < scene->m_NodesCount; ++i)
    {
        Node* node = &scene->m_Nodes[i];
        if (node->m_Parent == 0)
        {
            scene->m_RootNodes[scene->m_RootNodesCount++] = node;

            UpdateWorldTransforms(node);
        }
    }
}

static void LoadPrimitives(Model* model, cgltf_mesh* gltf_mesh)
{
    model->m_MeshesCount = gltf_mesh->primitives_count;
    model->m_Meshes = new Mesh[model->m_MeshesCount];
    memset(model->m_Meshes, 0, sizeof(Mesh)*model->m_MeshesCount);

    for (size_t i = 0; i < gltf_mesh->primitives_count; ++i)
    {
        cgltf_primitive* prim = &gltf_mesh->primitives[i];
        Mesh* mesh = &model->m_Meshes[i];
        mesh->m_Name = strdup(gltf_mesh->name ? gltf_mesh->name : "unknown");

        mesh->m_Material = strdup(prim->material ? prim->material->name : "no_material");
        mesh->m_VertexCount = 0;

        //printf("primitive_type: %s\n", getPrimitiveTypeStr(prim->type));

        mesh->m_IndexCount = prim->indices->count;
        mesh->m_Indices = ReadAccessorUint32(prim->indices, 1);

        for (uint32_t a = 0; a < prim->attributes_count; ++a)
        {
            cgltf_attribute* attribute = &prim->attributes[a];
            cgltf_accessor* accessor = attribute->data;
            //printf("  attributes: %s   index: %u   type: %s  count: %u\n", attribute->name, attribute->index, GetAttributeTypeStr(attribute->type), (uint32_t)accessor->count);

            mesh->m_VertexCount = accessor->count;

            uint32_t num_components = (uint32_t)cgltf_num_components(accessor->type);
            uint32_t desired_num_components = num_components;

            if (attribute->type == cgltf_attribute_type_tangent)
            {
                desired_num_components = 3; // for some reason it give 4 elements
            }

            float* fdata = 0;
            uint32_t* udata = 0;

            if (attribute->type == cgltf_attribute_type_joints)
            {
                udata = ReadAccessorUint32(accessor, desired_num_components);
            }
            else
            {
                fdata = ReadAccessorFloat(accessor, desired_num_components);
            }

            if (fdata || udata)
            {
                if (attribute->type == cgltf_attribute_type_position)
                    mesh->m_Positions = fdata;

                else if (attribute->type == cgltf_attribute_type_normal)
                    mesh->m_Normals = fdata;

                else if (attribute->type == cgltf_attribute_type_tangent)
                    mesh->m_Tangents = fdata;

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
                        mesh->m_TexCoord0 = fdata;
                        mesh->m_TexCoord0NumComponents = num_components;
                    }
                    else if (attribute->index == 1)
                    {
                        mesh->m_TexCoord1 = fdata;
                        mesh->m_TexCoord1NumComponents = num_components;
                    }
                }

                else if (attribute->type == cgltf_attribute_type_color)
                    mesh->m_Color = fdata;

                else if (attribute->type == cgltf_attribute_type_joints)
                    mesh->m_Bones = udata;

                else if (attribute->type == cgltf_attribute_type_weights)
                    mesh->m_Weights = fdata;
            }
        }

        if (!mesh->m_TexCoord0)
        {
            mesh->m_TexCoord0NumComponents = 2;
            mesh->m_TexCoord0 = new float[mesh->m_VertexCount * mesh->m_TexCoord0NumComponents];
            memset(mesh->m_TexCoord0, 0, mesh->m_VertexCount * mesh->m_TexCoord0NumComponents);
        }
    }
}

static void LoadMeshes(Scene* scene, cgltf_data* gltf_data)
{
    scene->m_ModelsCount = gltf_data->meshes_count;
    scene->m_Models = new Model[scene->m_ModelsCount];
    memset(scene->m_Models, 0, sizeof(Model)*scene->m_ModelsCount);

    for (uint32_t i = 0; i < gltf_data->meshes_count; ++i)
    {
        cgltf_mesh* gltf_mesh = &gltf_data->meshes[i]; // our "Model"
        Model* model = &scene->m_Models[i];
        model->m_Name = strdup(gltf_mesh->name ? gltf_mesh->name : "unknown");
        model->m_Index = i;

        LoadPrimitives(model, gltf_mesh); // Our "Meshes"
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

static void LoadSkins(Scene* scene, cgltf_data* gltf_data)
{
    if (gltf_data->skins_count == 0)
        return;

    scene->m_SkinsCount = gltf_data->skins_count;
    scene->m_Skins = new Skin[scene->m_SkinsCount];
    memset(scene->m_Skins, 0, sizeof(Skin)*scene->m_SkinsCount);

    for (uint32_t i = 0; i < gltf_data->skins_count; ++i)
    {
        cgltf_skin* gltf_skin = &gltf_data->skins[i];

        Skin* skin = &scene->m_Skins[i];
        skin->m_Name = strdup(gltf_skin->name ? gltf_skin->name : "unknown");
        skin->m_Index = i;

        skin->m_BonesCount = gltf_skin->joints_count;
        skin->m_Bones = new Bone[skin->m_BonesCount];
        memset(skin->m_Bones, 0, sizeof(Bone)*skin->m_BonesCount);

        cgltf_accessor* accessor = gltf_skin->inverse_bind_matrices;
        for (uint32_t j = 0; j < gltf_skin->joints_count; ++j)
        {
            cgltf_node* gltf_joint = gltf_skin->joints[j];
            Bone* bone = &skin->m_Bones[j];
            bone->m_Name = strdup(gltf_joint->name ? gltf_joint->name : "unknown");
            bone->m_Index = j;
            bone->m_ParentIndex = FindBoneIndex(gltf_skin, gltf_joint->parent);
            // Cannot translate the bones here, since they're not created yet
            // bone->m_Node = ...

            float matrix[16];
            if (ReadAccessorMatrix4(accessor, j, matrix))
            {
                bone->m_InvBindPose = ToTransform(matrix);
            } else
            {
                assert(false);
            }
        }

        // LOAD SKELETON?
    }
}

static void GenerateRootBone(Scene* scene)
{
    Node* generated_node = 0;

    // For each skin, we must only have one root bone
    for (uint32_t i = 0; i < scene->m_SkinsCount; ++i)
    {
        Skin* skin = &scene->m_Skins[i];

        uint32_t num_root_nodes = 0;

        for (uint32_t j = 0; j < skin->m_BonesCount; ++j)
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
            Node* node = &scene->m_Nodes[scene->m_NodesCount];
            memset(node, 0, sizeof(Node));
            node->m_Index = scene->m_NodesCount++;

            node->m_Name = strdup("_generated_node");
            node->m_Local.SetIdentity();
            node->m_World.SetIdentity();

            generated_node = node;
        }

        // Create the new array
        Bone* new_bones = new Bone[skin->m_BonesCount+1];
        memcpy(new_bones+1, skin->m_Bones, sizeof(Bone)*skin->m_BonesCount);

        Bone* bone = &new_bones[0];
        memset(bone, 0, sizeof(Bone));
        bone->m_Index = 0;
        bone->m_ParentIndex = INVALID_INDEX;
        bone->m_Name = strdup("_generated_root");
        bone->m_Node = generated_node;
        bone->m_InvBindPose = dmTransform::Transform(dmVMath::Vector3(0,0,0),
                                                     dmVMath::Quat(0,0,0,1),
                                                     dmVMath::Vector3(1,1,1));

        Bone* prev_bones = skin->m_Bones;
        skin->m_Bones = new_bones;
        skin->m_BonesCount++;
        delete[] prev_bones;

        // Update the indices
        for (uint32_t j = 1; j < skin->m_BonesCount; ++j)
        {
            Bone* bone = &skin->m_Bones[j];
            bone->m_Index++;
            if (bone->m_ParentIndex == INVALID_INDEX)
                bone->m_ParentIndex = 0;
            else
                bone->m_ParentIndex++;
        }

        // Update the vertex influences
        for (uint32_t i = 0; i < scene->m_ModelsCount; ++i)
        {
            Model* model = &scene->m_Models[i];
            for (uint32_t j = 0; j < model->m_MeshesCount; ++j)
            {
                Mesh* mesh = &model->m_Meshes[j];

                if (!mesh->m_Bones)
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
            Bone* bone = &skin->m_Bones[j];
            bone->m_Node = TranslateNode(gltf_joint, gltf_data, scene);
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
        node_animation->m_TranslationKeys = key_frames;
        node_animation->m_TranslationKeysCount = key_count;
    }
    else if(channel->target_path == cgltf_animation_path_type_rotation)
    {
        node_animation->m_RotationKeys = key_frames;
        node_animation->m_RotationKeysCount = key_count;
    }
    else if(channel->target_path == cgltf_animation_path_type_scale)
    {
        node_animation->m_ScaleKeys = key_frames;
        node_animation->m_ScaleKeysCount = key_count;
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

    scene->m_AnimationsCount = gltf_data->animations_count;
    scene->m_Animations = new Animation[scene->m_AnimationsCount];

    // first, count number of animated nodes we have
    for (uint32_t i = 0; i < gltf_data->animations_count; ++i)
    {
        cgltf_animation* gltf_animation = &gltf_data->animations[i];
        Animation* animation = &scene->m_Animations[i];

        animation->m_Name = strdup(gltf_animation->name ? gltf_animation->name : "default");

        dmHashTable64<uint32_t> node_name_to_index;
        animation->m_NodeAnimationsCount = CountAnimatedNodes(gltf_animation, node_name_to_index);
        animation->m_NodeAnimations = new NodeAnimation[animation->m_NodeAnimationsCount];
        memset(animation->m_NodeAnimations, 0, sizeof(NodeAnimation) * animation->m_NodeAnimationsCount);

        for (size_t i = 0; i < gltf_animation->channels_count; ++i)
        {
            cgltf_animation_channel* channel = &gltf_animation->channels[i];

            const char* node_name = channel->target_node->name;
            dmhash_t node_name_hash = dmHashString64(node_name);

            uint32_t* node_index = node_name_to_index.Get(node_name_hash);
            assert(node_index != 0);

            NodeAnimation* node_animation = &animation->m_NodeAnimations[*node_index];
            if (node_animation->m_Node == 0)
                node_animation->m_Node = TranslateNode(channel->target_node, gltf_data, scene);

            LoadChannel(node_animation, channel);

            animation->m_Duration = node_animation->m_EndTime - node_animation->m_StartTime;
        }
    }
}


Scene* LoadGltfFromBuffer(Options* importeroptions, void* mem, uint32_t file_size)
{
    cgltf_options options;
    memset(&options, 0, sizeof(cgltf_options));

    cgltf_data* data = NULL;
    cgltf_result result = cgltf_parse(&options, (uint8_t*)mem, file_size, &data);

    if (result == cgltf_result_success)
        result = cgltf_load_buffers(&options, data, 0);

    if (result == cgltf_result_success)
        result = cgltf_validate(data);

    if (result != cgltf_result_success)
    {
        printf("Failed to load gltf file: %s (%d)\n", GetResultStr(result), result);
        return 0;
    }

    Scene* scene = new Scene;
    memset(scene, 0, sizeof(Scene));
    scene->m_OpaqueSceneData = (void*)data;
    scene->m_DestroyFn = DestroyGltf;

    LoadSkins(scene, data);
    LoadNodes(scene, data);
    LoadMeshes(scene, data);
    LinkNodesWithBones(scene, data);
    LinkMeshesWithNodes(scene, data);
    LoadAnimations(scene, data);

    // At this point, all the nodes reference each other

    // Make sure the skins have only one root node
    GenerateRootBone(scene);

    return scene;
}

static void DestroyGltf(void* opaque_scene_data)
{
    cgltf_free((cgltf_data*)opaque_scene_data);
}

}
