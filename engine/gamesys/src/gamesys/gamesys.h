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

#ifndef DM_GAMESYS_H
#define DM_GAMESYS_H

#include <stdint.h>
#include <string.h>
#include <script/script.h>

#include <resource/resource.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/lua/lua.h>
#include <dmsdk/gameobject/gameobject.h>

#include <gui/gui.h>
#include <input/input.h>
#include <render/render.h>
#include <physics/physics.h>

namespace dmMessage { struct URL; }

namespace dmGameSystem
{
    /// Config key to use for tweaking maximum number of collisions reported
    extern const char* PHYSICS_MAX_COLLISIONS_KEY;
    /// Config key to use for tweaking maximum number of contacts reported
    extern const char* PHYSICS_MAX_CONTACTS_KEY;
    /// Config key to use for tweaking the physics frame rate
    extern const char* PHYSICS_USE_FIXED_TIMESTEP;
    /// Config key for using max updates during a single step
    extern const char* PHYSICS_MAX_FIXED_TIMESTEPS;
    /// Config key to use for tweaking maximum number of collection proxies
    extern const char* COLLECTION_PROXY_MAX_COUNT_KEY;
    /// Config key to use for tweaking maximum number of factories
    extern const char* FACTORY_MAX_COUNT_KEY;
    /// Config key to use for tweaking maximum number of collection factories
    extern const char* COLLECTION_FACTORY_MAX_COUNT_KEY;

    struct TilemapContext
    {
        TilemapContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        uint32_t                    m_MaxTilemapCount;
        uint32_t                    m_MaxTileCount;
    };

    struct LabelContext
    {
        LabelContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        uint32_t                    m_MaxLabelCount;
        uint32_t                    m_Subpixels : 1;
    };

    struct PhysicsContext
    {
        union
        {
            dmPhysics::HContext3D m_Context3D;
            dmPhysics::HContext2D m_Context2D;
        };
        uint32_t    m_MaxCollisionCount;
        uint32_t    m_MaxContactPointCount;
        bool        m_Debug;
        bool        m_3D;
        bool        m_UseFixedTimestep;
        uint32_t    m_MaxFixedTimesteps;
    };

    struct ParticleFXContext
    {
        ParticleFXContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory m_Factory;
        dmRender::HRenderContext m_RenderContext;
        uint32_t m_MaxParticleFXCount;
        uint32_t m_MaxParticleCount;
        uint32_t m_MaxEmitterCount;
        bool m_Debug;
    };

    struct RenderScriptPrototype
    {
        dmArray<dmRender::HMaterial>    m_Materials;
        dmhash_t                        m_NameHash;
        dmRender::HRenderScriptInstance m_Instance;
        dmRender::HRenderScript         m_Script;
    };

    struct SpriteContext
    {
        SpriteContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        uint32_t                    m_MaxSpriteCount;
        uint32_t                    m_Subpixels : 1;
    };

    struct ModelContext
    {
        ModelContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        dmResource::HFactory        m_Factory;
        uint32_t                    m_MaxModelCount;
    };

    struct SoundContext
    {
        SoundContext()
        {
            memset(this, 0, sizeof(*this));
        }
        uint32_t                    m_MaxComponentCount;
        uint32_t                    m_MaxSoundInstances;
    };

    struct MeshContext
    {
        MeshContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        dmResource::HFactory        m_Factory;
        uint32_t                    m_MaxMeshCount;
    };

    struct ScriptLibContext
    {
        ScriptLibContext();

        lua_State*              m_LuaState;
        dmResource::HFactory    m_Factory;
        dmGameObject::HRegister m_Register;
        dmHID::HContext         m_HidContext;
    };


    struct CollectionProxyContext
    {
        CollectionProxyContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmResource::HFactory m_Factory;
        uint32_t m_MaxCollectionProxyCount;
    };

    struct FactoryContext
    {
        FactoryContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmScript::HContext m_ScriptContext;
        uint32_t m_MaxFactoryCount;
    };

    struct CollectionFactoryContext
    {
        CollectionFactoryContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmScript::HContext m_ScriptContext;
        uint32_t m_MaxCollectionFactoryCount;
    };

    bool InitializeScriptLibs(const ScriptLibContext& context);
    void FinalizeScriptLibs(const ScriptLibContext& context);

    dmResource::Result RegisterResourceTypes(dmResource::HFactory factory,
        dmRender::HRenderContext render_context,
        dmInput::HContext input_context,
        PhysicsContext* physics_context);

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderContext render_context,
                                                  PhysicsContext* physics_context,
                                                  ParticleFXContext* emitter_context,
                                                  SpriteContext* sprite_context,
                                                  CollectionProxyContext* collection_proxy_context,
                                                  FactoryContext* factory_context,
                                                  CollectionFactoryContext *collectionfactory_context,
                                                  ModelContext* model_context,
                                                  MeshContext* Mesh_context,
                                                  LabelContext* label_context,
                                                  TilemapContext* tilemap_context,
                                                  SoundContext* sound_context);

    void OnWindowFocus(bool focus);
    void OnWindowIconify(bool iconfiy);
    void OnWindowResized(int width, int height);
    void OnWindowCreated(int width, int height);
}

#endif // DM_GAMESYS_H
