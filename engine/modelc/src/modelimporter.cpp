
// Copyright 2020-2025 The Defold Foundation
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
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/dstrings.h>
#include <stdio.h>
#include <stdlib.h> // getenv
#include <string.h>


static void SetLogLevel()
{
    const char* env_debug_level = getenv("DM_LOG_LEVEL");
    if (!env_debug_level)
        return;

    LogSeverity severity = LOG_SEVERITY_WARNING;
#define STRMATCH(LEVEL) if (strcmp(env_debug_level, #LEVEL) == 0) \
        severity = LOG_SEVERITY_ ## LEVEL;

    STRMATCH(DEBUG);
    STRMATCH(USER_DEBUG);
    STRMATCH(INFO);
    STRMATCH(WARNING);
    STRMATCH(ERROR);
    STRMATCH(FATAL);
#undef STRMATCH

    dmLogSetLevel(severity);
}

struct ModelImporterInitializer
{
    ModelImporterInitializer() {
        SetLogLevel();
    }
} g_ModelImporterInitializer;


namespace dmModelImporter
{

Options::Options()
{
}

static void DestroySampler(Sampler* sampler)
{
    free((void*)sampler->m_Name);
}

static void DestroyTexture(Texture* texture)
{
    free((void*)texture->m_Name);
}

static void DestroyImage(Image* image)
{
    free((void*)image->m_Name);
    free((void*)image->m_Uri);
    free((void*)image->m_MimeType);
    //buffer->m_Buffer Memory owned by the gltf data
}

static void DestroyMesh(Mesh* mesh)
{
    mesh->m_Positions.SetCapacity(0);
    mesh->m_Normals.SetCapacity(0);
    mesh->m_Tangents.SetCapacity(0);
    mesh->m_Colors.SetCapacity(0);
    mesh->m_Weights.SetCapacity(0);
    mesh->m_Bones.SetCapacity(0);
    mesh->m_TexCoords0.SetCapacity(0);
    mesh->m_TexCoords1.SetCapacity(0);
    mesh->m_Indices.SetCapacity(0);
    free((void*)mesh->m_Name);
}

static void DestroyModel(Model* model)
{
    free((void*)model->m_Name);
    for (uint32_t i = 0; i < model->m_Meshes.Size(); ++i)
        DestroyMesh(&model->m_Meshes[i]);
    model->m_Meshes.SetCapacity(0);
}

static void DestroyNode(Node* node)
{
    free((void*)node->m_Name);
    node->m_Children.SetCapacity(0);
}

static void DestroyBone(Bone* bone)
{
    bone->m_Children.SetCapacity(0);
    free((void*)bone->m_Name);
    bone->m_Name = 0;
}

static void DestroySkin(Skin* skin)
{
    free((void*)skin->m_Name);
    for (uint32_t i = 0; i < skin->m_Bones.Size(); ++i)
        DestroyBone(&skin->m_Bones[i]);
    skin->m_Bones.SetCapacity(0);
    skin->m_BoneRemap.SetCapacity(0);
}

static void DestroyNodeAnimation(NodeAnimation* node_animation)
{
    node_animation->m_TranslationKeys.SetCapacity(0);
    node_animation->m_RotationKeys.SetCapacity(0);
    node_animation->m_ScaleKeys.SetCapacity(0);
}

static void DestroyAnimation(Animation* animation)
{
    free((void*)animation->m_Name);
    for (uint32_t i = 0; i < animation->m_NodeAnimations.Size(); ++i)
        DestroyNodeAnimation(&animation->m_NodeAnimations[i]);
    animation->m_NodeAnimations.SetCapacity(0);
}

static void DestroyMaterial(Material* material)
{
    free((void*)material->m_Name);
    delete material->m_PbrMetallicRoughness;
    delete material->m_PbrSpecularGlossiness;
    delete material->m_Clearcoat;
    delete material->m_Ior;
    delete material->m_Specular;
    delete material->m_Sheen;
    delete material->m_Transmission;
    delete material->m_Volume;
    delete material->m_EmissiveStrength;
    delete material->m_Iridescence;
}

static void DestroyBuffer(Buffer* buffer)
{
    free((void*)buffer->m_Uri);
    //buffer->m_Buffer Memory owned by the gltf data
}

bool Validate(Scene* scene)
{
    if (scene->m_ValidateFn)
        return scene->m_ValidateFn(scene);
    return true;
}

bool LoadFinalize(Scene* scene)
{
    if (scene->m_LoadFinalizeFn)
        return scene->m_LoadFinalizeFn(scene);
    return true;
}

void DestroyScene(Scene* scene)
{
    if (!scene)
    {
        return;
    }

    if (!scene->m_OpaqueSceneData)
    {
        printf("Already deleted!\n");
        return;
    }

    scene->m_DestroyFn(scene);
    scene->m_OpaqueSceneData = 0;

    for (uint32_t i = 0; i < scene->m_Nodes.Size(); ++i)
        DestroyNode(&scene->m_Nodes[i]);
    scene->m_Nodes.SetCapacity(0);

    scene->m_RootNodes.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Models.Size(); ++i)
        DestroyModel(&scene->m_Models[i]);
    scene->m_Models.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Skins.Size(); ++i)
        DestroySkin(&scene->m_Skins[i]);
    scene->m_Skins.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Animations.Size(); ++i)
        DestroyAnimation(&scene->m_Animations[i]);
    scene->m_Animations.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Materials.Size(); ++i)
        DestroyMaterial(&scene->m_Materials[i]);
    scene->m_Materials.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_DynamicMaterials.Size(); ++i)
    {
        DestroyMaterial(scene->m_DynamicMaterials[i]);
        delete scene->m_DynamicMaterials[i];
    }
    scene->m_DynamicMaterials.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
        DestroyBuffer(&scene->m_Buffers[i]);
    scene->m_Buffers.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Images.Size(); ++i)
        DestroyImage(&scene->m_Images[i]);
    scene->m_Images.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Textures.Size(); ++i)
        DestroyTexture(&scene->m_Textures[i]);
    scene->m_Textures.SetCapacity(0);

    for (uint32_t i = 0; i < scene->m_Samplers.Size(); ++i)
        DestroySampler(&scene->m_Samplers[i]);
    scene->m_Samplers.SetCapacity(0);

    delete scene;
}

Scene* LoadFromBuffer(Options* options, const char* suffix, void* data, uint32_t file_size)
{
    if (suffix == 0)
    {
        printf("ModelImporter: No suffix specified!\n");
        return 0;
    }

    if (dmStrCaseCmp(suffix, "gltf") == 0 || dmStrCaseCmp(suffix, "glb") == 0)
        return LoadGltfFromBuffer(options, data, file_size);

    printf("ModelImporter: File type not supported: %s\n", suffix);
    return 0;
}

static void* BufferResolveUri(const char* dirname, const char* uri, uint32_t* file_size)
{
    char path[512];
    dmStrlCpy(path, dirname, sizeof(path));
    dmStrlCat(path, "/", sizeof(path));
    dmStrlCat(path, uri, sizeof(path));

    return dmModelImporter::ReadFile(path, file_size);
}

Scene* LoadFromPath(Options* options, const char* path)
{
    const char* suffix = strrchr(path, '.') + 1;

    uint32_t file_size = 0;
    void* data = ReadFile(path, &file_size);
    if (!data)
    {
        printf("Failed to load '%s'\n", path);
        return 0;
    }

    Scene* scene = LoadFromBuffer(options, suffix, data, file_size);
    if (!scene)
    {
        dmLogError("Failed to create scene from path '%s'", path);
        return 0;
    }

    char dirname[512];
    dmStrlCpy(dirname, path, sizeof(dirname));
    char* c = strrchr(dirname, '/');
    if (!c)
        c = strrchr(dirname, '\\');
    if (c)
        *c = 0;

    if (dmModelImporter::NeedsResolve(scene))
    {
        for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
        {
            if (scene->m_Buffers[i].m_Buffer)
                continue;

            uint32_t mem_size = 0;
            void* mem = BufferResolveUri(dirname, scene->m_Buffers[i].m_Uri, &mem_size);
            dmModelImporter::ResolveBuffer(scene, scene->m_Buffers[i].m_Uri, mem, mem_size);
        }
    }

    if (!dmModelImporter::LoadFinalize(scene))
    {
        DestroyScene(scene);
        printf("Failed to load '%s'\n", path);
        return 0;
    }

    free(data);

    return scene;
}

bool NeedsResolve(Scene* scene)
{
    for (uint32_t i = 0; i < scene->m_Buffers.Size(); ++i)
    {
        if (!scene->m_Buffers[i].m_Buffer)
            return true;
    }
    return false;
}

void EnableDebugLogging(bool enable)
{
    dmLogSetLevel( enable ? LOG_SEVERITY_DEBUG : LOG_SEVERITY_WARNING);
}

}
