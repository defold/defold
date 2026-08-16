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

#include "markup.h"

#include <string.h>

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>

struct Markup
{
    dmArray<char>            m_Source;
    dmArray<uint32_t>        m_Text;
    dmArray<MarkupSpan>      m_Spans;
    dmArray<MarkupStyleNode> m_StyleNodes;
    dmArray<MarkupAttribute> m_Attributes;
};

struct ParseContext
{
    Markup*     m_Markup;
    const char* m_Source;
    uint32_t    m_Length;
    uint32_t    m_Cursor;
    uint16_t    m_StyleNodeIndex;
};

template <typename T>
static void EnsurePushCapacity(dmArray<T>& array)
{
    if (array.Full())
    {
        array.SetCapacity(array.Capacity() ? array.Capacity() * 2 : 8);
    }
}

static void SetError(MarkupError* error, MarkupErrorType type, uint32_t byte_offset)
{
    if (error)
    {
        error->m_Type = type;
        error->m_ByteOffset = byte_offset;
    }
}

static void SetFirstError(MarkupError* error, const MarkupError& first)
{
    if (error && error->m_Type == MARKUP_ERROR_NONE)
    {
        *error = first;
    }
}

static bool IsNameStart(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static bool IsNameCharacter(char c)
{
    return IsNameStart(c) || (c >= '0' && c <= '9') || c == '-' || c == ':';
}

static bool IsWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void SkipWhitespace(ParseContext* context)
{
    while (context->m_Cursor < context->m_Length && IsWhitespace(context->m_Source[context->m_Cursor]))
    {
        ++context->m_Cursor;
    }
}

static MarkupString ParseName(ParseContext* context, bool* limit_exceeded)
{
    MarkupString name = { context->m_Cursor, 0 };
    *limit_exceeded = false;

    if (context->m_Cursor >= context->m_Length || !IsNameStart(context->m_Source[context->m_Cursor]))
    {
        return name;
    }

    ++context->m_Cursor;

    while (context->m_Cursor < context->m_Length && IsNameCharacter(context->m_Source[context->m_Cursor]))
    {
        ++context->m_Cursor;
    }

    uint32_t length = context->m_Cursor - name.m_Offset;

    if (length > UINT16_MAX)
    {
        *limit_exceeded = true;

        return name;
    }

    name.m_Length = (uint16_t)length;

    return name;
}

static bool StringEquals(const char* source, MarkupString a, MarkupString b)
{
    return a.m_Length == b.m_Length && memcmp(source + a.m_Offset, source + b.m_Offset, a.m_Length) == 0;
}

static dmhash_t HashString(const char* source, MarkupString string)
{
    return dmHashBuffer64(source + string.m_Offset, string.m_Length);
}

static const dmhash_t TAG_COLOR    = dmHashString64("color");
static const dmhash_t TAG_GRADIENT = dmHashString64("gradient");
static const dmhash_t TAG_LINK     = dmHashString64("link");
static const dmhash_t TAG_OUTLINE  = dmHashString64("outline");
static const dmhash_t TAG_SHADOW   = dmHashString64("shadow");
static const dmhash_t TAG_SHAKE    = dmHashString64("shake");
static const dmhash_t TAG_SIZE     = dmHashString64("size");
static const dmhash_t TAG_SPRITE   = dmHashString64("sprite");
static const dmhash_t TAG_STRIKE   = dmHashString64("strike");
static const dmhash_t TAG_UL       = dmHashString64("ul");
static const dmhash_t TAG_WAVE     = dmHashString64("wave");

static const dmhash_t ATTRIBUTE_AMPLITUDE  = dmHashString64("amplitude");
static const dmhash_t ATTRIBUTE_BL         = dmHashString64("bl");
static const dmhash_t ATTRIBUTE_BLUR       = dmHashString64("blur");
static const dmhash_t ATTRIBUTE_BOTTOM     = dmHashString64("bottom");
static const dmhash_t ATTRIBUTE_BR         = dmHashString64("br");
static const dmhash_t ATTRIBUTE_COLOR      = dmHashString64("color");
static const dmhash_t ATTRIBUTE_DIRECTION  = dmHashString64("direction");
static const dmhash_t ATTRIBUTE_FIT        = dmHashString64("fit");
static const dmhash_t ATTRIBUTE_HEIGHT     = dmHashString64("height");
static const dmhash_t ATTRIBUTE_HZ         = dmHashString64("hz");
static const dmhash_t ATTRIBUTE_ID         = dmHashString64("id");
static const dmhash_t ATTRIBUTE_LEFT       = dmHashString64("left");
static const dmhash_t ATTRIBUTE_PATTERN    = dmHashString64("pattern");
static const dmhash_t ATTRIBUTE_RIGHT      = dmHashString64("right");
static const dmhash_t ATTRIBUTE_SIZE       = dmHashString64("size");
static const dmhash_t ATTRIBUTE_TL         = dmHashString64("tl");
static const dmhash_t ATTRIBUTE_TOP        = dmHashString64("top");
static const dmhash_t ATTRIBUTE_TR         = dmHashString64("tr");
static const dmhash_t ATTRIBUTE_VALUE      = dmHashString64("value");
static const dmhash_t ATTRIBUTE_WAVELENGTH = dmHashString64("wavelength");
static const dmhash_t ATTRIBUTE_WIDTH      = dmHashString64("width");
static const dmhash_t ATTRIBUTE_X          = dmHashString64("x");
static const dmhash_t ATTRIBUTE_Y          = dmHashString64("y");

static const dmhash_t VALUE_DASHED  = dmHashString64("dashed");
static const dmhash_t VALUE_FORWARD = dmHashString64("forward");
static const dmhash_t VALUE_GLYPH   = dmHashString64("glyph");
static const dmhash_t VALUE_REVERSE = dmHashString64("reverse");
static const dmhash_t VALUE_SOLID   = dmHashString64("solid");
static const dmhash_t VALUE_SPAN    = dmHashString64("span");

static MarkupTagType GetTagType(dmhash_t tag)
{
    if (tag == TAG_COLOR)
    {
        return MARKUP_TAG_COLOR;
    }

    if (tag == TAG_GRADIENT)
    {
        return MARKUP_TAG_GRADIENT;
    }

    if (tag == TAG_LINK)
    {
        return MARKUP_TAG_LINK;
    }

    if (tag == TAG_OUTLINE)
    {
        return MARKUP_TAG_OUTLINE;
    }

    if (tag == TAG_SHADOW)
    {
        return MARKUP_TAG_SHADOW;
    }

    if (tag == TAG_SHAKE)
    {
        return MARKUP_TAG_SHAKE;
    }

    if (tag == TAG_SIZE)
    {
        return MARKUP_TAG_SIZE;
    }

    if (tag == TAG_SPRITE)
    {
        return MARKUP_TAG_SPRITE;
    }

    if (tag == TAG_STRIKE)
    {
        return MARKUP_TAG_STRIKE;
    }

    if (tag == TAG_UL)
    {
        return MARKUP_TAG_UNDERLINE;
    }

    if (tag == TAG_WAVE)
    {
        return MARKUP_TAG_WAVE;
    }

    return MARKUP_TAG_ROOT;
}

static MarkupAttributeType GetAttributeType(dmhash_t name)
{
    if (name == 0)
    {
        return MARKUP_ATTRIBUTE_SHORTHAND;
    }

    if (name == ATTRIBUTE_AMPLITUDE)
    {
        return MARKUP_ATTRIBUTE_AMPLITUDE;
    }

    if (name == ATTRIBUTE_BL)
    {
        return MARKUP_ATTRIBUTE_BL;
    }

    if (name == ATTRIBUTE_BLUR)
    {
        return MARKUP_ATTRIBUTE_BLUR;
    }

    if (name == ATTRIBUTE_BOTTOM)
    {
        return MARKUP_ATTRIBUTE_BOTTOM;
    }

    if (name == ATTRIBUTE_BR)
    {
        return MARKUP_ATTRIBUTE_BR;
    }

    if (name == ATTRIBUTE_COLOR)
    {
        return MARKUP_ATTRIBUTE_COLOR;
    }

    if (name == ATTRIBUTE_DIRECTION)
    {
        return MARKUP_ATTRIBUTE_DIRECTION;
    }

    if (name == ATTRIBUTE_FIT)
    {
        return MARKUP_ATTRIBUTE_FIT;
    }

    if (name == ATTRIBUTE_HEIGHT)
    {
        return MARKUP_ATTRIBUTE_HEIGHT;
    }

    if (name == ATTRIBUTE_HZ)
    {
        return MARKUP_ATTRIBUTE_HZ;
    }

    if (name == ATTRIBUTE_ID)
    {
        return MARKUP_ATTRIBUTE_ID;
    }

    if (name == ATTRIBUTE_LEFT)
    {
        return MARKUP_ATTRIBUTE_LEFT;
    }

    if (name == ATTRIBUTE_PATTERN)
    {
        return MARKUP_ATTRIBUTE_PATTERN;
    }

    if (name == ATTRIBUTE_RIGHT)
    {
        return MARKUP_ATTRIBUTE_RIGHT;
    }

    if (name == ATTRIBUTE_SIZE)
    {
        return MARKUP_ATTRIBUTE_SIZE;
    }

    if (name == ATTRIBUTE_TL)
    {
        return MARKUP_ATTRIBUTE_TL;
    }

    if (name == ATTRIBUTE_TOP)
    {
        return MARKUP_ATTRIBUTE_TOP;
    }

    if (name == ATTRIBUTE_TR)
    {
        return MARKUP_ATTRIBUTE_TR;
    }

    if (name == ATTRIBUTE_VALUE)
    {
        return MARKUP_ATTRIBUTE_VALUE;
    }

    if (name == ATTRIBUTE_WAVELENGTH)
    {
        return MARKUP_ATTRIBUTE_WAVELENGTH;
    }

    if (name == ATTRIBUTE_WIDTH)
    {
        return MARKUP_ATTRIBUTE_WIDTH;
    }

    if (name == ATTRIBUTE_X)
    {
        return MARKUP_ATTRIBUTE_X;
    }

    if (name == ATTRIBUTE_Y)
    {
        return MARKUP_ATTRIBUTE_Y;
    }

    return MARKUP_ATTRIBUTE_CUSTOM;
}

static bool IsSupportedAttribute(MarkupTagType tag, MarkupAttributeType name)
{
    // Object attributes are application-defined and forwarded verbatim to the resolver.

    if (tag == MARKUP_TAG_LINK || tag == MARKUP_TAG_SPRITE)
    {
        return name != MARKUP_ATTRIBUTE_SHORTHAND;
    }

    if (name == MARKUP_ATTRIBUTE_SHORTHAND)
    {
        return tag == MARKUP_TAG_COLOR || tag == MARKUP_TAG_SIZE;
    }

    if (tag == MARKUP_TAG_COLOR || tag == MARKUP_TAG_SIZE)
    {
        return name == MARKUP_ATTRIBUTE_VALUE;
    }

    if (tag == MARKUP_TAG_OUTLINE)
    {
        return name == MARKUP_ATTRIBUTE_COLOR || name == MARKUP_ATTRIBUTE_SIZE;
    }

    if (tag == MARKUP_TAG_SHADOW)
    {
        return name == MARKUP_ATTRIBUTE_BLUR || name == MARKUP_ATTRIBUTE_COLOR ||
               name == MARKUP_ATTRIBUTE_X || name == MARKUP_ATTRIBUTE_Y;
    }

    if (tag == MARKUP_TAG_GRADIENT)
    {
        return name == MARKUP_ATTRIBUTE_BL || name == MARKUP_ATTRIBUTE_BOTTOM || name == MARKUP_ATTRIBUTE_BR ||
               name == MARKUP_ATTRIBUTE_DIRECTION || name == MARKUP_ATTRIBUTE_FIT || name == MARKUP_ATTRIBUTE_HZ ||
               name == MARKUP_ATTRIBUTE_LEFT || name == MARKUP_ATTRIBUTE_RIGHT || name == MARKUP_ATTRIBUTE_TL ||
               name == MARKUP_ATTRIBUTE_TOP || name == MARKUP_ATTRIBUTE_TR;
    }

    if (tag == MARKUP_TAG_WAVE)
    {
        return name == MARKUP_ATTRIBUTE_AMPLITUDE || name == MARKUP_ATTRIBUTE_DIRECTION || name == MARKUP_ATTRIBUTE_FIT ||
               name == MARKUP_ATTRIBUTE_HZ || name == MARKUP_ATTRIBUTE_WAVELENGTH;
    }

    if (tag == MARKUP_TAG_SHAKE)
    {
        return name == MARKUP_ATTRIBUTE_AMPLITUDE || name == MARKUP_ATTRIBUTE_FIT || name == MARKUP_ATTRIBUTE_HZ;
    }

    if (tag == MARKUP_TAG_UNDERLINE || tag == MARKUP_TAG_STRIKE)
    {
        return name == MARKUP_ATTRIBUTE_PATTERN;
    }

    return false;
}

static bool GetConstantType(MarkupTagType tag, MarkupAttributeType name, dmhash_t value, MarkupConstantType* type)
{
    *type = MARKUP_CONSTANT_NONE;

    if (name == MARKUP_ATTRIBUTE_FIT)
    {
        if (value == VALUE_GLYPH) *type = MARKUP_CONSTANT_GLYPH;
        if (value == VALUE_SPAN)  *type = MARKUP_CONSTANT_SPAN;

        return *type != MARKUP_CONSTANT_NONE;
    }

    if (name == MARKUP_ATTRIBUTE_DIRECTION)
    {
        if (value == VALUE_FORWARD) *type = MARKUP_CONSTANT_FORWARD;
        if (value == VALUE_REVERSE) *type = MARKUP_CONSTANT_REVERSE;

        return *type != MARKUP_CONSTANT_NONE;
    }

    if ((tag == MARKUP_TAG_UNDERLINE || tag == MARKUP_TAG_STRIKE) && name == MARKUP_ATTRIBUTE_PATTERN)
    {
        if (value == VALUE_SOLID)  *type = MARKUP_CONSTANT_SOLID;
        if (value == VALUE_DASHED) *type = MARKUP_CONSTANT_DASHED;

        return *type != MARKUP_CONSTANT_NONE;
    }

    return true;
}

static MarkupResult ParseValue(ParseContext* context, MarkupString* value, MarkupError* error)
{
    value->m_Offset = context->m_Cursor;
    value->m_Length = 0;

    if (context->m_Cursor >= context->m_Length)
    {
        SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, context->m_Cursor);

        return MARKUP_RESULT_INCOMPLETE;
    }

    const char quote = context->m_Source[context->m_Cursor];

    if (quote == '\'' || quote == '"')
    {
        ++context->m_Cursor;
        value->m_Offset = context->m_Cursor;

        while (context->m_Cursor < context->m_Length && context->m_Source[context->m_Cursor] != quote)
        {
            ++context->m_Cursor;
        }

        if (context->m_Cursor == context->m_Length)
        {
            SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, value->m_Offset - 1);

            return MARKUP_RESULT_INCOMPLETE;
        }

        uint32_t length = context->m_Cursor - value->m_Offset;

        if (length > UINT16_MAX)
        {
            SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, value->m_Offset);

            return MARKUP_RESULT_LIMIT_EXCEEDED;
        }

        value->m_Length = (uint16_t)length;
        ++context->m_Cursor;

        return MARKUP_RESULT_OK;
    }

    while (context->m_Cursor < context->m_Length)
    {
        char c = context->m_Source[context->m_Cursor];

        if (IsWhitespace(c) || c == '>')
        {
            break;
        }

        if (c == '/' && context->m_Cursor + 1 < context->m_Length && context->m_Source[context->m_Cursor + 1] == '>')
        {
            break;
        }

        if (c == '<' || c == '=' || c == '\'' || c == '"')
        {
            SetError(error, MARKUP_ERROR_INVALID_ATTRIBUTE, context->m_Cursor);

            return MARKUP_RESULT_SYNTAX_ERROR;
        }

        ++context->m_Cursor;
    }

    uint32_t length = context->m_Cursor - value->m_Offset;

    if (length == 0)
    {
        SetError(error, MARKUP_ERROR_INVALID_ATTRIBUTE, value->m_Offset);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    if (length > UINT16_MAX)
    {
        SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, value->m_Offset);

        return MARKUP_RESULT_LIMIT_EXCEEDED;
    }

    value->m_Length = (uint16_t)length;

    return MARKUP_RESULT_OK;
}

static MarkupResult PushAttribute(ParseContext* context, MarkupString name, MarkupString value,
                                  MarkupAttributeType type, MarkupConstantType constant, MarkupError* error)
{
    if (context->m_Markup->m_Attributes.Size() == UINT16_MAX)
    {
        SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, name.m_Offset);

        return MARKUP_RESULT_LIMIT_EXCEEDED;
    }

    MarkupAttribute attribute = { name, value, (uint8_t)type, (uint8_t)constant };
    EnsurePushCapacity(context->m_Markup->m_Attributes);
    context->m_Markup->m_Attributes.Push(attribute);

    return MARKUP_RESULT_OK;
}

static MarkupResult ParseOpeningTag(ParseContext* context, MarkupString tag, MarkupTagType tag_type, MarkupError* error, bool* self_closing)
{
    *self_closing = false;

    if (context->m_Markup->m_StyleNodes.Size() == UINT16_MAX)
    {
        SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, tag.m_Offset);

        return MARKUP_RESULT_LIMIT_EXCEEDED;
    }

    const uint32_t attribute_index = context->m_Markup->m_Attributes.Size();
    bool           shorthand_seen = false;

    while (true)
    {
        SkipWhitespace(context);

        if (context->m_Cursor == context->m_Length)
        {
            SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, tag.m_Offset - 1);

            return MARKUP_RESULT_INCOMPLETE;
        }

        char c = context->m_Source[context->m_Cursor];

        if (c == '>')
        {
            ++context->m_Cursor;
            break;
        }

        if (c == '/')
        {
            if (context->m_Cursor + 1 == context->m_Length)
            {
                SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, context->m_Cursor);

                return MARKUP_RESULT_INCOMPLETE;
            }

            if (context->m_Source[context->m_Cursor + 1] != '>')
            {
                SetError(error, MARKUP_ERROR_INVALID_TAG, context->m_Cursor);

                return MARKUP_RESULT_SYNTAX_ERROR;
            }

            context->m_Cursor += 2;
            *self_closing = true;
            break;
        }

        MarkupString name = { context->m_Cursor, 0 };

        if (c != '=')
        {
            bool limit_exceeded;
            name = ParseName(context, &limit_exceeded);

            if (limit_exceeded)
            {
                SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, name.m_Offset);

                return MARKUP_RESULT_LIMIT_EXCEEDED;
            }

            if (name.m_Length == 0)
            {
                SetError(error, MARKUP_ERROR_INVALID_ATTRIBUTE, context->m_Cursor);

                return MARKUP_RESULT_SYNTAX_ERROR;
            }
        }
        else if (shorthand_seen || context->m_Markup->m_Attributes.Size() != attribute_index)
        {
            SetError(error, MARKUP_ERROR_INVALID_ATTRIBUTE, context->m_Cursor);

            return MARKUP_RESULT_SYNTAX_ERROR;
        }

        if (context->m_Cursor >= context->m_Length)
        {
            SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, context->m_Cursor);

            return MARKUP_RESULT_INCOMPLETE;
        }

        if (context->m_Source[context->m_Cursor] != '=')
        {
            SetError(error, MARKUP_ERROR_INVALID_ATTRIBUTE, context->m_Cursor);

            return MARKUP_RESULT_SYNTAX_ERROR;
        }

        const dmhash_t            name_hash = name.m_Length == 0 ? 0 : HashString(context->m_Source, name);
        const MarkupAttributeType attribute_type = GetAttributeType(name_hash);

        if (!IsSupportedAttribute(tag_type, attribute_type))
        {
            SetError(error, MARKUP_ERROR_UNKNOWN_ATTRIBUTE, name.m_Offset);

            return MARKUP_RESULT_SYNTAX_ERROR;
        }

        if (name.m_Length == 0)
        {
            shorthand_seen = true;
        }

        ++context->m_Cursor;

        MarkupString value;
        MarkupResult result = ParseValue(context, &value, error);

        if (result != MARKUP_RESULT_OK)
        {
            return result;
        }

        MarkupConstantType constant_type = MARKUP_CONSTANT_NONE;
        const bool         has_constant = attribute_type == MARKUP_ATTRIBUTE_FIT ||
                                          attribute_type == MARKUP_ATTRIBUTE_DIRECTION ||
                                          ((tag_type == MARKUP_TAG_UNDERLINE || tag_type == MARKUP_TAG_STRIKE) &&
                                           attribute_type == MARKUP_ATTRIBUTE_PATTERN);
        if (has_constant && !GetConstantType(tag_type, attribute_type, HashString(context->m_Source, value), &constant_type))
        {
            SetError(error, MARKUP_ERROR_INVALID_ATTRIBUTE_VALUE, value.m_Offset);

            return MARKUP_RESULT_SYNTAX_ERROR;
        }

        result = PushAttribute(context, name, value, attribute_type, constant_type, error);

        if (result != MARKUP_RESULT_OK)
        {
            return result;
        }
    }

    uint32_t attribute_count = context->m_Markup->m_Attributes.Size() - attribute_index;

    if (attribute_count > UINT16_MAX)
    {
        SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, tag.m_Offset);

        return MARKUP_RESULT_LIMIT_EXCEEDED;
    }

    MarkupStyleNode node = { tag, context->m_StyleNodeIndex, (uint16_t)attribute_index, (uint16_t)attribute_count,
                             (uint8_t)tag_type, context->m_Markup->m_Text.Size(), 0 };
    EnsurePushCapacity(context->m_Markup->m_StyleNodes);
    context->m_Markup->m_StyleNodes.Push(node);

    if (!*self_closing)
    {
        context->m_StyleNodeIndex = (uint16_t)(context->m_Markup->m_StyleNodes.Size() - 1);
    }

    return MARKUP_RESULT_OK;
}

static MarkupResult ParseTag(ParseContext* context, MarkupError* error, bool* self_closing)
{
    *self_closing = false;
    const uint32_t tag_start = context->m_Cursor;
    ++context->m_Cursor;

    if (context->m_Cursor == context->m_Length)
    {
        SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, tag_start);

        return MARKUP_RESULT_INCOMPLETE;
    }

    bool closing = context->m_Source[context->m_Cursor] == '/';

    if (closing)
    {
        ++context->m_Cursor;

        if (context->m_Cursor == context->m_Length)
        {
            SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, tag_start);

            return MARKUP_RESULT_INCOMPLETE;
        }
    }

    bool         limit_exceeded;
    MarkupString tag = ParseName(context, &limit_exceeded);

    if (limit_exceeded)
    {
        SetError(error, MARKUP_ERROR_LIMIT_EXCEEDED, tag.m_Offset);

        return MARKUP_RESULT_LIMIT_EXCEEDED;
    }

    if (tag.m_Length == 0)
    {
        SetError(error, MARKUP_ERROR_INVALID_TAG, tag_start);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    if (context->m_Cursor == context->m_Length)
    {
        SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, tag_start);

        return MARKUP_RESULT_INCOMPLETE;
    }

    const MarkupTagType tag_type = GetTagType(HashString(context->m_Source, tag));

    if (tag_type == MARKUP_TAG_ROOT)
    {
        SetError(error, MARKUP_ERROR_UNKNOWN_TAG, tag.m_Offset);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    if (!closing)
    {
        return ParseOpeningTag(context, tag, tag_type, error, self_closing);
    }

    SkipWhitespace(context);

    if (context->m_Cursor == context->m_Length)
    {
        SetError(error, MARKUP_ERROR_INCOMPLETE_TAG, tag_start);

        return MARKUP_RESULT_INCOMPLETE;
    }

    if (context->m_Source[context->m_Cursor] != '>')
    {
        SetError(error, MARKUP_ERROR_INVALID_TAG, context->m_Cursor);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    ++context->m_Cursor;

    if (context->m_StyleNodeIndex == 0)
    {
        SetError(error, MARKUP_ERROR_UNEXPECTED_CLOSING_TAG, tag_start);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    const MarkupStyleNode& current = context->m_Markup->m_StyleNodes[context->m_StyleNodeIndex];

    if (!StringEquals(context->m_Source, tag, current.m_Tag))
    {
        SetError(error, MARKUP_ERROR_MISMATCHED_CLOSING_TAG, tag_start);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    context->m_Markup->m_StyleNodes[context->m_StyleNodeIndex].m_TextLength =
    context->m_Markup->m_Text.Size() - current.m_TextOffset;
    context->m_StyleNodeIndex = current.m_Parent;

    return MARKUP_RESULT_OK;
}

static MarkupResult DecodeCodepoint(const char* source, uint32_t length, uint32_t* cursor, uint32_t* codepoint, MarkupError* error)
{
    const uint32_t start = *cursor;
    const uint8_t  first = (uint8_t)source[(*cursor)++];

    if (first < 0x80)
    {
        *codepoint = first;

        return MARKUP_RESULT_OK;
    }

    uint32_t value;
    uint32_t continuation_count;
    uint32_t minimum;

    if ((first & 0xe0) == 0xc0)
    {
        value = first & 0x1f;
        continuation_count = 1;
        minimum = 0x80;
    }
    else if ((first & 0xf0) == 0xe0)
    {
        value = first & 0x0f;
        continuation_count = 2;
        minimum = 0x800;
    }
    else if ((first & 0xf8) == 0xf0)
    {
        value = first & 0x07;
        continuation_count = 3;
        minimum = 0x10000;
    }
    else
    {
        SetError(error, MARKUP_ERROR_INVALID_UTF8, start);

        return MARKUP_RESULT_INVALID_UTF8;
    }

    if (continuation_count > length - *cursor)
    {
        SetError(error, MARKUP_ERROR_INVALID_UTF8, start);

        return MARKUP_RESULT_INCOMPLETE;
    }

    for (uint32_t i = 0; i < continuation_count; ++i)
    {
        uint8_t continuation = (uint8_t)source[(*cursor)++];

        if ((continuation & 0xc0) != 0x80)
        {
            SetError(error, MARKUP_ERROR_INVALID_UTF8, start);

            return MARKUP_RESULT_INVALID_UTF8;
        }

        value = (value << 6) | (continuation & 0x3f);
    }

    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
    {
        SetError(error, MARKUP_ERROR_INVALID_UTF8, start);

        return MARKUP_RESULT_INVALID_UTF8;
    }

    *codepoint = value;

    return MARKUP_RESULT_OK;
}

struct Entity
{
    const char* m_Name;
    uint8_t     m_Length;
    uint32_t    m_Codepoint;
};

static MarkupResult ParseEntity(ParseContext* context, uint32_t* codepoint, MarkupError* error)
{
    static const Entity entities[] = {
        { "amp", 3, '&' },
        { "apos", 4, '\'' },
        { "gt", 2, '>' },
        { "lt", 2, '<' },
        { "quot", 4, '"' },
    };

    const uint32_t start = context->m_Cursor++;
    uint32_t       end = context->m_Cursor;

    while (end < context->m_Length && context->m_Source[end] != ';' && end - context->m_Cursor <= 5)
    {
        ++end;
    }

    if (end == context->m_Length)
    {
        SetError(error, MARKUP_ERROR_INCOMPLETE_ENTITY, start);

        return MARKUP_RESULT_INCOMPLETE;
    }

    if (context->m_Source[end] != ';')
    {
        SetError(error, MARKUP_ERROR_INVALID_ENTITY, start);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    uint32_t name_length = end - context->m_Cursor;

    for (uint32_t i = 0; i < sizeof(entities) / sizeof(entities[0]); ++i)
    {
        if (name_length == entities[i].m_Length &&
            memcmp(context->m_Source + context->m_Cursor, entities[i].m_Name, name_length) == 0)
        {
            *codepoint = entities[i].m_Codepoint;
            context->m_Cursor = end + 1;

            return MARKUP_RESULT_OK;
        }
    }

    SetError(error, MARKUP_ERROR_INVALID_ENTITY, start);

    return MARKUP_RESULT_SYNTAX_ERROR;
}

static void PushText(ParseContext* context, uint32_t codepoint)
{
    Markup*        markup = context->m_Markup;
    const uint32_t text_offset = markup->m_Text.Size();
    EnsurePushCapacity(markup->m_Text);
    markup->m_Text.Push(codepoint);

    if (!markup->m_Spans.Empty() && markup->m_Spans.Back().m_StyleNodeIndex == context->m_StyleNodeIndex &&
        markup->m_Spans.Back().m_TextOffset + markup->m_Spans.Back().m_TextLength == text_offset)
    {
        ++markup->m_Spans.Back().m_TextLength;
    }
    else
    {
        MarkupSpan span = { text_offset, 1, context->m_StyleNodeIndex };
        EnsurePushCapacity(markup->m_Spans);
        markup->m_Spans.Push(span);
    }
}

struct ParseSnapshot
{
    uint32_t m_SourceOffset;
    uint32_t m_TextCount;
    uint32_t m_SpanCount;
    uint32_t m_LastSpanLength;
    uint32_t m_StyleNodeCount;
    uint32_t m_AttributeCount;
    uint16_t m_StyleNodeIndex;
};

static ParseSnapshot TakeSnapshot(ParseContext* context, uint32_t source_offset)
{
    ParseSnapshot snapshot;
    snapshot.m_SourceOffset = source_offset;
    snapshot.m_TextCount = context->m_Markup->m_Text.Size();
    snapshot.m_SpanCount = context->m_Markup->m_Spans.Size();
    snapshot.m_LastSpanLength = snapshot.m_SpanCount ? context->m_Markup->m_Spans.Back().m_TextLength : 0;
    snapshot.m_StyleNodeCount = context->m_Markup->m_StyleNodes.Size();
    snapshot.m_AttributeCount = context->m_Markup->m_Attributes.Size();
    snapshot.m_StyleNodeIndex = context->m_StyleNodeIndex;

    return snapshot;
}

static void RestoreSnapshot(ParseContext* context, const ParseSnapshot& snapshot)
{
    context->m_Markup->m_Text.SetSize(snapshot.m_TextCount);
    context->m_Markup->m_Spans.SetSize(snapshot.m_SpanCount);

    if (snapshot.m_SpanCount)
    {
        context->m_Markup->m_Spans.Back().m_TextLength = snapshot.m_LastSpanLength;
    }

    context->m_Markup->m_StyleNodes.SetSize(snapshot.m_StyleNodeCount);
    context->m_Markup->m_Attributes.SetSize(snapshot.m_AttributeCount);
    context->m_StyleNodeIndex = snapshot.m_StyleNodeIndex;
}

static uint32_t FindTagEnd(const char* source, uint32_t length, uint32_t cursor)
{
    while (cursor < length && source[cursor++] != '>')
    {
    }

    return cursor;
}

static MarkupResult PushLiteralRange(ParseContext* context, uint32_t begin, uint32_t end, MarkupError* error)
{
    uint32_t cursor = begin;

    while (cursor < end)
    {
        uint32_t     codepoint;
        MarkupResult result = DecodeCodepoint(context->m_Source, end, &cursor, &codepoint, error);

        if (result != MARKUP_RESULT_OK)
        {
            return result;
        }

        PushText(context, codepoint);
    }

    return MARKUP_RESULT_OK;
}

// Parses one tag and restores the nearest useful parser state on recoverable errors.
static MarkupResult ParseMarkupTag(ParseContext* context, dmArray<ParseSnapshot>* open_tags, bool recover, bool style_fragment, MarkupError* error)
{
    const uint32_t tag_start = context->m_Cursor;
    const bool     closing = tag_start + 1 < context->m_Length && context->m_Source[tag_start + 1] == '/';

    if (style_fragment && closing)
    {
        SetError(error, MARKUP_ERROR_UNEXPECTED_CLOSING_TAG, tag_start);

        return MARKUP_RESULT_SYNTAX_ERROR;
    }

    ParseSnapshot snapshot = {};

    if (recover)
    {
        snapshot = TakeSnapshot(context, tag_start);
    }

    MarkupError  parse_error = { tag_start, MARKUP_ERROR_NONE };
    bool         self_closing = false;
    MarkupResult result = ParseTag(context, &parse_error, &self_closing);

    if (result == MARKUP_RESULT_OK)
    {
        if (style_fragment && self_closing)
        {
            SetError(error, MARKUP_ERROR_INVALID_TAG, tag_start);

            return MARKUP_RESULT_SYNTAX_ERROR;
        }

        if (self_closing)
        {
            MarkupStyleNode& node = context->m_Markup->m_StyleNodes.Back();

            if (node.m_Tag.m_Length == 6 && memcmp(context->m_Source + node.m_Tag.m_Offset, "sprite", 6) == 0)
            {
                PushText(context, 0xfffc);
                node.m_TextLength = 1;
            }
        }

        if (!recover)
        {
            return MARKUP_RESULT_OK;
        }

        if (closing)
        {
            if (!open_tags->Empty())
            {
                open_tags->Pop();
            }
        }
        else if (!self_closing)
        {
            EnsurePushCapacity(*open_tags);
            open_tags->Push(snapshot);
        }

        return MARKUP_RESULT_OK;
    }

    SetFirstError(error, parse_error);

    if (!recover || result == MARKUP_RESULT_LIMIT_EXCEEDED)
    {
        return result;
    }

    if (parse_error.m_Type == MARKUP_ERROR_MISMATCHED_CLOSING_TAG && !open_tags->Empty())
    {
        const ParseSnapshot unmatched = open_tags->Back();
        open_tags->Pop();
        RestoreSnapshot(context, unmatched);
        result = PushLiteralRange(context, unmatched.m_SourceOffset, tag_start, error);
        context->m_Cursor = tag_start;

        return result;
    }

    RestoreSnapshot(context, snapshot);
    const uint32_t tag_end = parse_error.m_Type == MARKUP_ERROR_UNEXPECTED_CLOSING_TAG
                                 ? context->m_Cursor
                                 : FindTagEnd(context->m_Source, context->m_Length, parse_error.m_ByteOffset);
    result = PushLiteralRange(context, tag_start, tag_end, error);
    context->m_Cursor = tag_end;

    return result;
}

// Parses one visible codepoint, including entity recovery for authoring tools.
static MarkupResult ParseVisibleText(ParseContext* context, bool recover, MarkupError* error)
{
    if (context->m_Source[context->m_Cursor] != '&')
    {
        uint32_t    codepoint;
        MarkupError parse_error = { context->m_Cursor, MARKUP_ERROR_NONE };
        const MarkupResult result = DecodeCodepoint(context->m_Source, context->m_Length, &context->m_Cursor, &codepoint, &parse_error);

        if (result != MARKUP_RESULT_OK)
        {
            SetFirstError(error, parse_error);

            return result;
        }

        PushText(context, codepoint);

        return MARKUP_RESULT_OK;
    }

    const uint32_t entity_start = context->m_Cursor;
    uint32_t       codepoint;
    MarkupError    parse_error = { entity_start, MARKUP_ERROR_NONE };
    const MarkupResult result = ParseEntity(context, &codepoint, &parse_error);

    if (result == MARKUP_RESULT_OK)
    {
        PushText(context, codepoint);

        return MARKUP_RESULT_OK;
    }

    SetFirstError(error, parse_error);

    if (!recover || result == MARKUP_RESULT_LIMIT_EXCEEDED)
    {
        return result;
    }

    context->m_Cursor = entity_start + 1;
    PushText(context, '&');

    return MARKUP_RESULT_OK;
}

static MarkupResult MarkupCreateInternal(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error, bool recover, bool style_fragment)
{
    if (out_markup)
    {
        *out_markup = 0;
    }

    SetError(out_error, MARKUP_ERROR_NONE, 0);

    if (!out_markup || (!text && text_length != 0))
    {
        SetError(out_error, MARKUP_ERROR_INVALID_UTF8, 0);

        return MARKUP_RESULT_INVALID_UTF8;
    }

    Markup* markup = new Markup;
    markup->m_Source.SetCapacity(text_length + 1);
    markup->m_Source.SetSize(text_length + 1);

    if (text_length)
    {
        memcpy(markup->m_Source.Begin(), text, text_length);
    }

    markup->m_Source[text_length] = 0;

    MarkupStyleNode root = {};
    root.m_Parent = MARKUP_INVALID_INDEX;
    EnsurePushCapacity(markup->m_StyleNodes);
    markup->m_StyleNodes.Push(root);

    dmArray<ParseSnapshot> open_tags;
    ParseContext           context = { markup, markup->m_Source.Begin(), text_length, 0, 0 };
    MarkupResult           result = MARKUP_RESULT_OK;

    while (context.m_Cursor < context.m_Length)
    {
        if (context.m_Source[context.m_Cursor] == '<')
        {
            result = ParseMarkupTag(&context, &open_tags, recover, style_fragment, out_error);
        }
        else
        {
            if (style_fragment)
            {
                if (IsWhitespace(context.m_Source[context.m_Cursor]))
                {
                    ++context.m_Cursor;
                    continue;
                }

                SetError(out_error, MARKUP_ERROR_INVALID_TAG, context.m_Cursor);
                result = MARKUP_RESULT_SYNTAX_ERROR;
                break;
            }

            result = ParseVisibleText(&context, recover, out_error);
        }

        if (result != MARKUP_RESULT_OK)
        {
            break;
        }
    }

    if (result == MARKUP_RESULT_OK && style_fragment)
    {
        PushText(&context, 0xfffd);
        uint16_t node_index = context.m_StyleNodeIndex;

        while (node_index != 0 && node_index != MARKUP_INVALID_INDEX)
        {
            MarkupStyleNode& open_node = markup->m_StyleNodes[node_index];
            open_node.m_TextLength = 1;
            node_index = open_node.m_Parent;
        }

        context.m_StyleNodeIndex = 0;
    }

    if (result == MARKUP_RESULT_OK && context.m_StyleNodeIndex != 0)
    {
        const MarkupStyleNode& node = markup->m_StyleNodes[context.m_StyleNodeIndex];
        MarkupError            unclosed_error = { node.m_Tag.m_Offset - 1, MARKUP_ERROR_UNCLOSED_TAG };
        SetFirstError(out_error, unclosed_error);

        if (!recover)
        {
            result = MARKUP_RESULT_INCOMPLETE;
        }
        else
        {
            uint16_t node_index = context.m_StyleNodeIndex;

            while (node_index != 0 && node_index != MARKUP_INVALID_INDEX)
            {
                MarkupStyleNode& open_node = markup->m_StyleNodes[node_index];
                open_node.m_TextLength = markup->m_Text.Size() - open_node.m_TextOffset;
                node_index = open_node.m_Parent;
            }
        }
    }

    if (result != MARKUP_RESULT_OK)
    {
        delete markup;

        return result;
    }

    *out_markup = markup;

    return MARKUP_RESULT_OK;
}

MarkupResult MarkupCreate(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupCreateInternal(text, text_length, out_markup, out_error, false, false);
}

MarkupResult MarkupCreateRecovering(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupCreateInternal(text, text_length, out_markup, out_error, true, false);
}

MarkupResult MarkupCreateStyleFragment(const char* text, uint32_t text_length, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupCreateInternal(text, text_length, out_markup, out_error, false, true);
}

void MarkupDestroy(HMarkup markup)
{
    delete markup;
}

const char* MarkupGetSource(HMarkup markup)
{
    return markup->m_Source.Begin();
}

uint32_t MarkupGetSourceLength(HMarkup markup)
{
    return markup->m_Source.Size() - 1;
}

const uint32_t* MarkupGetText(HMarkup markup)
{
    return markup->m_Text.Begin();
}

uint32_t MarkupGetTextLength(HMarkup markup)
{
    return markup->m_Text.Size();
}

const MarkupSpan* MarkupGetSpans(HMarkup markup)
{
    return markup->m_Spans.Begin();
}

uint32_t MarkupGetSpanCount(HMarkup markup)
{
    return markup->m_Spans.Size();
}

const MarkupStyleNode* MarkupGetStyleNodes(HMarkup markup)
{
    return markup->m_StyleNodes.Begin();
}

uint32_t MarkupGetStyleNodeCount(HMarkup markup)
{
    return markup->m_StyleNodes.Size();
}

const MarkupAttribute* MarkupGetAttributes(HMarkup markup)
{
    return markup->m_Attributes.Begin();
}

uint32_t MarkupGetAttributeCount(HMarkup markup)
{
    return markup->m_Attributes.Size();
}
