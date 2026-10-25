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

#include "font_private.h"
#include "fontcollection.h"
#include "text_layout.h"

#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/font/fontcollection.h>
#include <string.h>

#if defined(FONT_USE_SKRIBIDI)
    #include <dmsdk/dlib/hashtable.h>
    #include <skribidi/skb_font_collection.h>
    #include <skribidi/skb_layout.h>
    #include "harfbuzz/font_harfbuzz.h"
    #include "font_ttf.h"
    #include "truetype/font_colr.h"
#endif

struct TextNamedStyle
{
    dmhash_t                 m_Name;
    TextRenderStyle          m_Style;
    TextNamedStyleDecoration m_Decoration;
    TextNamedStyleDecoration m_BaseDecoration;
    dmArray<TextEffect>      m_Effects;
};

struct FontCollection
{
    dmArray<HFont>                  m_Fonts;
    bool                            m_ColorEmojiEnabled;
    dmArray<TextNamedStyle*>        m_NamedStyles;
    TextLayoutType                  m_LayoutType;
    uint32_t                        m_NamedStyleRevision;
    FontCollectionFallbackCallback  m_FallbackCallback;
    void*                           m_FallbackContext;

#if defined(FONT_USE_SKRIBIDI)
    dmArray<skb_font_handle_t>            m_EmojiHandles;
    skb_font_collection_t*                m_Collection;
    dmHashTable<skb_font_handle_t, HFont> m_FontLookup;
#endif
};

static TextNamedStyle* FindNamedStyle(HFontCollection collection, dmhash_t name)
{
    for (uint32_t i = 0; i < collection->m_NamedStyles.Size(); ++i)
    {
        if (collection->m_NamedStyles[i]->m_Name == name)
        {
            return collection->m_NamedStyles[i];
        }
    }

    return 0;
}

static TextNamedStyle* GetOrCreateNamedStyle(HFontCollection collection, dmhash_t name)
{
    TextNamedStyle* named_style = FindNamedStyle(collection, name);

    if (named_style)
    {
        return named_style;
    }

    if (collection->m_NamedStyles.Full())
    {
        collection->m_NamedStyles.OffsetCapacity(1);
    }

    named_style = new TextNamedStyle;
    named_style->m_Name = name;
    memset(&named_style->m_Style, 0, sizeof(named_style->m_Style));
    memset(&named_style->m_Decoration, 0, sizeof(named_style->m_Decoration));
    memset(&named_style->m_BaseDecoration, 0, sizeof(named_style->m_BaseDecoration));
    collection->m_NamedStyles.Push(named_style);

    return named_style;
}

HFontCollection FontCollectionCreate()
{
    FontCollection* coll = new FontCollection;
    coll->m_LayoutType = TEXT_LAYOUT_TYPE_FULL;
    coll->m_ColorEmojiEnabled = true;
    coll->m_NamedStyleRevision = 0;
    coll->m_FallbackCallback = 0;
    coll->m_FallbackContext = 0;

#if defined(FONT_USE_SKRIBIDI)
    coll->m_Collection = skb_font_collection_create();
#endif
    return coll;
}

void FontCollectionDestroy(HFontCollection coll)
{
    for (uint32_t i = 0; i < coll->m_NamedStyles.Size(); ++i)
    {
        delete coll->m_NamedStyles[i];
    }
#if defined(FONT_USE_SKRIBIDI)
    skb_font_collection_destroy(coll->m_Collection);
#endif
    delete coll;
}

#if defined(FONT_USE_SKRIBIDI)
static skb_font_handle_t AddSkribidiFont(HFontCollection coll, HFont font, uint8_t family)
{
    skb_font_handle_t handle = skb_font_collection_add_hb_font(coll->m_Collection, FontGetPath(font), FontGetHarfbuzzFontFromTTF(font), family, 0);
    if (handle)
    {
        if (coll->m_FontLookup.Full())
            coll->m_FontLookup.OffsetCapacity(1);
        coll->m_FontLookup.Put(handle, font);
    }
    return handle;
}

static skb_font_handle_t AddEmojiFont(HFontCollection coll, HFont font)
{
    const FontColr* colr = FontGetColrTTF(font);
    if (!coll->m_ColorEmojiEnabled && colr && (colr->m_BaseGlyphCount || colr->m_BaseGlyphList))
        return 0;
    // Emoji is a separate Skribidi family. Include outline fonts as well as
    // COLR fonts so SDF/Bitmap can use the collection's monochrome fallback.
    return AddSkribidiFont(coll, font, SKB_FONT_FAMILY_EMOJI);
}
#endif

FontResult FontCollectionAddFont(HFontCollection coll, HFont hfont)
{
    if (coll->m_Fonts.Full())
        coll->m_Fonts.OffsetCapacity(1);
    coll->m_Fonts.Push(hfont);
#if defined(FONT_USE_SKRIBIDI)
    if (coll->m_EmojiHandles.Full())
        coll->m_EmojiHandles.OffsetCapacity(1);
    coll->m_EmojiHandles.Push(0);
#endif

    TextLayoutType layout_type = FontGetLayoutType(hfont);
    if (layout_type != TEXT_LAYOUT_TYPE_FULL)
        coll->m_LayoutType = TEXT_LAYOUT_TYPE_LEGACY;

    // We mustn't use the full layout code path if the font doesn't support it
    if (coll->m_LayoutType == TEXT_LAYOUT_TYPE_LEGACY)
    {
        return FONT_RESULT_OK;
    }

#if defined(FONT_USE_SKRIBIDI)
    if (!AddSkribidiFont(coll, hfont, SKB_FONT_FAMILY_DEFAULT))
    {
        FontCollectionRemoveFont(coll, hfont);
        return FONT_RESULT_ERROR;
    }
    coll->m_EmojiHandles.Back() = AddEmojiFont(coll, hfont);
#endif
    return FONT_RESULT_OK;
}

void FontCollectionSetColorEmojiEnabled(HFontCollection coll, bool enabled)
{
    if (coll->m_ColorEmojiEnabled == enabled)
        return;
    coll->m_ColorEmojiEnabled = enabled;
#if defined(FONT_USE_SKRIBIDI)
    // Preserve the collection and default-family handles held by layouts.
    // Remove in reverse order: Skribidi reuses freed slots in LIFO order.
    for (uint32_t i = coll->m_EmojiHandles.Size(); i-- > 0;)
    {
        skb_font_handle_t handle = coll->m_EmojiHandles[i];
        if (handle)
        {
            skb_font_collection_remove_font(coll->m_Collection, handle);
            coll->m_FontLookup.Erase(handle);
        }
        coll->m_EmojiHandles[i] = 0;
    }
    if (coll->m_LayoutType == TEXT_LAYOUT_TYPE_FULL)
        for (uint32_t i = 0; i < coll->m_Fonts.Size(); ++i)
            coll->m_EmojiHandles[i] = AddEmojiFont(coll, coll->m_Fonts[i]);
#endif
}

FontResult FontCollectionRemoveFont(HFontCollection coll, HFont hfont)
{
    for (uint32_t i = 0; i < coll->m_Fonts.Size(); ++i)
    {
        if (coll->m_Fonts[i] == hfont)
        {
            coll->m_Fonts.EraseSwap(i);
#if defined(FONT_USE_SKRIBIDI)
            coll->m_EmojiHandles.EraseSwap(i);
#endif
            break;
        }
    }

#if defined(FONT_USE_SKRIBIDI)
    // Erasing can invalidate the iterator. Restart for the optional emoji alias.
    bool removed = true;
    while (removed)
    {
        removed = false;
        dmHashTable<skb_font_handle_t, HFont>::Iterator iter = coll->m_FontLookup.GetIterator();
        while (iter.Next())
        {
            if (iter.GetValue() == hfont)
            {
                skb_font_collection_remove_font(coll->m_Collection, iter.GetKey());
                coll->m_FontLookup.Erase(iter.GetKey());
                removed = true;
                break;
            }
        }
    }
#endif
    return FONT_RESULT_OK;
}

TextLayoutType FontCollectionGetLayoutType(HFontCollection coll)
{
    return coll->m_LayoutType;
}

void FontCollectionSetNamedStyle(HFontCollection collection, dmhash_t name, const TextRenderStyle& style, const TextEffect* effects, uint32_t effect_count)
{
    TextNamedStyle* named_style = GetOrCreateNamedStyle(collection, name);
    named_style->m_Style = style;
    named_style->m_Effects.EnsureSize(effect_count);
    if (effect_count)
        memcpy(named_style->m_Effects.Begin(), effects, effect_count * sizeof(TextEffect));
    ++collection->m_NamedStyleRevision;
}

bool FontCollectionSetNamedStyleMarkup(HFontCollection collection, dmhash_t name, const char* definition, uint32_t definition_length, MarkupError* error)
{
    TextRenderStyle          style = {};
    dmArray<TextEffect>      effects;
    TextNamedStyleDecoration decoration = {};

    if (!TextLayoutCompileStyleFragment(definition, definition_length, &style, &effects, &decoration, error))
    {
        effects.SetCapacity(0);

        return false;
    }

    TextNamedStyle* named_style = GetOrCreateNamedStyle(collection, name);
    named_style->m_Style = style;
    named_style->m_Effects.Swap(effects);
    named_style->m_Decoration = decoration.m_Flags ? decoration : named_style->m_BaseDecoration;
    effects.SetCapacity(0);
    ++collection->m_NamedStyleRevision;

    return true;
}

void FontCollectionSetNamedStyleDecoration(HFontCollection collection, dmhash_t name, const TextNamedStyleDecoration& decoration)
{
    TextNamedStyle* named_style = GetOrCreateNamedStyle(collection, name);
    named_style->m_Decoration = decoration;
    named_style->m_BaseDecoration = decoration;
    ++collection->m_NamedStyleRevision;
}

const TextRenderStyle* FontCollectionGetNamedStyle(HFontCollection collection, dmhash_t name)
{
    const TextNamedStyle* named_style = FindNamedStyle(collection, name);

    return named_style ? &named_style->m_Style : 0;
}

const TextEffect* FontCollectionGetNamedStyleEffects(HFontCollection collection, dmhash_t name, uint32_t* effect_count)
{
    const TextNamedStyle* named_style = FindNamedStyle(collection, name);

    if (!named_style)
    {
        *effect_count = 0;

        return 0;
    }

    *effect_count = named_style->m_Effects.Size();

    return named_style->m_Effects.Begin();
}

const TextNamedStyleDecoration* FontCollectionGetNamedStyleDecoration(HFontCollection collection, dmhash_t name)
{
    const TextNamedStyle* named_style = FindNamedStyle(collection, name);

    return named_style && named_style->m_Decoration.m_Flags ? &named_style->m_Decoration : 0;
}

uint32_t FontCollectionGetNamedStyleRevision(HFontCollection collection)
{
    return collection->m_NamedStyleRevision;
}

uint32_t FontCollectionGetFontCount(HFontCollection coll)
{
    return coll->m_Fonts.Size();
}

HFont FontCollectionGetFont(HFontCollection coll, uint32_t index)
{
    return coll->m_Fonts[index];
}

#if defined(FONT_USE_SKRIBIDI)
static bool OnFontFallback(skb_font_collection_t* collection, const char* language, uint8_t script,
                           uint8_t font_family, void* context)
{
    (void)collection;
    FontCollection* font_collection = (FontCollection*)context;

    return font_collection->m_FallbackCallback(font_collection, language,
                                                skb_script_to_iso15924_tag(script), font_family,
                                                font_collection->m_FallbackContext);
}
#endif

void FontCollectionSetFallbackCallback(HFontCollection collection, FontCollectionFallbackCallback callback, void* context)
{
    collection->m_FallbackCallback = callback;
    collection->m_FallbackContext = context;
#if defined(FONT_USE_SKRIBIDI)
    skb_font_collection_set_on_font_fallback(collection->m_Collection, callback ? OnFontFallback : 0, collection);
#endif
}

#if defined(FONT_USE_SKRIBIDI)
// Used by the text_layout.cpp
skb_font_collection_t* FontCollectionGetSkribidiPtr(HFontCollection coll)
{
    return coll->m_Collection;
}

HFont FontCollectionGetFontFromHandle(HFontCollection coll, skb_font_handle_t handle)
{
    HFont* pfont = coll->m_FontLookup.Get(handle);
    return pfont ? *pfont : 0;
}
#endif
