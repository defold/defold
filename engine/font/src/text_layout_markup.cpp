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

#include "fontcollection.h"
#include "text_layout.h"

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/hashtable.h>
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

static bool AddStyle(ResolvedMarkup* resolved, dmHashTable64<uint16_t>* style_indices, const TextRenderStyle& style, uint16_t* style_index)
{
    const dmhash_t hash = dmHashBuffer64(&style, sizeof(style));
    uint16_t*      cached_index = style_indices->Get(hash);

    if (cached_index && StyleEquals(resolved->m_Styles[*cached_index], style))
    {
        *style_index = *cached_index;

        return true;
    }

    // Hash collisions are exceptionally rare, but retain exact style
    // equality semantics instead of treating the hash as an identity.
    if (cached_index)
    {
        for (uint32_t i = 0; i < resolved->m_Styles.Size(); ++i)
        {
            if (StyleEquals(resolved->m_Styles[i], style))
            {
                *style_index = (uint16_t)i;

                return true;
            }
        }
    }

    if (resolved->m_Styles.Size() == MARKUP_INVALID_INDEX)
    {
        return false;
    }

    EnsurePushCapacity(resolved->m_Styles);
    resolved->m_Styles.Push(style);
    *style_index = (uint16_t)(resolved->m_Styles.Size() - 1);

    if (!cached_index)
    {
        if (style_indices->Full())
        {
            const uint32_t capacity = style_indices->Capacity();
            style_indices->SetCapacity(capacity > MARKUP_INVALID_INDEX / 2 ? MARKUP_INVALID_INDEX : capacity * 2);
        }

        style_indices->Put(hash, *style_index);
    }

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
        const MarkupAttribute* alpha = FindAttribute(markup, node, MARKUP_ATTRIBUTE_ALPHA);

        if (!size && !color && !alpha)
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

        if (alpha && (!ParseFloat(source, alpha->m_Value, &style->m_OutlineAlpha) || style->m_OutlineAlpha < 0.0f))
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

        if (alpha)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_OUTLINE_ALPHA;
        }
    }
    else if (node.m_Type == MARKUP_TAG_SHADOW)
    {
        const MarkupAttribute* color = FindAttribute(markup, node, MARKUP_ATTRIBUTE_COLOR);
        const MarkupAttribute* x = FindAttribute(markup, node, MARKUP_ATTRIBUTE_X);
        const MarkupAttribute* y = FindAttribute(markup, node, MARKUP_ATTRIBUTE_Y);
        const MarkupAttribute* blur = FindAttribute(markup, node, MARKUP_ATTRIBUTE_BLUR);
        const MarkupAttribute* alpha = FindAttribute(markup, node, MARKUP_ATTRIBUTE_ALPHA);

        if (!color && !x && !y && !blur && !alpha)
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

        if (alpha && (!ParseFloat(source, alpha->m_Value, &style->m_ShadowAlpha) || style->m_ShadowAlpha < 0.0f))
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

        if (alpha)
        {
            style->m_Flags |= TEXT_RENDER_STYLE_SHADOW_ALPHA;
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

bool TextLayoutCompileStyleFragment(const char* definition, uint32_t definition_length, TextRenderStyle* style, dmArray<TextEffect>* effects, TextNamedStyleDecoration* decoration, MarkupError* error)
{
    if (decoration)
    {
        memset(decoration, 0, sizeof(*decoration));
    }

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
        else if (decoration && IsDecorationNode(nodes[i]))
        {
            uint8_t pattern;
            if (!ParseDecorationNode(markup, nodes[i], &pattern))
            {
                if (error)
                {
                    error->m_ByteOffset = nodes[i].m_Tag.m_Offset - 1;
                    error->m_Type = MARKUP_ERROR_INVALID_TAG;
                }
                valid = false;
                break;
            }
            if (nodes[i].m_Type == MARKUP_TAG_UNDERLINE)
            {
                decoration->m_Flags |= TEXT_RESOLVED_DECORATION_UNDERLINE;
                decoration->m_UnderlinePattern = pattern;
            }
            else
            {
                decoration->m_Flags |= TEXT_RESOLVED_DECORATION_STRIKE;
                decoration->m_StrikePattern = pattern;
            }
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

// Markup nodes are stored parent-first, so each effective state can be copied
// from its already-resolved parent instead of replaying the full chain per span.
struct ResolvedMarkupNodeState
{
    TextRenderStyle m_Style;
    uint16_t        m_EffectNode;
    uint16_t        m_EffectCount;
    uint8_t         m_DecorationFlags;
    uint8_t         m_InlineDecorationFlags;
    uint8_t         m_UnderlinePattern;
    uint8_t         m_StrikePattern;
    uint8_t         m_HasObjectStyle;
    uint8_t         m_Invalid;
};

static bool SetSpanEffectSize(dmArray<uint16_t>* effects, uint32_t size)
{
    if (size > MARKUP_INVALID_INDEX)
    {
        return false;
    }

    if (effects->Capacity() < size)
    {
        uint32_t capacity = effects->Capacity() > 8 ? effects->Capacity() : 8;

        while (capacity < size)
        {
            capacity = capacity > MARKUP_INVALID_INDEX / 2 ? MARKUP_INVALID_INDEX : capacity * 2;
        }

        effects->SetCapacity(capacity);
    }

    effects->SetSize(size);

    return true;
}

bool TextLayoutResolveMarkup(HFontCollection collection, HMarkup markup, TextLayoutSettings* settings, ResolvedMarkup* resolved)
{
    const float            base_font_size = settings->m_Size;
    const MarkupStyleNode* nodes = MarkupGetStyleNodes(markup);
    const MarkupSpan*      spans = MarkupGetSpans(markup);
    uint32_t               node_count = MarkupGetStyleNodeCount(markup);
    uint32_t               span_count = MarkupGetSpanCount(markup);

    if (node_count == 0 || node_count > MARKUP_INVALID_INDEX || span_count > MARKUP_INVALID_INDEX)
    {
        return false;
    }

    resolved->m_Effects.SetCapacity(node_count);
    resolved->m_SpanEffects.SetCapacity(span_count);
    resolved->m_Spans.SetCapacity(span_count);

    dmArray<ResolvedMarkupNodeState> node_states;
    dmArray<uint16_t>                node_effects;
    dmArray<uint16_t>                node_effect_parents;
    node_states.SetCapacity(node_count);
    node_states.SetSize(node_count);
    node_effects.SetCapacity(node_count);
    node_effects.SetSize(node_count);
    node_effect_parents.SetCapacity(node_count);
    node_effect_parents.SetSize(node_count);

    for (uint32_t i = 0; i < node_count; ++i)
    {
        node_effects[i] = MARKUP_INVALID_INDEX;
        node_effect_parents[i] = MARKUP_INVALID_INDEX;
    }

    ResolvedMarkupNodeState root = {};
    root.m_Style.m_FaceColor[0] = root.m_Style.m_FaceColor[1] = root.m_Style.m_FaceColor[2] = root.m_Style.m_FaceColor[3] = 1.0f;
    root.m_Style.m_OutlineColor[0] = root.m_Style.m_OutlineColor[1] = root.m_Style.m_OutlineColor[2] = root.m_Style.m_OutlineColor[3] = 1.0f;
    root.m_Style.m_ShadowColor[0] = root.m_Style.m_ShadowColor[1] = root.m_Style.m_ShadowColor[2] = root.m_Style.m_ShadowColor[3] = 1.0f;
    root.m_Style.m_FontSize = base_font_size;
    root.m_EffectNode = MARKUP_INVALID_INDEX;
    root.m_UnderlinePattern = TEXT_DECORATION_PATTERN_SOLID;
    root.m_StrikePattern = TEXT_DECORATION_PATTERN_SOLID;
    const TextNamedStyleDecoration* base_decoration = settings->m_UseBaseStyle ? FontCollectionGetNamedStyleDecoration(collection, settings->m_BaseStyle) : 0;
    if (base_decoration)
    {
        root.m_DecorationFlags = base_decoration->m_Flags;
        root.m_UnderlinePattern = base_decoration->m_UnderlinePattern;
        root.m_StrikePattern = base_decoration->m_StrikePattern;
    }
    node_states[0] = root;

    for (uint32_t i = 1; i < node_count; ++i)
    {
        const uint16_t parent = nodes[i].m_Parent;
        ResolvedMarkupNodeState state = parent == MARKUP_INVALID_INDEX ? root : node_states[parent];
        node_effect_parents[i] = state.m_EffectNode;

        if (nodes[i].m_TextLength == 0)
        {
            node_states[i] = state;
            continue;
        }

        if (state.m_Invalid)
        {
            node_states[i] = state;
            continue;
        }

        if (IsEffectNode(nodes[i]))
        {
            TextEffect effect = {};

            if (!CreateEffect(markup, nodes[i], nodes[i].m_TextOffset, nodes[i].m_TextLength, &effect))
            {
                state.m_Invalid = 1;
                LogInvalidNode(markup, nodes[i]);
                node_states[i] = state;
                continue;
            }

            if (resolved->m_Effects.Size() == MARKUP_INVALID_INDEX)
            {
                return false;
            }

            EnsurePushCapacity(resolved->m_Effects);
            resolved->m_Effects.Push(effect);
            node_effects[i] = (uint16_t)(resolved->m_Effects.Size() - 1);
            state.m_EffectNode = (uint16_t)i;
            ++state.m_EffectCount;
        }
        else if (IsStyleNode(nodes[i]))
        {
            TextRenderStyle style = state.m_Style;

            if (!ApplyStyleNode(markup, nodes[i], base_font_size, &style))
            {
                state.m_Invalid = 1;
                LogInvalidNode(markup, nodes[i]);
            }
            else
            {
                state.m_Style = style;
            }
        }
        else if (IsDecorationNode(nodes[i]))
        {
            uint8_t pattern;

            if (!ParseDecorationNode(markup, nodes[i], &pattern))
            {
                state.m_Invalid = 1;
                LogInvalidNode(markup, nodes[i]);
            }
            else if (nodes[i].m_Type == MARKUP_TAG_UNDERLINE)
            {
                state.m_DecorationFlags |= TEXT_RESOLVED_DECORATION_UNDERLINE;
                state.m_InlineDecorationFlags |= TEXT_RESOLVED_DECORATION_UNDERLINE;
                state.m_UnderlinePattern = pattern;
            }
            else
            {
                state.m_DecorationFlags |= TEXT_RESOLVED_DECORATION_STRIKE;
                state.m_InlineDecorationFlags |= TEXT_RESOLVED_DECORATION_STRIKE;
                state.m_StrikePattern = pattern;
            }
        }

        if (!state.m_Invalid)
        {
            state.m_HasObjectStyle |= GetObjectTag(nodes[i]) != 0;
        }

        node_states[i] = state;
    }

    dmHashTable64<uint16_t> style_indices;

    if (span_count > 0)
    {
        style_indices.SetCapacity(span_count < 32 ? span_count : 32);
    }

    for (uint32_t i = 0; i < span_count; ++i)
    {
        const uint16_t node_index = spans[i].m_StyleNodeIndex;
        const ResolvedMarkupNodeState& state = node_index == MARKUP_INVALID_INDEX ? root : node_states[node_index];
        uint16_t effect_index = MARKUP_INVALID_INDEX;

        if (state.m_EffectCount > 0)
        {
            const uint32_t span_effect_size = resolved->m_SpanEffects.Size();
            const uint32_t new_span_effect_size = span_effect_size + state.m_EffectCount;

            if (!SetSpanEffectSize(&resolved->m_SpanEffects, new_span_effect_size))
            {
                return false;
            }

            effect_index = (uint16_t)span_effect_size;
            uint16_t effect_node = state.m_EffectNode;

            for (uint32_t j = state.m_EffectCount; j > 0; --j)
            {
                resolved->m_SpanEffects[effect_index + j - 1] = node_effects[effect_node];
                effect_node = node_effect_parents[effect_node];
            }
        }

        uint16_t style_index;

        if (!AddStyle(resolved, &style_indices, state.m_Style, &style_index))
        {
            return false;
        }

        TextResolvedSpan resolved_span = { spans[i].m_TextOffset, spans[i].m_TextLength, style_index, effect_index, state.m_EffectCount,
                                           state.m_DecorationFlags, state.m_InlineDecorationFlags, state.m_UnderlinePattern, state.m_StrikePattern, state.m_HasObjectStyle };
        EnsurePushCapacity(resolved->m_Spans);
        resolved->m_Spans.Push(resolved_span);
    }

    return ResolveObjects(markup, settings, resolved);
}
