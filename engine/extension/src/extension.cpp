#include <dlib/log.h>
#include <dlib/static_assert.h>
#include "extension.h"

namespace dmExtension
{
    Desc* g_FirstExtension = 0;
    Desc* g_CurrentExtension = 0;
    extern const size_t m_ExtensionDescBufferSize;

    AppParams::AppParams()
    {
        memset(this, 0, sizeof(*this));
    }

    Params::Params()
    {
        memset(this, 0, sizeof(*this));
    }

    void Register(struct Desc* desc,
        uint32_t desc_size,
        const char *name,
        Result (*app_init)(AppParams*),
        Result (*app_finalize)(AppParams*),
        Result (*initialize)(Params*),
        Result (*finalize)(Params*),
        Result (*update)(Params*),
        void   (*on_event)(Params*, const Event*))
    {
        DM_STATIC_ASSERT(dmExtension::m_ExtensionDescBufferSize >= sizeof(struct Desc), Invalid_Struct_Size);
        desc->m_Name = name;
        desc->AppInitialize = app_init;
        desc->AppFinalize = app_finalize;
        desc->Initialize = initialize;
        desc->Finalize = finalize;
        desc->Update = update;
        desc->OnEvent = on_event;
        desc->m_Next = g_FirstExtension;

        desc->PreRender = 0x0;
        desc->PostRender = 0x0;
        g_FirstExtension = desc;
    }

    const Desc* GetFirstExtension()
    {
        return g_FirstExtension;
    }

    Result AppInitialize(AppParams* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        uint32_t i = 0;
        Result ret = RESULT_OK;
        while (ed) {
            if (ed->AppInitialize) {
                g_CurrentExtension = ed;
                dmExtension::Result r = ed->AppInitialize(params);
                if (r != dmExtension::RESULT_OK) {
                    dmLogError("Failed to initialize (app-level) extension: %s", ed->m_Name);
                    ret = r;
                    break;
                } else {
                    ed->m_AppInitialized = true;
                }
            }
            ++i;
            ed = (dmExtension::Desc*) ed->m_Next;
        }

        g_CurrentExtension = 0x0;

        if (ret != RESULT_OK) {
            ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
            uint32_t i = 0;
            uint32_t n = i;
            while (ed && i < n) {
                if (ed->AppFinalize) {
                    dmExtension::Result r = ed->AppFinalize(params);
                    if (r != dmExtension::RESULT_OK) {
                        dmLogError("Failed to initialize (app-level) extension: %s", ed->m_Name);
                    }
                }
                ++i;
                ed = (dmExtension::Desc*) ed->m_Next;
            }
        }

        return ret;
    }

    void PreRender(Params* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->PreRender && ed->m_AppInitialized) {
                ed->PreRender(params);
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }
    }

    void PostRender(Params* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->PostRender && ed->m_AppInitialized) {
                ed->PostRender(params);
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }
    }

    bool RegisterCallback(CallbackType callback_type, extension_callback_t func)
    {
        if (!g_CurrentExtension) {
            dmLogError("Cannot call dmExtension::RegisterCallback outside of AppInitialize");
            return false;
        }

        switch (callback_type)
        {
            case dmExtension::CALLBACK_PRE_RENDER:
                g_CurrentExtension->PreRender = func;
                break;
            case dmExtension::CALLBACK_POST_RENDER:
                g_CurrentExtension->PostRender = func;
                break;
            default:
                return false;
        }

        return true;
    }

    Result AppFinalize(AppParams* params)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->AppFinalize && ed->m_AppInitialized) {
                ed->m_AppInitialized = false;
                dmExtension::Result r = ed->AppFinalize(params);
                if (r != dmExtension::RESULT_OK) {
                    dmLogError("Failed to finalize (app-level) extension: %s", ed->m_Name);
                }
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }

        return RESULT_OK;
    }


    void DispatchEvent(Params* params, const Event* event)
    {
        dmExtension::Desc* ed = (dmExtension::Desc*) dmExtension::GetFirstExtension();
        while (ed) {
            if (ed->OnEvent && ed->m_AppInitialized) {
                ed->OnEvent(params, event);
            }
            ed = (dmExtension::Desc*) ed->m_Next;
        }
    }

}

