
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
    free((void*)mesh->m_Name);
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

static void DestroyBone(Bone* bone)
{
    delete bone->m_Children;
    free((void*)bone->m_Name);
}

static void DestroySkin(Skin* skin)
{
    free((void*)skin->m_Name);
    for (uint32_t i = 0; i < skin->m_BonesCount; ++i)
        DestroyBone(&skin->m_Bones[i]);
    delete[] skin->m_Bones;
    delete[] skin->m_BoneRemap;
}

static void DestroyNodeAnimation(NodeAnimation* node_animation)
{
    delete[] node_animation->m_TranslationKeys;
    delete[] node_animation->m_RotationKeys;
    delete[] node_animation->m_ScaleKeys;
}

static void DestroyAnimation(Animation* animation)
{
    free((void*)animation->m_Name);
    for (uint32_t i = 0; i < animation->m_NodeAnimationsCount; ++i)
        DestroyNodeAnimation(&animation->m_NodeAnimations[i]);
    delete[] animation->m_NodeAnimations;
}

static void DestroyMaterial(Material* material)
{
    free((void*)material->m_Name);
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

    for (uint32_t i = 0; i < scene->m_NodesCount; ++i)
        DestroyNode(&scene->m_Nodes[i]);
    scene->m_NodesCount = 0;
    delete[] scene->m_Nodes;

    scene->m_RootNodesCount = 0;
    delete[] scene->m_RootNodes;

    for (uint32_t i = 0; i < scene->m_ModelsCount; ++i)
        DestroyModel(&scene->m_Models[i]);
    scene->m_ModelsCount = 0;
    delete[] scene->m_Models;

    for (uint32_t i = 0; i < scene->m_SkinsCount; ++i)
        DestroySkin(&scene->m_Skins[i]);
    scene->m_SkinsCount = 0;
    delete[] scene->m_Skins;

    for (uint32_t i = 0; i < scene->m_AnimationsCount; ++i)
        DestroyAnimation(&scene->m_Animations[i]);
    scene->m_AnimationsCount = 0;
    delete[] scene->m_Animations;

    for (uint32_t i = 0; i < scene->m_MaterialsCount; ++i)
        DestroyMaterial(&scene->m_Materials[i]);
    scene->m_MaterialsCount = 0;
    delete[] scene->m_Materials;

    for (uint32_t i = 0; i < scene->m_DynamicMaterialsCount; ++i)
        DestroyMaterial(scene->m_DynamicMaterials[i]);
    scene->m_DynamicMaterialsCount = 0;
    free((void*)scene->m_DynamicMaterials);

    scene->m_BuffersCount = 0;
    delete[] scene->m_Buffers;

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
        for (uint32_t i = 0; i < scene->m_BuffersCount; ++i)
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
    for (uint32_t i = 0; i < scene->m_BuffersCount; ++i)
    {
        if (!scene->m_Buffers[i].m_Buffer)
            return true;;
    }
    return false;
}

void EnableDebugLogging(bool enable)
{
    dmLogSetLevel( enable ? LOG_SEVERITY_DEBUG : LOG_SEVERITY_WARNING);
}

}
