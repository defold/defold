#include "graphics.h"

namespace dmGraphics
{
    WindowParams::WindowParams()
    : m_ResizeCallback(0x0)
    , m_ResizeCallbackUserData(0x0)
    , m_CloseCallback(0x0)
    , m_CloseCallbackUserData(0x0)
    , m_Width(640)
    , m_Height(480)
    , m_Samples(1)
    , m_Title("Dynamo App")
    , m_Fullscreen(false)
    , m_PrintDeviceInfo(false)
    , m_HighDPI(false)
    {

    }

    ContextParams::ContextParams()
    : m_DefaultTextureMinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
    , m_DefaultTextureMagFilter(TEXTURE_FILTER_LINEAR)
    , m_VerifyGraphicsCalls(false)
    {

    }
}
