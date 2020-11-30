#include "extension.h"
#include <stddef.h>
#include <dlib/static_assert.h>
#include <stddef.h> // offsetof

// The same struct is used on all platforms. Although not all platforms use C++11 or higher,
// we'll at least check on most platforms.
// (I don't want to add any runtime code to do these types of checks /MAWE)
#if __cplusplus >= 201103L
// Keeping backwards compatibility
DM_STATIC_ASSERT(offsetof(dmExtension::AppParams, m_ConfigFile) == offsetof(dmEngine::ExtensionAppParams, m_ConfigFile), Struct_Member_Offset_Mismatch);
#endif

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
