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

#ifndef DM_FONTCOLLECTION_H
#define DM_FONTCOLLECTION_H

#include <dmsdk/dlib/hash.h>
#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>
#include <dmsdk/font/text_layout.h>

struct TextRenderStyle;
struct TextEffect;
struct MarkupError;

/** Decoration geometry contributed by a named object's base style.
 *
 * Decorations affect layout geometry and must be registered before creating
 * layouts that use the named style. Runtime render-style changes preserve the
 * registered decoration.
 */
struct TextNamedStyleDecoration
{
    /** Bitwise `TextResolvedDecorationFlags`. */
    uint8_t m_Flags;
    /** `TextDecorationPattern` used for underlines. */
    uint8_t m_UnderlinePattern;
    /** `TextDecorationPattern` used for strikethroughs. */
    uint8_t m_StrikePattern;
};

TextLayoutType FontCollectionGetLayoutType(HFontCollection coll);

/** Called when full text layout cannot find a font for a language and script.
 * The callback may add fonts to the collection and return true to retry matching. */
typedef bool (*FontCollectionFallbackCallback)(HFontCollection collection, const char* language,
                                               uint32_t script, uint8_t font_family, void* context);

/** Set the fallback callback used by full text layout. */
void FontCollectionSetFallbackCallback(HFontCollection collection, FontCollectionFallbackCallback callback, void* context);

/** Copy named render properties and effects, preserving decorations, and increment the style revision. */
void FontCollectionSetNamedStyle(HFontCollection collection, dmhash_t name, const TextRenderStyle& style, const TextEffect* effects, uint32_t effect_count);

/** Parse and replace a named opening-only markup style definition. */
bool FontCollectionSetNamedStyleMarkup(HFontCollection collection, dmhash_t name, const char* definition, uint32_t definition_length, MarkupError* error);

/** Set the decoration applied by a named style. Must be called before creating layouts that use the style. */
void FontCollectionSetNamedStyleDecoration(HFontCollection collection, dmhash_t name, const TextNamedStyleDecoration& decoration);

/** Return a borrowed named style, or null when the name is not registered. */
const TextRenderStyle* FontCollectionGetNamedStyle(HFontCollection collection, dmhash_t name);

/** Return the ordered effects stored in a named style. */
const TextEffect* FontCollectionGetNamedStyleEffects(HFontCollection collection, dmhash_t name, uint32_t* effect_count);

/** Return the borrowed decoration stored in a named style, or null when none is registered. */
const TextNamedStyleDecoration* FontCollectionGetNamedStyleDecoration(HFontCollection collection, dmhash_t name);

/** Revision used by layouts to refresh named styles without reshaping text. */
uint32_t FontCollectionGetNamedStyleRevision(HFontCollection collection);

#if defined(FONT_USE_SKRIBIDI)
#include <skribidi/skb_font_collection.h>

skb_font_collection_t* FontCollectionGetSkribidiPtr(HFontCollection coll);

HFont FontCollectionGetFontFromHandle(HFontCollection coll, skb_font_handle_t handle);
#endif

#endif // DM_FONTCOLLECTION_H
