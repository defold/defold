#ifndef DM_GAMESYS_H
#define DM_GAMESYS_H

#include <dlib/configfile.h>

#include <script/script.h>

#include <resource/resource.h>

#include <gameobject/gameobject.h>

#include <gui/gui.h>
#include <input/input.h>
#include <render/render.h>
#include <render/font_renderer.h>
#include <physics/physics.h>
#include <rig/rig.h>

namespace dmGameSystem
{
    /// Config key to use for tweaking maximum number of collisions reported
    extern const char* PHYSICS_MAX_COLLISIONS_KEY;
    /// Config key to use for tweaking maximum number of contacts reported
    extern const char* PHYSICS_MAX_CONTACTS_KEY;
    /// Config key to use for tweaking maximum number of collection proxies
    extern const char* COLLECTION_PROXY_MAX_COUNT_KEY;
    /// Config key to use for tweaking maximum number of factories
    extern const char* FACTORY_MAX_COUNT_KEY;
    /// Config key to use for tweaking maximum number of collection factories
    extern const char* COLLECTION_FACTORY_MAX_COUNT_KEY;

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
        uint32_t m_MaxCollisionCount;
        uint32_t m_MaxContactPointCount;
        bool m_Debug;
        bool m_3D;
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
        bool m_Debug;
    };

    struct RenderScriptPrototype
    {
        dmArray<dmRender::HMaterial>    m_Materials;
        dmhash_t                        m_NameHash;
        dmRender::HRenderScriptInstance m_Instance;
        dmRender::HRenderScript         m_Script;
    };

    struct GuiContext
    {
        GuiContext();

        dmArray<void*>              m_Worlds;
        dmRender::HRenderContext    m_RenderContext;
        dmGui::HContext             m_GuiContext;
        dmScript::HContext          m_ScriptContext;
        uint32_t                    m_MaxGuiComponents;
        uint32_t                    m_MaxParticleFXCount;
        uint32_t                    m_MaxParticleCount;
        uint32_t                    m_MaxSpineCount;
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

    struct SpineModelContext
    {
        SpineModelContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        dmResource::HFactory        m_Factory;
        uint32_t                    m_MaxSpineModelCount;
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

    struct ScriptLibContext
    {
        ScriptLibContext();

        lua_State* m_LuaState;
        dmResource::HFactory m_Factory;
        dmGameObject::HRegister m_Register;
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
        GuiContext* gui_context,
        dmInput::HContext input_context,
        PhysicsContext* physics_context);

    dmGameObject::Result RegisterComponentTypes(dmResource::HFactory factory,
                                                  dmGameObject::HRegister regist,
                                                  dmRender::HRenderContext render_context,
                                                  PhysicsContext* physics_context,
                                                  ParticleFXContext* emitter_context,
                                                  GuiContext* gui_context,
                                                  SpriteContext* sprite_context,
                                                  CollectionProxyContext* collection_proxy_context,
                                                  FactoryContext* factory_context,
                                                  CollectionFactoryContext *collectionfactory_context,
                                                  SpineModelContext* spine_model_context,
                                                  ModelContext* model_context,
                                                  LabelContext* label_context);

    void GuiGetURLCallback(dmGui::HScene scene, dmMessage::URL* url);
    uintptr_t GuiGetUserDataCallback(dmGui::HScene scene);
    dmhash_t GuiResolvePathCallback(dmGui::HScene scene, const char* path, uint32_t path_size);
    void GuiGetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics);

    void OnWindowFocus(bool focus);
    void OnWindowResized(int width, int height);
}

#endif // DM_GAMESYS_H
