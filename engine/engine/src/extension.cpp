#include "extension.h"
#include <dlib/static_assert.h>
#include <stddef.h> // offsetof

// Keeping backwards compatibility
DM_STATIC_ASSERT(offsetof(dmExtension::AppParams, m_ConfigFile) == offsetof(dmEngine::ExtensionAppParams, m_ConfigFile), Struct_Member_Offset_Mismatch);

namespace dmEngine
{
    dmConfigFile::HConfig GetConfigFile(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_ConfigFile;
    }

    dmWebServer::HServer GetWebServer(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_WebServer;
    }

    dmGameObject::HRegister GetGameObjectRegister(dmExtension::AppParams* app_params)
    {
        return ((dmEngine::ExtensionAppParams*)app_params)->m_GameObjectRegister;
    }
}
