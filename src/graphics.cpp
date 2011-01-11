#include "graphics.h"

namespace dmGraphics
{
    WindowParams::WindowParams()
    : m_ResizeCallback(0x0)
    , m_Width(640)
    , m_Height(480)
    , m_Samples(1)
    , m_Title("Dynamo App")
    , m_Fullscreen(false)
    , m_PrintDeviceInfo(false)
    {

    }
}
