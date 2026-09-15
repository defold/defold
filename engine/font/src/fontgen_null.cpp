// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#include "fontgen.h"
#include "font_ttf.h"

#include <dmsdk/gamesys/resources/res_ttf.h>

bool FontGenIsSupported()
{
    return false;
}

dmExtension::Result FontGenInitialize(dmExtension::Params*)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FontGenFinalize(dmExtension::Params*)
{
    return dmExtension::RESULT_OK;
}

float FontGenGetBasePadding()
{
    return 3.0f;
}

float FontGenGetEdgeValue()
{
    return 191.0f;
}

FontGenJobData* FontGenCreateJobData(const FontGenParams*, uint32_t)
{
    return 0;
}

void FontGenDestroyJobData(FontGenJobData*)
{
}

HJob FontGenAddGlyphByIndex(FontGenJobData*, HFont, uint32_t)
{
    return 0;
}

HJob FontGenAddGlyphs(FontGenJobData*, TextGlyph*, uint32_t)
{
    return 0;
}

void FontGenFlushFinishedJobs(HJobContext, uint64_t)
{
}

HFont FontLoadFromMemoryTTF(const char*, const void*, uint32_t, bool)
{
    return 0;
}

namespace dmGameSystem
{
HFont GetFont(TTFResource*)
{
    return 0;
}
}

extern "C" void ResourceTypeTTF()
{
}
