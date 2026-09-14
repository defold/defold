// Copyright 2020-2026 The Defold Foundation
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

#include "text_layout.h"

static MarkupResult MarkupUnsupported(HMarkup* out_markup, MarkupError* out_error)
{
    if (out_markup)
    {
        *out_markup = 0;
    }

    if (out_error)
    {
        out_error->m_ByteOffset = 0;
        out_error->m_Type = MARKUP_ERROR_UNSUPPORTED;
    }

    return MARKUP_RESULT_UNSUPPORTED;
}

MarkupResult MarkupCreate(const char*, uint32_t, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupUnsupported(out_markup, out_error);
}

MarkupResult MarkupCreateStyleFragment(const char*, uint32_t, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupUnsupported(out_markup, out_error);
}

void MarkupDestroy(HMarkup)
{
}

const char* MarkupGetSource(HMarkup)
{
    return 0;
}

uint32_t MarkupGetSourceLength(HMarkup)
{
    return 0;
}

const uint32_t* MarkupGetText(HMarkup)
{
    return 0;
}

uint32_t MarkupGetTextLength(HMarkup)
{
    return 0;
}

MarkupResult MarkupFilterText(const char*, uint32_t, const uint32_t*, uint32_t, char*, uint32_t, uint32_t* output_length, MarkupError* error)
{
    if (output_length)
    {
        *output_length = 0;
    }

    return MarkupUnsupported(0, error);
}

const MarkupSpan* MarkupGetSpans(HMarkup)
{
    return 0;
}

uint32_t MarkupGetSpanCount(HMarkup)
{
    return 0;
}

const MarkupStyleNode* MarkupGetStyleNodes(HMarkup)
{
    return 0;
}

uint32_t MarkupGetStyleNodeCount(HMarkup)
{
    return 0;
}

const MarkupAttribute* MarkupGetAttributes(HMarkup)
{
    return 0;
}

uint32_t MarkupGetAttributeCount(HMarkup)
{
    return 0;
}

bool TextLayoutResolveMarkup(HFontCollection, HMarkup, TextLayoutSettings*, ResolvedMarkup*)
{
    return false;
}

bool TextLayoutCompileStyleFragment(const char*, uint32_t, TextRenderStyle*, dmArray<TextEffect>*, TextNamedStyleDecoration*, MarkupError*)
{
    return false;
}
