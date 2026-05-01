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

#include "modelimporter.h"
#include <dmsdk/dlib/array.h>
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

// static void OutputTransform(const dmTransform::Transform& transform)
// {
//     printf("t: %f, %f, %f  ", transform.GetTranslation().getX(), transform.GetTranslation().getY(), transform.GetTranslation().getZ());
//     printf("r: %f, %f, %f, %f  ", transform.GetRotation().getX(), transform.GetRotation().getY(), transform.GetRotation().getZ(), transform.GetRotation().getW());
//     printf("s: %f, %f, %f  ", transform.GetScale().getX(), transform.GetScale().getY(), transform.GetScale().getZ());
// }

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

// TODO: move this to a private header
static dmTransform::Transform& ToTransform(const dmModelImporter::Transform& in, dmTransform::Transform& out)
{
    out = dmTransform::Transform(dmVMath::Vector3(in.m_Translation.x, in.m_Translation.y, in.m_Translation.z),
                                 dmVMath::Quat(in.m_Rotation.x, in.m_Rotation.y, in.m_Rotation.z, in.m_Rotation.w),
                                 dmVMath::Vector3(in.m_Scale.x, in.m_Scale.y, in.m_Scale.z));
    return out;
}

static void OutputMatrix(const dmModelImporter::Transform& transform)
{
    dmTransform::Transform out;
    OutputMatrix(ToTransform(transform, out));
}

static void OutputBone(int i, Bone* bone, int indent)
{
    OutputIndent(indent);
    printf("#%d: %s  idx: %u parent: %u node: %s inv_bind_pose:\n", i, bone->m_Name, bone->m_Index, bone->m_ParentIndex, bone->m_Node?bone->m_Node->m_Name:"null");
    OutputMatrix(bone->m_InvBindPose);
    printf("\n");
}

static void OutputSkin(Skin* skin, int indent)
{
    OutputIndent(indent);
    printf("Skin name: %s\n", skin->m_Name);

    printf("  Bones: count: %u\n", skin->m_Bones.Size());
    for (uint32_t i = 0; i < skin->m_Bones.Size(); ++i)
    {
        OutputBone(i, &skin->m_Bones[i], indent+1);
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


    for (uint32_t i = 0; i < node->m_Children.Size(); ++i)
    {
        OutputNodeTree(node->m_Children[i], indent+1);
    }
}

template <int N>
static void OutputArray(const char* name, float (&p)[N], int indent)
{
    OutputIndent(indent);
    printf("%s: ", name);
    for (int i = 0; i < N; ++i)
    {
        printf("%.3f", p[i]);
        if (i < (N-1))
            printf(", ");
    }
    printf("\n");
}

template<typename T>
static void OutputValue(const char* name, T value, int indent)
{
    OutputIndent(indent);
    printf("%s: %.3f\n", name, (float)value);
}

template<>
void OutputValue(const char* name, int value, int indent)
{
    OutputIndent(indent);
    printf("%s: %d\n", name, value);
}

template<>
void OutputValue(const char* name, bool value, int indent)
{
    OutputIndent(indent);
    printf("%s: %s\n", name, value?"true":"false");
}

template<>
void OutputValue(const char* name, const char* value, int indent)
{
    OutputIndent(indent);
    printf("%s: %s\n", name, value);
}

template<>
void OutputValue(const char* name, AlphaMode value, int indent)
{
    const char* modes[] = {
        "OPAQUE",
        "MASK",
        "BLEND",
        "MAX_ENUM"
    };
    OutputIndent(indent);
    printf("%s: %s (%d)\n", name, modes[value], value);
}

static void OutputSampler(Sampler* sampler, int indent)
{
    OutputIndent(indent);
    printf("Sampler %d: %s \n", sampler->m_Index, sampler->m_Name);

    OutputValue("mag_filter", sampler->m_MagFilter, indent+1);
    OutputValue("min_filter", sampler->m_MinFilter, indent+1);
    OutputValue("wrap_s", sampler->m_WrapS,     indent+1);
    OutputValue("wrap_t", sampler->m_WrapT,     indent+1);
}

static void OutputTextureTransform(TextureTransform* p, int indent)
{
    OutputArray("offset", p->m_Offset,      indent);
    OutputValue("rotation", p->m_Rotation,  indent);
    OutputArray("scale", p->m_Scale,        indent);
    if (p->m_Texcoord >= 0)
        OutputValue("texcoord", p->m_Texcoord, indent);
}

static void OutputTextureView(const char* name, TextureView* p, int indent)
{
    if (!p)
        return;
    OutputIndent(indent);
    printf("%s:\n", name);
    if (p->m_Texture)
    {
        OutputIndent(indent+1);
        printf("texture: %s\n", p->m_Texture->m_Name);
    }

    OutputValue("tex_coord", p->m_Texcoord, indent);
    OutputValue("scale", p->m_Scale, indent);

    if (p->m_HasTransform)
    {
        OutputIndent(indent+1);
        printf("transform:\n");
        OutputTextureTransform(&p->m_Transform, indent+1);
    }
}

static void OutputTexture(Texture* texture, int indent)
{
    OutputIndent(indent);
    printf("Texture %d: %s \n", texture->m_Index, texture->m_Name);

    if (texture->m_Image)
        OutputValue("image", texture->m_Image->m_Name, indent+1);
    if (texture->m_Sampler)
        OutputValue("sampler", texture->m_Sampler->m_Name, indent+1);
    if (texture->m_BasisuImage)
        OutputValue("basisu_image", texture->m_BasisuImage->m_Name, indent+1);
}


static void OutputPbrMetallicRoughness(PbrMetallicRoughness* p, int indent)
{
    OutputIndent(indent);
    printf("pbr_metallic_roughness\n");
    OutputArray("base_color_factor", p->m_BaseColorFactor, indent+1);
    OutputValue("roughness_factor", p->m_RoughnessFactor, indent+1);

    OutputTextureView("base_color_texture", &p->m_BaseColorTexture, indent+1);
    OutputTextureView("metallic_roughness_texture", &p->m_MetallicRoughnessTexture, indent+1);
}

static void OutputPbrSpecularGlossiness(PbrSpecularGlossiness* p, int indent)
{
    OutputIndent(indent);
    printf("pbr_specular_glossiness\n");

    OutputArray("diffuse_factor", p->m_DiffuseFactor, indent+1);
    OutputArray("specular_factor", p->m_SpecularFactor, indent+1);

    OutputValue("specular_glossiness", p->m_GlossinessFactor, indent+1);

    OutputTextureView("diffuse_texture", &p->m_DiffuseTexture, indent+1);
    OutputTextureView("specular_glossiness_texture", &p->m_SpecularGlossinessTexture, indent+1);
}

static void OutputClearcoat(Clearcoat* p, int indent)
{
    OutputIndent(indent);
    printf("clearcoat\n");

    OutputValue("clearcoat_factor", p->m_ClearcoatFactor, indent+1);
    OutputValue("clearcoat_roughness_factor", p->m_ClearcoatRoughnessFactor, indent+1);

    OutputTextureView("clearcoat_texture", &p->m_ClearcoatTexture, indent+1);
    OutputTextureView("clearcoat_roughness_texture", &p->m_ClearcoatRoughnessTexture, indent+1);
    OutputTextureView("clearcoat_normal_texture", &p->m_ClearcoatNormalTexture, indent+1);
}

static void OutputTransmission(Transmission* p, int indent)
{
    OutputIndent(indent);
    printf("transmission\n");
    OutputValue("transmission_factor", p->m_TransmissionFactor, indent+1);
    OutputTextureView("transmission_texture", &p->m_TransmissionTexture, indent+1);
}

static void OutputIor(Ior* p, int indent)
{
    OutputIndent(indent);
    printf("transmission\n");
    OutputValue("ior", p->m_Ior, indent+1);
}

static void OutputSpecular(Specular* p, int indent)
{
    OutputIndent(indent);
    printf("specular\n");
    OutputArray("specular_color_factor", p->m_SpecularColorFactor, indent+1);
    OutputValue("specular_factor", p->m_SpecularFactor, indent+1);
    OutputTextureView("specular_texture", &p->m_SpecularTexture, indent+1);
    OutputTextureView("specular_color_texture", &p->m_SpecularColorTexture, indent+1);
}

static void OutputVolume(Volume* p, int indent)
{
    OutputIndent(indent);
    printf("volume\n");
    OutputArray("attenuation_color", p->m_AttenuationColor, indent+1);
    OutputValue("attenuation_distance", p->m_AttenuationDistance, indent+1);
    OutputValue("thickness_factor", p->m_ThicknessFactor, indent+1);
    OutputTextureView("thickness_texture", &p->m_ThicknessTexture, indent+1);
}

static void OutputSheen(Sheen* p, int indent)
{
    OutputIndent(indent);
    printf("sheen\n");
    OutputArray("sheen_color_factor", p->m_SheenColorFactor, indent+1);
    OutputValue("sheen_roughness_factor", p->m_SheenRoughnessFactor, indent+1);
    OutputTextureView("sheen_color_texture", &p->m_SheenColorTexture, indent+1);
    OutputTextureView("sheen_roughness_texture", &p->m_SheenRoughnessTexture, indent+1);
}

static void OutputEmissiveStrength(EmissiveStrength* p, int indent)
{
    OutputIndent(indent);
    printf("emissive_strength\n");
    OutputValue("emissive_strength", p->m_EmissiveStrength, indent+1);
}

static void OutputIridescence(Iridescence* p, int indent)
{
    OutputIndent(indent);
    printf("iridescence\n");
    OutputValue("iridescence_factor", p->m_IridescenceFactor, indent+1);
    OutputTextureView("iridescence_texture", &p->m_IridescenceTexture, indent+1);

    OutputValue("iridescence_ior", p->m_IridescenceIor, indent+1);
    OutputValue("iridescence_thickness_min", p->m_IridescenceThicknessMin, indent+1);
    OutputValue("iridescence_thickness_max", p->m_IridescenceThicknessMax, indent+1);
    OutputTextureView("iridescence_thickness_texture", &p->m_IridescenceThicknessTexture, indent+1);
}

static void OutputMaterial(Material* material, int indent)
{
    OutputIndent(indent);
    printf("material  %s\n", material->m_Name);

    if (material->m_PbrMetallicRoughness)   OutputPbrMetallicRoughness(material->m_PbrMetallicRoughness, indent+1);
    if (material->m_PbrSpecularGlossiness)  OutputPbrSpecularGlossiness(material->m_PbrSpecularGlossiness, indent+1);
    if (material->m_Clearcoat)              OutputClearcoat(material->m_Clearcoat, indent+1);
    if (material->m_Transmission)           OutputTransmission(material->m_Transmission, indent+1);
    if (material->m_Ior)                    OutputIor(material->m_Ior, indent+1);
    if (material->m_Specular)               OutputSpecular(material->m_Specular, indent+1);
    if (material->m_Volume)                 OutputVolume(material->m_Volume, indent+1);
    if (material->m_Sheen)                  OutputSheen(material->m_Sheen, indent+1);
    if (material->m_EmissiveStrength)       OutputEmissiveStrength(material->m_EmissiveStrength, indent+1);
    if (material->m_Iridescence)            OutputIridescence(material->m_Iridescence, indent+1);

    OutputTextureView("normal_texture", &material->m_NormalTexture, indent+1);
    OutputTextureView("occlusion_texture", &material->m_OcclusionTexture, indent+1);
    OutputTextureView("emissive_texture", &material->m_EmissiveTexture, indent+1);

    OutputArray("emissive_factor", material->m_EmissiveFactor, indent+1);
    OutputValue("alpha_cutoff", material->m_AlphaCutoff, indent+1);
    OutputValue("alpha_mode", material->m_AlphaMode, indent+1);
    OutputValue("double_sided", material->m_DoubleSided, indent+1);
    OutputValue("unlit", material->m_Unlit, indent+1);
}

static void OutputMesh(Mesh* mesh, int indent)
{
    OutputIndent(indent);

    const char* material_name = (mesh->m_Material && mesh->m_Material->m_Name) ? mesh->m_Material->m_Name : "null";

    printf("mesh  %s  vertices: %u  indices: %u  mat: %s  weights: %s  colors: %s aabb: (%f, %f, %f) (%f, %f, %f)\n",
            mesh->m_Name?mesh->m_Name:"null", mesh->m_VertexCount, mesh->m_Indices.Size(),
            material_name, mesh->m_Weights.Empty()?"no":"yes", mesh->m_Colors.Empty()?"no":"yes",
            mesh->m_Aabb.m_Min.x, mesh->m_Aabb.m_Min.y, mesh->m_Aabb.m_Min.z,
            mesh->m_Aabb.m_Max.x, mesh->m_Aabb.m_Max.y, mesh->m_Aabb.m_Max.z);

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
    printf("%s   meshes count: %u", model->m_Name, model->m_Meshes.Size());
        if (model->m_ParentBone)
        {
            printf("  bone: %s", model->m_ParentBone->m_Name);
        }
    printf("\n");
    for (uint32_t i = 0; i < model->m_Meshes.Size(); ++i)
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
    printf("# translation keys: %u\n", node_animation->m_TranslationKeys.Size());
    for (uint32_t i = 0; i < node_animation->m_TranslationKeys.Size(); ++i)
    {
        OutputIndent(indent+1);
        KeyFrame* key = &node_animation->m_TranslationKeys[i];
        printf("%.3f:  %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2]);
    }

    OutputIndent(indent);
    printf("# rotation keys: %u\n", node_animation->m_RotationKeys.Size());
    for (uint32_t i = 0; i < node_animation->m_RotationKeys.Size(); ++i)
    {
        OutputIndent(indent+1);
        KeyFrame* key = &node_animation->m_RotationKeys[i];
        printf("%.3f:  %.3f, %.3f, %.3f, %.3f\n", key->m_Time, key->m_Value[0], key->m_Value[1], key->m_Value[2], key->m_Value[3]);
    }

    OutputIndent(indent);
    printf("# scale keys: %u\n", node_animation->m_ScaleKeys.Size());
    for (uint32_t i = 0; i < node_animation->m_ScaleKeys.Size(); ++i)
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

    for (uint32_t i = 0; i < animation->m_NodeAnimations.Size(); ++i)
    {
        NodeAnimation* node_animation = &animation->m_NodeAnimations[i];
        OutputNodeAnimation(node_animation, indent+1);
    }
}

void DebugScene(Scene* scene)
{
    if (!scene)
    {
        printf("Output model importer scene: Scene is null!\n");
        return;
    }

    printf("Output model importer scene:\n");

    printf("------------------------------\n");
    printf("Buffers\n");
    for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
    {
        OutputIndent(1);
        printf("Buffer uri: '%.48s' sz: %u  %p\n", scene->m_Buffers[i].m_Uri, scene->m_Buffers[i].m_BufferCount, scene->m_Buffers[i].m_Buffer);
    }

    printf("------------------------------\n");
    printf("Samplers\n");

    for (uint32_t i = 0; i < scene->m_Samplers.Size(); ++i)
    {
        OutputSampler(&scene->m_Samplers[i], 0);
    }

    printf("------------------------------\n");
    printf("Textures\n");

    for (uint32_t i = 0; i < scene->m_Samplers.Size(); ++i)
    {
        OutputTexture(&scene->m_Textures[i], 0);
    }

    printf("------------------------------\n");
    printf("Materials\n");

    for (uint32_t i = 0; i < scene->m_Materials.Size(); ++i)
    {
        OutputMaterial(&scene->m_Materials[i], 0);
    }

    for (uint32_t i = 0; i < scene->m_DynamicMaterials.Size(); ++i)
    {
        OutputMaterial(scene->m_DynamicMaterials[i], 0);
    }

    printf("------------------------------\n");
    for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
    {
        OutputNode(&scene->m_Nodes[i]);
    }
    printf("------------------------------\n");

    printf("Subscenes: count: %u\n", scene->m_RootNodes.Size());
    for (uint32_t i = 0; i < scene->m_RootNodes.Size(); ++i)
    {
        printf("------------------------------\n");
        OutputNodeTree(scene->m_RootNodes[i], 1);
        printf("------------------------------\n");
    }

    printf("Skins: count: %u\n", scene->m_Skins.Size());
    for (uint32_t i = 0; i < scene->m_Skins.Size(); ++i)
    {
        printf("------------------------------\n");
        OutputSkin(&scene->m_Skins[i], 1);
        printf("------------------------------\n");
    }

    printf("Models: count: %u\n", scene->m_Models.Size());
    for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
    {
        printf("------------------------------\n");
        OutputModel(&scene->m_Models[i], 1);
        printf("------------------------------\n");
    }

    printf("Animations: count: %u\n", scene->m_Animations.Size());
    for (uint32_t i = 0; i < scene->m_Animations.Size(); ++i)
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
    OutputIndent(indent); printf("  m_Children: %p\n", node->m_Children.Begin());
    OutputIndent(indent); printf("  m_ChildrenCount: %u\n", node->m_Children.Size());
}

static void DebugStructNodeTree(Node* node, int indent)
{
    DebugStructNode(node, indent);

    for (uint32_t i = 0; i < node->m_Children.Size(); ++i)
    {
        DebugStructNodeTree(node->m_Children[i], indent+1);
    }
}

static void DebugStructMesh(Mesh* mesh, int indent)
{
    OutputIndent(indent); printf("Mesh: %p\n", mesh);
    assert(mesh->m_Name);
    assert(mesh->m_Material);
    OutputIndent(indent); printf("  m_Name: %p (%s)\n", mesh->m_Name, mesh->m_Name);
    OutputIndent(indent); printf("  m_Material: %p (%s)\n", mesh->m_Material->m_Name, mesh->m_Material->m_Name);

    OutputIndent(indent); printf("  m_Positions: %p\n", mesh->m_Positions.Begin());
    OutputIndent(indent); printf("  m_Normals: %p\n", mesh->m_Normals.Begin());
    OutputIndent(indent); printf("  m_Tangents: %p\n", mesh->m_Tangents.Begin());
    OutputIndent(indent); printf("  m_Colors: %p\n", mesh->m_Colors.Begin());
    OutputIndent(indent); printf("  m_Weights: %p\n", mesh->m_Weights.Begin());
    OutputIndent(indent); printf("  m_Bones: %p\n", mesh->m_Bones.Begin());

    OutputIndent(indent); printf("  m_TexCoords0: %p\n", mesh->m_TexCoords0.Begin());
    OutputIndent(indent); printf("  m_TexCoords0NumComponents: %u\n", mesh->m_TexCoords0NumComponents);
    OutputIndent(indent); printf("  m_TexCoords1: %p\n", mesh->m_TexCoords1.Begin());
    OutputIndent(indent); printf("  m_TexCoords1NumComponents: %u\n", mesh->m_TexCoords1NumComponents);

    OutputIndent(indent); printf("  m_VertexCount: %u\n", mesh->m_VertexCount);
    OutputIndent(indent); printf("  m_Indices: %p\n", mesh->m_Indices.Begin());
    OutputIndent(indent); printf("  m_Indices.size: %u\n", mesh->m_Indices.Size());
}

static void DebugStructModel(Model* model, int indent)
{
    OutputIndent(indent); printf("Model: %p\n", model);
    assert(model->m_Name);
    OutputIndent(indent); printf("  m_Name: %p (%s)\n", model->m_Name, model->m_Name);
    OutputIndent(indent); printf("  m_Meshes: %p\n", model->m_Meshes.Begin());
    OutputIndent(indent); printf("  m_Meshes.size¸: %u\n", model->m_Meshes.Size());

    for (uint32_t i = 0; i < model->m_Meshes.Size(); ++i) {
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
    printf("  m_Nodes: %p\n", scene->m_Nodes.Begin());
    printf("  m_NodesCount: %u\n", scene->m_Nodes.Size());
    printf("  m_Models: %p\n", scene->m_Models.Begin());
    printf("  m_ModelsCount: %u\n", scene->m_Models.Size());
    printf("  m_Skins: %p\n", scene->m_Skins.Begin());
    printf("  m_SkinsCount: %u\n", scene->m_Skins.Size());
    printf("  m_RootNodes: %p\n", scene->m_RootNodes.Begin());
    printf("  m_RootNodesCount: %u\n", scene->m_RootNodes.Size());
    printf("  m_Animations: %p\n", scene->m_Animations.Begin());
    printf("  m_AnimationsCount: %u\n", scene->m_Animations.Size());

    printf("-------------------------------\n");
    for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i) {
        DebugStructNode(&scene->m_Nodes[i], 1);
        printf("-------------------------------\n");
    }
    for (uint32_t i = 0; i < scene->m_Models.Size(); ++i) {
        DebugStructModel(&scene->m_Models[i], 1);
        printf("-------------------------------\n");
    }
    for (uint32_t i = 0; i < scene->m_Skins.Size(); ++i) {
        DebugStructSkin(&scene->m_Skins[i], 1);
        printf("-------------------------------\n");
    }
    for (uint32_t i = 0; i < scene->m_RootNodes.Size(); ++i) {
        DebugStructNodeTree(scene->m_RootNodes[i], 1);
        printf("-------------------------------\n");
    }
}

}
