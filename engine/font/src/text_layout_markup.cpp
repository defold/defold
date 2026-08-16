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

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

template <typename T>
static void EnsurePushCapacity(dmArray<T>& array)
{
    if (array.Full())
    {
        array.SetCapacity(array.Capacity() ? array.Capacity() * 2 : 8);
    }
}

static const dmhash_t TAG_LINK   = dmHashString64("link");
static const dmhash_t TAG_SPRITE = dmHashString64("sprite");

static bool ParseFloat(const char* source, MarkupString string, float* value)
{
    if (string.m_Length == 0 || string.m_Length >= 64)
    {
        return false;
    }

    char buffer[64];
    memcpy(buffer, source + string.m_Offset, string.m_Length);
    buffer[string.m_Length] = 0;
    char* end = 0;
    *value = strtof(buffer, &end);

    return end == buffer + string.m_Length && isfinite(*value);
}

static int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }

    return -1;
}

static bool ParseColor(const char* source, MarkupString string, float color[4])
{
    uint32_t offset = string.m_Offset;
    uint32_t length = string.m_Length;

    if (length == 0 || source[offset] != '#')
    {
        return false;
    }

    ++offset;
    --length;

    if (length != 6 && length != 8)
    {
        return false;
    }

    uint32_t rgba = 0;

    for (uint32_t i = 0; i < length; ++i)
    {
        int digit = HexDigit(source[offset + i]);

        if (digit < 0)
        {
            return false;
        }

        rgba = (rgba << 4) | (uint32_t)digit;
    }

    if (length == 6)
    {
        rgba = (rgba << 8) | 0xff;
    }

    color[0] = ((rgba >> 24) & 0xff) / 255.0f;
    color[1] = ((rgba >> 16) & 0xff) / 255.0f;
    color[2] = ((rgba >> 8) & 0xff) / 255.0f;
    color[3] = (rgba & 0xff) / 255.0f;

    return true;
}

static const MarkupAttribute* FindAttribute(HMarkup markup, const MarkupStyleNode& node, MarkupAttributeType type)
{
    const MarkupAttribute* attributes = MarkupGetAttributes(markup);

    for (uint32_t i = 0; i < node.m_AttributeCount; ++i)
    {
        const MarkupAttribute& attribute = attributes[node.m_AttributeIndex + i];

        if (attribute.m_Type == type)
        {
            return &attribute;
        }
    }

    return 0;
}

static bool ParseEffectFit(const MarkupAttribute* attribute, uint8_t default_fit, uint8_t* fit)
{
    *fit = default_fit;

    if (!attribute)
    {
        return true;
    }

    if (attribute->m_Constant == MARKUP_CONSTANT_GLYPH)
    {
        *fit = TEXT_EFFECT_FIT_GLYPH;
    }
    else if (attribute->m_Constant == MARKUP_CONSTANT_SPAN)
    {
        *fit = TEXT_EFFECT_FIT_SPAN;
    }
    else
    {
        return false;
    }

    return true;
}

static bool ParseAnimationDirection(const MarkupAttribute* attribute, float* direction)
{
    *direction = 1.0f;

    if (!attribute || attribute->m_Constant == MARKUP_CONSTANT_FORWARD)
    {
        return true;
    }

    if (attribute->m_Constant == MARKUP_CONSTANT_REVERSE)
    {
        *direction = -1.0f;

        return true;
    }

    return false;
}

static bool ParseSize(HMarkup markup, const MarkupStyleNode& node, float base_size, float* size)
{
    const MarkupAttribute* attribute = FindAttribute(markup, node, MARKUP_ATTRIBUTE_SHORTHAND);

    if (!attribute)
    {
        attribute = FindAttribute(markup, node, MARKUP_ATTRIBUTE_VALUE);
    }

    if (!attribute)
    {
        return false;
    }

    const char*  source = MarkupGetSource(markup);
    MarkupString value = attribute->m_Value;
    float        number;
    float        resolved_size;

    if (value.m_Length > 0 && source[value.m_Offset + value.m_Length - 1] == '%')
    {
        --value.m_Length;

        if (!ParseFloat(source, value, &number))
        {
            return false;
        }

        resolved_size = base_size * number / 100.0f;
    }
    else if (value.m_Length > 2 && source[value.m_Offset + value.m_Length - 2] == 'e' && source[value.m_Offset + value.m_Length - 1] == 'm')
    {
        value.m_Length -= 2;

        if (!ParseFloat(source, value, &number))
        {
            return false;
        }

        resolved_size = base_size * number;
    }
    else
    {
        if (value.m_Length > 2 && source[value.m_Offset + value.m_Length - 2] == 'p' && source[value.m_Offset + value.m_Length - 1] == 'x')
        {
            value.m_Length -= 2;
        }

        if (!ParseFloat(source, value, &number))
        {
            return false;
        }

        resolved_size = (source[value.m_Offset] == '+' || source[value.m_Offset] == '-') ? base_size + number : number;
    }

    if (!isfinite(resolved_size) || resolved_size <= 0.0f)
    {
        return false;
    }

    *size = resolved_size;

    return true;
}

static bool ParseObjectDimension(const char* source, MarkupString value, float em, float* dimension)
{
    float number;

    if (value.m_Length > 0 && source[value.m_Offset + value.m_Length - 1] == '%')
    {
        --value.m_Length;

        if (!ParseFloat(source, value, &number))
        {
            return false;
        }

        number = em * number / 100.0f;
    }
    else if (value.m_Length > 2 && source[value.m_Offset + value.m_Length - 2] == 'e' && source[value.m_Offset + value.m_Length - 1] == 'm')
    {
        value.m_Length -= 2;

        if (!ParseFloat(source, value, &number))
        {
            return false;
        }

        number *= em;
    }
    else
    {
        if (value.m_Length > 2 && source[value.m_Offset + value.m_Length - 2] == 'p' && source[value.m_Offset + value.m_Length - 1] == 'x')
        {
            value.m_Length -= 2;
        }

        if (!ParseFloat(source, value, &number))
        {
            return false;
        }
    }

    if (!isfinite(number) || number <= 0.0f)
    {
        return false;
    }

    *dimension = number;

    return true;
}

static dmhash_t GetObjectTag(const MarkupStyleNode& node)
{
    if (node.m_Type == MARKUP_TAG_SPRITE)
    {
        return TAG_SPRITE;
    }

    if (node.m_Type == MARKUP_TAG_LINK)
    {
        return TAG_LINK;
    }

    return 0;
}

// Resolves resources after all object and attribute arrays have stable storage.
static bool ResolveObjectResources(TextLayoutSettings* settings, ResolvedMarkup* resolved)
{
    for (uint32_t i = 0; i < resolved->m_Objects.Size(); ++i)
    {
        TextLayoutObject& object = resolved->m_Objects[i];

        if (object.m_Tag == TAG_LINK)
        {
            continue;
        }

        const float proposed_width = object.m_Width;
        const float proposed_height = object.m_Height;
        const bool  success = settings->m_ResolveObject(settings->m_ObjectContext,
                                                        resolved->m_ObjectSource.Begin(),
                                                        resolved->m_ObjectAttributes.Begin(),
                                                        proposed_width,
                                                        proposed_height,
                                                        &object) != 0;
        if (success && isfinite(object.m_Width) && isfinite(object.m_Height) && object.m_Width > 0.0f && object.m_Height > 0.0f)
        {
            continue;
        }

        if (settings->m_ReleaseObject)
        {
            const uint32_t release_count = success ? i + 1 : i;

            for (uint32_t j = 0; j < release_count; ++j)
            {
                const TextLayoutObject& acquired = resolved->m_Objects[j];

                if (acquired.m_Tag == TAG_SPRITE)
                {
                    settings->m_ReleaseObject(settings->m_ObjectContext, &acquired);
                }
            }
        }

        return false;
    }

    return true;
}

static bool ResolveObjects(HMarkup markup, TextLayoutSettings* settings, ResolvedMarkup* resolved)
{
    const char*            source = MarkupGetSource(markup);
    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const MarkupAttribute* attributes = MarkupGetAttributes(markup);
    const uint32_t         node_count = MarkupGetStyleNodeCount(markup);

    uint32_t               object_count = 0;
    uint32_t               object_attribute_count = 0;

    for (uint32_t i = 1; i < node_count; ++i)
    {
        if (GetObjectTag(nodes[i]))
        {
            ++object_count;
            object_attribute_count += nodes[i].m_AttributeCount;
        }
    }

    if (object_count == 0)
    {
        return true;
    }

    if (object_count > MARKUP_INVALID_INDEX || object_attribute_count > MARKUP_INVALID_INDEX)
    {
        return false;
    }

    const uint32_t source_length = MarkupGetSourceLength(markup);
    resolved->m_ObjectSource.SetCapacity(source_length + 1);
    resolved->m_ObjectSource.SetSize(source_length + 1);
    memcpy(resolved->m_ObjectSource.Begin(), source, source_length + 1);
    resolved->m_Objects.SetCapacity(object_count);
    resolved->m_ObjectAttributes.SetCapacity(object_attribute_count);

    for (uint32_t i = 1; i < node_count; ++i)
    {
        const dmhash_t tag = GetObjectTag(nodes[i]);

        if (!tag)
        {
            continue;
        }

        TextLayoutObject object = {};
        object.m_Tag = tag;
        object.m_TextOffset = nodes[i].m_TextOffset;
        object.m_TextLength = nodes[i].m_TextLength;
        const MarkupAttribute* id = FindAttribute(markup, nodes[i], MARKUP_ATTRIBUTE_ID);
        object.m_Id = id && id->m_Value.m_Length
                          ? dmHashBuffer64(source + id->m_Value.m_Offset, id->m_Value.m_Length)
                          : 0x8000000000000000ULL | (resolved->m_Objects.Size() + 1);
        object.m_AttributeIndex = (uint16_t)resolved->m_ObjectAttributes.Size();
        object.m_AttributeCount = nodes[i].m_AttributeCount;

        for (uint32_t j = 0; j < nodes[i].m_AttributeCount; ++j)
        {
            const MarkupAttribute&    source_attribute = attributes[nodes[i].m_AttributeIndex + j];
            TextLayoutObjectAttribute attribute = {
                source_attribute.m_Name.m_Offset,
                source_attribute.m_Value.m_Offset,
                source_attribute.m_Name.m_Length,
                source_attribute.m_Value.m_Length,
            };
            EnsurePushCapacity(resolved->m_ObjectAttributes);
            resolved->m_ObjectAttributes.Push(attribute);
        }

        if (tag != TAG_LINK)
        {
            if (!settings->m_ResolveObject)
            {
                return false;
            }

            float                  proposed_width = settings->m_Size;
            float                  proposed_height = settings->m_Size;
            const MarkupAttribute* width = FindAttribute(markup, nodes[i], MARKUP_ATTRIBUTE_WIDTH);
            const MarkupAttribute* height = FindAttribute(markup, nodes[i], MARKUP_ATTRIBUTE_HEIGHT);

            if ((width && !ParseObjectDimension(source, width->m_Value, settings->m_Size, &proposed_width)) ||
                (height && !ParseObjectDimension(source, height->m_Value, settings->m_Size, &proposed_height)))
            {
                return false;
            }
            object.m_Width = proposed_width;
            object.m_Height = proposed_height;
        }

        EnsurePushCapacity(resolved->m_Objects);
        resolved->m_Objects.Push(object);
    }

    return ResolveObjectResources(settings, resolved);
}

static bool StyleEquals(const TextRenderStyle& a, const TextRenderStyle& b)
{
    return memcmp(&a, &b, sizeof(a)) == 0;
}

static bool AddStyle(ResolvedMarkup* resolved, const TextRenderStyle& style, uint16_t* style_index)
{
    for (uint32_t i = 0; i < resolved->m_Styles.Size(); ++i)
    {
        if (StyleEquals(resolved->m_Styles[i], style))
        {
            *style_index = (uint16_t)i;

            return true;
        }
    }

    if (resolved->m_Styles.Size() == MARKUP_INVALID_INDEX)
    {
        return false;
    }

    EnsurePushCapacity(resolved->m_Styles);
    resolved->m_Styles.Push(style);
    *style_index = (uint16_t)(resolved->m_Styles.Size() - 1);

    return true;
}

static bool ApplyStyleNode(HMarkup markup, const MarkupStyleNode& node, float base_size, TextRenderStyle* style)
{
    const char* source = MarkupGetSource(markup);

    if (node.m_Type == MARKUP_TAG_COLOR)
    {
        const MarkupAttribute* attribute = FindAttribute(markup, node, MARKUP_ATTRIBUTE_SHORTHAND);

        if (!attribute)
        {
            attribute = FindAttribute(markup, node, MARKUP_ATTRIBUTE_VALUE);
        }

        if (!attribute || !ParseColor(source, attribute->m_Value, style->m_FaceColor))
        {
            return false;
        }

        style->m_Flags |= TEXT_RENDER_STYLE_FACE_COLOR;
    }
    else if (node.m_Type == MARKUP_TAG_SIZE)
    {
        if (!ParseSize(markup, node, base_size, &style->m_FontSize))
        {
            return false;
        }

        style->m_Flags |= TEXT_RENDER_STYLE_FONT_SIZE;
    }
    else if (node.m_Type == MARKUP_TAG_OUTLINE)
    {
        const MarkupAttribute* size = FindAttribute(markup, node, MARKUP_ATTRIBUTE_SIZE);
        const MarkupAttribute* color = FindAttribute(markup, node, MARKUP_ATTRIBUTE_COLOR);

        if (!size && !color)
        {
            return false;
        }

        if (size && !ParseFloat(source, size->m_Value, &style->m_OutlineWidth))
        {
            return false;
        }

        if (size && style->m_OutlineWidth < 0.0f)
        {
            return false;
        }

        if (color && !ParseColor(source, color->m_Value, style->m_OutlineColor))
        {
            return false;
        }

        if (size)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_OUTLINE_WIDTH;
        }

        if (color)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_OUTLINE_COLOR;
        }
    }
    else if (node.m_Type == MARKUP_TAG_SHADOW)
    {
        const MarkupAttribute* color = FindAttribute(markup, node, MARKUP_ATTRIBUTE_COLOR);
        const MarkupAttribute* x = FindAttribute(markup, node, MARKUP_ATTRIBUTE_X);
        const MarkupAttribute* y = FindAttribute(markup, node, MARKUP_ATTRIBUTE_Y);
        const MarkupAttribute* blur = FindAttribute(markup, node, MARKUP_ATTRIBUTE_BLUR);

        if (!color && !x && !y && !blur)
        {
            return false;
        }

        if (color && !ParseColor(source, color->m_Value, style->m_ShadowColor))
        {
            return false;
        }

        if (x && !ParseFloat(source, x->m_Value, &style->m_ShadowX))
        {
            return false;
        }

        if (y && !ParseFloat(source, y->m_Value, &style->m_ShadowY))
        {
            return false;
        }

        if (blur && !ParseFloat(source, blur->m_Value, &style->m_ShadowBlur))
        {
            return false;
        }

        if (blur && style->m_ShadowBlur < 0.0f)
        {
            return false;
        }

        if (color)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_SHADOW_COLOR;
        }

        if (x)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_SHADOW_X;
        }

        if (y)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_SHADOW_Y;
        }

        if (blur)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_SHADOW_BLUR;
        }
    }

    return true;
}

static bool CreateEffect(HMarkup markup, const MarkupStyleNode& node, uint32_t text_offset, uint32_t text_length, TextEffect* effect)
{
    const char* source = MarkupGetSource(markup);
    memset(effect, 0, sizeof(*effect));
    effect->m_TextOffset = text_offset;
    effect->m_TextLength = text_length;

    if (node.m_Type == MARKUP_TAG_GRADIENT)
    {
        const MarkupAttribute* left                = 0;
        const MarkupAttribute* right               = 0;
        const MarkupAttribute* top                 = 0;
        const MarkupAttribute* bottom              = 0;
        const MarkupAttribute* bl                  = 0;
        const MarkupAttribute* br                  = 0;
        const MarkupAttribute* tl                  = 0;
        const MarkupAttribute* tr                  = 0;
        const MarkupAttribute* fit                 = 0;
        const MarkupAttribute* hz                  = 0;
        const MarkupAttribute* direction_attribute = 0;
        const MarkupAttribute* attributes          = MarkupGetAttributes(markup) + node.m_AttributeIndex;

        for (uint32_t i = 0; i < node.m_AttributeCount; ++i)
        {
            const MarkupAttribute* attribute = &attributes[i];

            switch (attribute->m_Type)
            {
                case MARKUP_ATTRIBUTE_LEFT:      left = attribute; break;
                case MARKUP_ATTRIBUTE_RIGHT:     right = attribute; break;
                case MARKUP_ATTRIBUTE_TOP:       top = attribute; break;
                case MARKUP_ATTRIBUTE_BOTTOM:    bottom = attribute; break;
                case MARKUP_ATTRIBUTE_BL:        bl = attribute; break;
                case MARKUP_ATTRIBUTE_BR:        br = attribute; break;
                case MARKUP_ATTRIBUTE_TL:        tl = attribute; break;
                case MARKUP_ATTRIBUTE_TR:        tr = attribute; break;
                case MARKUP_ATTRIBUTE_FIT:       fit = attribute; break;
                case MARKUP_ATTRIBUTE_HZ:        hz = attribute; break;
                case MARKUP_ATTRIBUTE_DIRECTION: direction_attribute = attribute; break;
            }
        }

        float direction;
        const bool horizontal = left && right && !top && !bottom && !bl && !br && !tl && !tr;
        const bool vertical   = top && bottom && !left && !right && !bl && !br && !tl && !tr;
        const bool quad       = bl && br && tl && tr && !left && !right && !top && !bottom;

        if (!horizontal && !vertical && !quad)
        {
            return false;
        }

        if (horizontal)
        {
            effect->m_Gradient.m_Mode = TEXT_GRADIENT_MODE_HORIZONTAL;

            if (!ParseColor(source, left->m_Value, effect->m_Gradient.m_BottomLeft) ||
                !ParseColor(source, right->m_Value, effect->m_Gradient.m_BottomRight))

                return false;
            memcpy(effect->m_Gradient.m_TopLeft, effect->m_Gradient.m_BottomLeft, sizeof(effect->m_Gradient.m_TopLeft));
            memcpy(effect->m_Gradient.m_TopRight, effect->m_Gradient.m_BottomRight, sizeof(effect->m_Gradient.m_TopRight));
        }
        else if (vertical)
        {
            effect->m_Gradient.m_Mode = TEXT_GRADIENT_MODE_VERTICAL;

            if (!ParseColor(source, bottom->m_Value, effect->m_Gradient.m_BottomLeft) ||
                !ParseColor(source, top->m_Value, effect->m_Gradient.m_TopLeft))

                return false;
            memcpy(effect->m_Gradient.m_BottomRight, effect->m_Gradient.m_BottomLeft, sizeof(effect->m_Gradient.m_BottomRight));
            memcpy(effect->m_Gradient.m_TopRight, effect->m_Gradient.m_TopLeft, sizeof(effect->m_Gradient.m_TopRight));
        }
        else
        {
            effect->m_Gradient.m_Mode = TEXT_GRADIENT_MODE_QUAD;

            if (!ParseColor(source, bl->m_Value, effect->m_Gradient.m_BottomLeft) ||
                !ParseColor(source, br->m_Value, effect->m_Gradient.m_BottomRight) ||
                !ParseColor(source, tl->m_Value, effect->m_Gradient.m_TopLeft) ||
                !ParseColor(source, tr->m_Value, effect->m_Gradient.m_TopRight))

                return false;
        }

        if (!ParseEffectFit(fit, TEXT_EFFECT_FIT_TEXT, &effect->m_Gradient.m_Fit))
        {
            return false;
        }

        if ((hz && !ParseFloat(source, hz->m_Value, &effect->m_Gradient.m_Hz)) || effect->m_Gradient.m_Hz < 0.0f ||
            !ParseAnimationDirection(direction_attribute, &direction))

            return false;
        effect->m_Gradient.m_Hz *= direction;
        effect->m_Type = TEXT_EFFECT_GRADIENT;
        effect->m_Flags = TEXT_EFFECT_AFFECTS_COLOR;

        return true;
    }

    if (node.m_Type == MARKUP_TAG_WAVE)
    {
        effect->m_Wave.m_Amplitude = 1.0f;
        effect->m_Wave.m_Hz = 1.0f;
        effect->m_Wave.m_Wavelength = 6.0f;
        const MarkupAttribute* amplitude = FindAttribute(markup, node, MARKUP_ATTRIBUTE_AMPLITUDE);
        const MarkupAttribute* hz = FindAttribute(markup, node, MARKUP_ATTRIBUTE_HZ);
        const MarkupAttribute* wavelength = FindAttribute(markup, node, MARKUP_ATTRIBUTE_WAVELENGTH);
        const MarkupAttribute* fit = FindAttribute(markup, node, MARKUP_ATTRIBUTE_FIT);
        const MarkupAttribute* direction_attribute = FindAttribute(markup, node, MARKUP_ATTRIBUTE_DIRECTION);
        float direction;

        if ((amplitude && !ParseFloat(source, amplitude->m_Value, &effect->m_Wave.m_Amplitude)) ||
            (hz && !ParseFloat(source, hz->m_Value, &effect->m_Wave.m_Hz)) ||
            (wavelength && !ParseFloat(source, wavelength->m_Value, &effect->m_Wave.m_Wavelength)) ||
            effect->m_Wave.m_Amplitude < 0.0f || effect->m_Wave.m_Hz < 0.0f || effect->m_Wave.m_Wavelength <= 0.0f ||
            !ParseEffectFit(fit, TEXT_EFFECT_FIT_GLYPH, &effect->m_Wave.m_Fit) ||
            !ParseAnimationDirection(direction_attribute, &direction))

            return false;
        effect->m_Wave.m_Hz *= direction;
        effect->m_Type = TEXT_EFFECT_WAVE;
        effect->m_Flags = TEXT_EFFECT_AFFECTS_POSITION;

        return true;
    }

    if (node.m_Type == MARKUP_TAG_SHAKE)
    {
        effect->m_Shake.m_Amplitude = 0.5f;
        effect->m_Shake.m_Hz = 20.0f;
        const MarkupAttribute* amplitude = FindAttribute(markup, node, MARKUP_ATTRIBUTE_AMPLITUDE);
        const MarkupAttribute* hz = FindAttribute(markup, node, MARKUP_ATTRIBUTE_HZ);
        const MarkupAttribute* fit = FindAttribute(markup, node, MARKUP_ATTRIBUTE_FIT);

        if ((amplitude && !ParseFloat(source, amplitude->m_Value, &effect->m_Shake.m_Amplitude)) ||
            (hz && !ParseFloat(source, hz->m_Value, &effect->m_Shake.m_Hz)) ||
            effect->m_Shake.m_Amplitude < 0.0f || effect->m_Shake.m_Hz < 0.0f ||
            !ParseEffectFit(fit, TEXT_EFFECT_FIT_GLYPH, &effect->m_Shake.m_Fit))

            return false;
        effect->m_Type = TEXT_EFFECT_SHAKE;
        effect->m_Flags = TEXT_EFFECT_AFFECTS_POSITION;

        return true;
    }

    return false;
}

static float MirroredWrap(float value)
{
    value = value - floorf(value * 0.5f) * 2.0f;

    return value <= 1.0f ? value : 2.0f - value;
}

static float Clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    if (value > 1.0f)
    {
        return 1.0f;
    }

    return value;
}

static void MultiplyGradient(float color[4], const TextGradientEffect& gradient, float x, float y)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        const float bottom = gradient.m_BottomLeft[i] + (gradient.m_BottomRight[i] - gradient.m_BottomLeft[i]) * x;
        const float top = gradient.m_TopLeft[i] + (gradient.m_TopRight[i] - gradient.m_TopLeft[i]) * x;
        color[i] *= bottom + (top - bottom) * y;
    }
}

static uint32_t MixHash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;

    return value ^ (value >> 16);
}

static float ShakeAngle(uint32_t effect_index, uint32_t glyph_key, uint32_t tick)
{
    const uint32_t hash = MixHash(glyph_key ^ MixHash(effect_index + 0x9e3779b9U) ^ MixHash(tick));

    return (hash & 0x00ffffffU) * (6.28318530717958647692f / 16777216.0f);
}

// Initializes render data from the glyph's static style before applying effects.
static void InitializeGlyphRenderData(const TextRenderStyle* style, const float base_color[4], TextGlyphRenderData* data)
{
    TextGlyphFaceColors* colors = &data->m_FaceColors;
    data->m_OffsetX = 0.0f;
    data->m_OffsetY = 0.0f;
    data->m_OutlineWidth = style ? style->m_OutlineWidth : 0.0f;
    data->m_ShadowX = style ? style->m_ShadowX : 0.0f;
    data->m_ShadowY = style ? style->m_ShadowY : 0.0f;
    data->m_ShadowBlur = style ? style->m_ShadowBlur : 0.0f;
    data->m_StyleFlags = style ? style->m_Flags : 0;

    for (uint32_t channel = 0; channel < 4; ++channel)
    {
        float value = base_color[channel];

        if (style && (style->m_Flags & TEXT_RENDER_STYLE_FACE_COLOR))
        {
            value *= style->m_FaceColor[channel];
        }

        colors->m_BottomLeft[channel] = value;
        colors->m_BottomRight[channel] = value;
        colors->m_TopLeft[channel] = value;
        colors->m_TopRight[channel] = value;
        data->m_OutlineColor[channel] = style && (style->m_Flags & TEXT_RENDER_STYLE_OUTLINE_COLOR) ? style->m_OutlineColor[channel] : 1.0f;
        data->m_ShadowColor[channel] = style && (style->m_Flags & TEXT_RENDER_STYLE_SHADOW_COLOR) ? style->m_ShadowColor[channel] : 1.0f;
    }

    if ((data->m_StyleFlags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) && data->m_OutlineWidth <= 0.0f)
    {
        data->m_OutlineColor[3] = 0.0f;
    }
}

// Applies one color sample uniformly to all four glyph corners.
static void MultiplyFaceColors(TextGlyphFaceColors* colors, const float sample[4])
{
    for (uint32_t channel = 0; channel < 4; ++channel)
    {
        colors->m_BottomLeft[channel] *= sample[channel];
        colors->m_BottomRight[channel] *= sample[channel];
        colors->m_TopLeft[channel] *= sample[channel];
        colors->m_TopRight[channel] *= sample[channel];
    }
}

// Applies a gradient using its fit mode to choose span, glyph, or corner samples.
static void ApplyGradientEffect(const TextLayout* layout, const TextGlyph& glyph, const TextEffect& effect, TextGlyphFaceColors* colors)
{
    const TextGradientEffect& gradient = effect.m_Gradient;
    const float animation_t = (float)(layout->m_ElapsedTime * gradient.m_Hz * 2.0);

    if (gradient.m_Fit == TEXT_EFFECT_FIT_SPAN)
    {
        const float first_glyph_center = 0.5f / effect.m_TextLength;
        float       sample_x;

        if (gradient.m_Mode == TEXT_GRADIENT_MODE_HORIZONTAL)
        {
            sample_x = MirroredWrap(fabsf(animation_t));

            if (gradient.m_Hz < 0.0f)
            {
                sample_x = 1.0f - sample_x;
            }
        }
        else
        {
            sample_x = MirroredWrap(first_glyph_center + animation_t);
        }

        const float sample_y = MirroredWrap(0.5f + animation_t);
        float       sample[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        MultiplyGradient(sample, gradient, sample_x, sample_y);
        MultiplyFaceColors(colors, sample);

        return;
    }

    if (gradient.m_Fit == TEXT_EFFECT_FIT_GLYPH && gradient.m_Mode == TEXT_GRADIENT_MODE_HORIZONTAL)
    {
        const float glyph_center = ((float)((int64_t)glyph.m_Cluster - effect.m_TextOffset) + 0.5f) / effect.m_TextLength;
        const float sample_x = MirroredWrap(glyph_center + animation_t);
        float       sample[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        MultiplyGradient(sample, gradient, sample_x, 0.5f);
        MultiplyFaceColors(colors, sample);

        return;
    }

    const bool  fit_text = gradient.m_Fit == TEXT_EFFECT_FIT_TEXT;
    const float left_u = fit_text ? Clamp01((float)((int64_t)glyph.m_Cluster - effect.m_TextOffset) / effect.m_TextLength) : 0.0f;
    const float right_u = fit_text ? Clamp01((float)((int64_t)glyph.m_Cluster + 1 - effect.m_TextOffset) / effect.m_TextLength) : 1.0f;
    const float left_t = MirroredWrap(left_u + animation_t);
    const float right_t = MirroredWrap(right_u + animation_t);
    const float bottom_t = MirroredWrap(animation_t);
    const float top_t = MirroredWrap(1.0f + animation_t);
    MultiplyGradient(colors->m_BottomLeft, gradient, left_t, bottom_t);
    MultiplyGradient(colors->m_BottomRight, gradient, right_t, bottom_t);
    MultiplyGradient(colors->m_TopLeft, gradient, left_t, top_t);
    MultiplyGradient(colors->m_TopRight, gradient, right_t, top_t);
}

// Interpolates deterministic shake samples so motion remains continuous.
static void ApplyShakeEffect(const TextLayout* layout, const TextGlyph& glyph, const TextEffect& effect, uint32_t effect_index, TextGlyphRenderData* data)
{
    const double   tick_time = layout->m_ElapsedTime * effect.m_Shake.m_Hz;
    const double   tick_floor = floor(tick_time);
    const uint32_t tick = (uint32_t)tick_floor;
    const float    t = (float)(tick_time - tick_floor);
    const uint32_t glyph_key = effect.m_Shake.m_Fit == TEXT_EFFECT_FIT_SPAN ? effect.m_TextOffset : glyph.m_Cluster;
    const float    current_angle = ShakeAngle(effect_index, glyph_key, tick);
    const float    next_angle = ShakeAngle(effect_index, glyph_key, tick + 1);
    const float    current_x = sinf(current_angle);
    const float    current_y = cosf(current_angle);
    data->m_OffsetX += (current_x + (sinf(next_angle) - current_x) * t) * effect.m_Shake.m_Amplitude;
    data->m_OffsetY += (current_y + (cosf(next_angle) - current_y) * t) * effect.m_Shake.m_Amplitude;
}

// Applies a sinusoidal vertical offset using either one span phase or per-glyph phases.
static void ApplyWaveEffect(const TextLayout* layout, const TextGlyph& glyph, const TextEffect& effect, TextGlyphRenderData* data)
{
    const float position = effect.m_Wave.m_Fit == TEXT_EFFECT_FIT_SPAN ? 0.0f :
                           (float)((int64_t)glyph.m_Cluster - effect.m_TextOffset);
    const double cycles = layout->m_ElapsedTime * effect.m_Wave.m_Hz;
    const float  t = (float)(cycles - floor(cycles));
    const float  phase = 6.28318530717958647692f * (t + position / effect.m_Wave.m_Wavelength);
    data->m_OffsetY += sinf(phase) * effect.m_Wave.m_Amplitude;
}

void TextLayoutGetGlyphRenderData(HTextLayout layout, const TextGlyph& glyph, const float base_color[4], TextGlyphRenderData* data)
{
    TextLayout* internal = (TextLayout*)layout;
    const TextRenderStyle* style = glyph.m_StyleIndex < internal->m_Styles.Size() ? &internal->m_Styles[glyph.m_StyleIndex] : 0;
    InitializeGlyphRenderData(style, base_color, data);

    if (glyph.m_MarkupSpanIndex >= internal->m_ResolvedSpans.Size())
    {
        return;
    }

    const TextResolvedSpan& span = internal->m_ResolvedSpans[glyph.m_MarkupSpanIndex];

    for (uint32_t i = 0; i < span.m_EffectCount; ++i)
    {
        const uint32_t    effect_index = internal->m_SpanEffects[span.m_EffectIndex + i];
        const TextEffect& effect = internal->m_Effects[effect_index];

        if (effect.m_Type == TEXT_EFFECT_GRADIENT)
        {
            if (effect.m_TextLength != 0)
            {
                ApplyGradientEffect(internal, glyph, effect, &data->m_FaceColors);
            }

            continue;
        }

        if (effect.m_Type == TEXT_EFFECT_SHAKE)
        {
            if (effect.m_Shake.m_Amplitude > 0.0f)
            {
                ApplyShakeEffect(internal, glyph, effect, effect_index, data);
            }

            continue;
        }

        if (effect.m_Type == TEXT_EFFECT_WAVE && effect.m_Wave.m_Amplitude != 0.0f)
        {
            ApplyWaveEffect(internal, glyph, effect, data);
        }
    }
}

bool TextLayoutHasMarkupOutline(HTextLayout layout)
{
    TextLayout* internal = (TextLayout*)layout;

    for (uint32_t i = 0; i < internal->m_Styles.Size(); ++i)
    {
        const TextRenderStyle& style = internal->m_Styles[i];

        if ((style.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH) && style.m_OutlineWidth > 0.0f)
        {
            return true;
        }
    }

    return false;
}

float TextLayoutGetMaxMarkupOutlineWidth(HTextLayout layout)
{
    TextLayout* internal = (TextLayout*)layout;
    float       width = 0.0f;

    for (uint32_t i = 0; i < internal->m_Styles.Size(); ++i)
    {
        const TextRenderStyle& style = internal->m_Styles[i];

        if (style.m_Flags & TEXT_RENDER_STYLE_OUTLINE_WIDTH)
        {
            width = fmaxf(width, style.m_OutlineWidth);
        }
    }

    return width;
}

bool TextLayoutHasMarkupShadow(HTextLayout layout)
{
    TextLayout* internal = (TextLayout*)layout;
    const uint32_t shadow_flags = TEXT_RENDER_STYLE_SHADOW_COLOR | TEXT_RENDER_STYLE_SHADOW_X | TEXT_RENDER_STYLE_SHADOW_Y | TEXT_RENDER_STYLE_SHADOW_BLUR;

    for (uint32_t i = 0; i < internal->m_Styles.Size(); ++i)
    {
        if (internal->m_Styles[i].m_Flags & shadow_flags)
        {
            return true;
        }
    }

    return false;
}

void TextLayoutGetGlyphFaceColors(HTextLayout layout, const TextGlyph& glyph, const float base_color[4], TextGlyphFaceColors* colors)
{
    TextGlyphRenderData data;
    TextLayoutGetGlyphRenderData(layout, glyph, base_color, &data);
    *colors = data.m_FaceColors;
}

static bool IsEffectNode(const MarkupStyleNode& node)
{
    return node.m_Type == MARKUP_TAG_GRADIENT || node.m_Type == MARKUP_TAG_WAVE || node.m_Type == MARKUP_TAG_SHAKE;
}

static bool IsStyleNode(const MarkupStyleNode& node)
{
    return node.m_Type == MARKUP_TAG_COLOR || node.m_Type == MARKUP_TAG_SIZE ||
           node.m_Type == MARKUP_TAG_OUTLINE || node.m_Type == MARKUP_TAG_SHADOW;
}

static void LogInvalidNode(HMarkup markup, const MarkupStyleNode& node)
{
    const char* source = MarkupGetSource(markup);
    dmLogWarning("Ignoring invalid rich-text tag <%.*s> at source byte %u", node.m_Tag.m_Length, source + node.m_Tag.m_Offset, node.m_Tag.m_Offset - 1);
}

static bool IsDecorationNode(const MarkupStyleNode& node)
{
    return node.m_Type == MARKUP_TAG_UNDERLINE || node.m_Type == MARKUP_TAG_STRIKE;
}

static bool ParseDecorationNode(HMarkup markup, const MarkupStyleNode& node, uint8_t* pattern)
{
    if (!IsDecorationNode(node) || node.m_AttributeCount > 1)
    {
        return false;
    }

    *pattern = TEXT_DECORATION_PATTERN_SOLID;

    if (node.m_AttributeCount == 0)
    {
        return true;
    }

    const MarkupAttribute* attribute = FindAttribute(markup, node, MARKUP_ATTRIBUTE_PATTERN);

    if (!attribute)
    {
        return false;
    }

    if (attribute->m_Constant == MARKUP_CONSTANT_SOLID)
    {
        return true;
    }

    if (attribute->m_Constant == MARKUP_CONSTANT_DASHED)
    {
        *pattern = TEXT_DECORATION_PATTERN_DASHED;

        return true;
    }

    return false;
}

bool TextLayoutCompileStyleFragment(const char* definition, uint32_t definition_length, TextRenderStyle* style, dmArray<TextEffect>* effects, MarkupError* error)
{
    HMarkup markup = 0;

    if (MarkupCreateStyleFragment(definition, definition_length, &markup, error) != MARKUP_RESULT_OK)
    {
        return false;
    }

    memset(style, 0, sizeof(*style));
    style->m_FaceColor[0] = style->m_FaceColor[1] = style->m_FaceColor[2] = style->m_FaceColor[3] = 1.0f;
    style->m_OutlineColor[0] = style->m_OutlineColor[1] = style->m_OutlineColor[2] = style->m_OutlineColor[3] = 1.0f;
    style->m_ShadowColor[0] = style->m_ShadowColor[1] = style->m_ShadowColor[2] = style->m_ShadowColor[3] = 1.0f;

    bool                   valid = true;
    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const uint32_t         node_count = MarkupGetStyleNodeCount(markup);

    for (uint32_t i = 1; i < node_count; ++i)
    {
        TextEffect effect = {};

        if (IsEffectNode(nodes[i]))
        {
            if (!CreateEffect(markup, nodes[i], 0, 1, &effect))
            {
                if (error)
                {
                    error->m_ByteOffset = nodes[i].m_Tag.m_Offset - 1;
                    error->m_Type = MARKUP_ERROR_INVALID_TAG;
                }

                valid = false;
                break;
            }

            EnsurePushCapacity(*effects);
            effects->Push(effect);
        }
        else if (IsStyleNode(nodes[i]))
        {
            if (nodes[i].m_Type == MARKUP_TAG_SIZE || !ApplyStyleNode(markup, nodes[i], 1.0f, style))
            {
                if (error)
                {
                    error->m_ByteOffset = nodes[i].m_Tag.m_Offset - 1;
                    error->m_Type = MARKUP_ERROR_INVALID_TAG;
                }

                valid = false;
                break;
            }
        }
        else
        {
            if (error)
            {
                error->m_ByteOffset = nodes[i].m_Tag.m_Offset - 1;
                error->m_Type = MARKUP_ERROR_INVALID_TAG;
            }

            valid = false;
            break;
        }
    }

    MarkupDestroy(markup);

    return valid;
}

bool TextLayoutResolveMarkup(HMarkup markup, TextLayoutSettings* settings, ResolvedMarkup* resolved)
{
    const float            base_font_size = settings->m_Size;
    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const MarkupSpan*      spans = MarkupGetSpans(markup);
    uint32_t               node_count = MarkupGetStyleNodeCount(markup);
    uint32_t               span_count = MarkupGetSpanCount(markup);

    if (span_count > MARKUP_INVALID_INDEX)
    {
        return false;
    }

    resolved->m_Effects.SetCapacity(node_count);
    resolved->m_SpanEffects.SetCapacity(span_count);
    resolved->m_Spans.SetCapacity(span_count);

    dmArray<uint16_t> node_effects;
    dmArray<uint8_t>  node_invalid;
    node_effects.SetCapacity(node_count);
    node_effects.SetSize(node_count);
    node_invalid.SetCapacity(node_count);
    node_invalid.SetSize(node_count);

    for (uint32_t i = 0; i < node_count; ++i)
    {
        node_effects[i] = MARKUP_INVALID_INDEX;
        node_invalid[i] = 0;
    }

    for (uint32_t i = 1; i < node_count; ++i)
    {
        if (nodes[i].m_TextLength == 0)
        {
            continue;
        }

        const uint16_t parent = nodes[i].m_Parent;

        if (parent != MARKUP_INVALID_INDEX && node_invalid[parent])
        {
            node_invalid[i] = 1;
            continue;
        }

        if (IsEffectNode(nodes[i]))
        {
            TextEffect effect = {};

            if (!CreateEffect(markup, nodes[i], nodes[i].m_TextOffset, nodes[i].m_TextLength, &effect))
            {
                node_invalid[i] = 1;
                LogInvalidNode(markup, nodes[i]);
                continue;
            }

            if (resolved->m_Effects.Size() == MARKUP_INVALID_INDEX)
            {
                return false;
            }

            EnsurePushCapacity(resolved->m_Effects);
            resolved->m_Effects.Push(effect);
            node_effects[i] = (uint16_t)(resolved->m_Effects.Size() - 1);
        }
        else if (IsStyleNode(nodes[i]))
        {
            TextRenderStyle style = {};

            if (!ApplyStyleNode(markup, nodes[i], base_font_size, &style))
            {
                node_invalid[i] = 1;
                LogInvalidNode(markup, nodes[i]);
            }
        }
        else if (IsDecorationNode(nodes[i]))
        {
            uint8_t pattern;

            if (!ParseDecorationNode(markup, nodes[i], &pattern))
            {
                node_invalid[i] = 1;
                LogInvalidNode(markup, nodes[i]);
            }
        }
    }

    dmArray<uint16_t> chain;
    chain.SetCapacity(node_count);

    for (uint32_t i = 0; i < span_count; ++i)
    {
        chain.SetSize(0);
        uint16_t node_index = spans[i].m_StyleNodeIndex;

        while (node_index != 0 && node_index != MARKUP_INVALID_INDEX)
        {
            chain.Push(node_index);
            node_index = nodes[node_index].m_Parent;
        }

        TextRenderStyle style = {};
        style.m_FaceColor[0] = style.m_FaceColor[1] = style.m_FaceColor[2] = style.m_FaceColor[3] = 1.0f;
        style.m_OutlineColor[0] = style.m_OutlineColor[1] = style.m_OutlineColor[2] = style.m_OutlineColor[3] = 1.0f;
        style.m_ShadowColor[0] = style.m_ShadowColor[1] = style.m_ShadowColor[2] = style.m_ShadowColor[3] = 1.0f;
        style.m_FontSize = base_font_size;
        uint16_t effect_index = MARKUP_INVALID_INDEX;
        uint16_t effect_count = 0;
        uint8_t  decoration_flags = 0;
        uint8_t  underline_pattern = TEXT_DECORATION_PATTERN_SOLID;
        uint8_t  strike_pattern = TEXT_DECORATION_PATTERN_SOLID;

        for (uint32_t j = chain.Size(); j > 0; --j)
        {
            uint16_t current = chain[j - 1];

            if (node_invalid[current])
            {
                break;
            }

            ApplyStyleNode(markup, nodes[current], base_font_size, &style);

            if (IsDecorationNode(nodes[current]))
            {
                uint8_t pattern;
                ParseDecorationNode(markup, nodes[current], &pattern);

                if (nodes[current].m_Type == MARKUP_TAG_UNDERLINE)
                {
                    decoration_flags |= TEXT_RESOLVED_DECORATION_UNDERLINE;
                    underline_pattern = pattern;
                }
                else
                {
                    decoration_flags |= TEXT_RESOLVED_DECORATION_STRIKE;
                    strike_pattern = pattern;
                }
            }

            if (node_effects[current] != MARKUP_INVALID_INDEX)
            {
                if (resolved->m_SpanEffects.Size() == MARKUP_INVALID_INDEX)
                {
                    return false;
                }

                if (effect_count == 0)
                {
                    effect_index = (uint16_t)resolved->m_SpanEffects.Size();
                }

                EnsurePushCapacity(resolved->m_SpanEffects);
                resolved->m_SpanEffects.Push(node_effects[current]);
                ++effect_count;
            }
        }

        uint16_t style_index;

        if (!AddStyle(resolved, style, &style_index))
        {
            return false;
        }

        TextResolvedSpan resolved_span = { spans[i].m_TextOffset, spans[i].m_TextLength, style_index, effect_index, effect_count,
                                           decoration_flags, underline_pattern, strike_pattern };
        EnsurePushCapacity(resolved->m_Spans);
        resolved->m_Spans.Push(resolved_span);
    }

    return ResolveObjects(markup, settings, resolved);
}
