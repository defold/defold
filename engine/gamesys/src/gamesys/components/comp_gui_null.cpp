// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include "comp_gui.h"

#include <dlib/message.h>

namespace dmGameSystem
{
    void GuiGetURLCallback(dmGui::HScene, dmMessage::URL* url)
    {
        *url = dmMessage::URL();
    }

    uintptr_t GuiGetUserDataCallback(dmGui::HScene)
    {
        return 0;
    }

    dmhash_t GuiResolvePathCallback(dmGui::HScene, const char*)
    {
        return 0;
    }

    void GuiGetTextMetricsCallback(FontResource*, const char*, float, bool, float, float, dmGui::TextMetrics* out_metrics)
    {
        out_metrics->m_Width = 0.0f;
        out_metrics->m_Height = 0.0f;
        out_metrics->m_MaxAscent = 0.0f;
        out_metrics->m_MaxDescent = 0.0f;
    }
} // namespace dmGameSystem
