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

#if defined(FONT_USE_SKRIBIDI)
    #include <dmsdk/dlib/hashtable.h>
    #include <skribidi/skb_font_collection.h>
    #include <skribidi/skb_layout.h>
    #include "harfbuzz/font_harfbuzz.h"
#endif

struct TextNamedStyle
{
    dmhash_t            m_Name;
    TextRenderStyle     m_Style;
    dmArray<TextEffect> m_Effects;
};

struct FontCollection
{
    dmArray<HFont>                  m_Fonts;
    dmArray<TextNamedStyle*>        m_NamedStyles;
    TextLayoutType                 m_LayoutType;
    uint32_t                       m_NamedStyleRevision;
    FontCollectionFallbackCallback m_FallbackCallback;
    void*                           m_FallbackContext;

#if defined(FONT_USE_SKRIBIDI)
    skb_font_collection_t* m_Collection;
    dmHashTable<skb_font_handle_t, HFont> m_FontLookup;
#endif
};

static TextRenderStyle MakeColorStyle(float r, float g, float b, float a)
{
    TextRenderStyle style = {};
    style.m_FaceColor[0] = r;
    style.m_FaceColor[1] = g;
    style.m_FaceColor[2] = b;
    style.m_FaceColor[3] = a;
    style.m_Flags = TEXT_RENDER_STYLE_FACE_COLOR;

    return style;
}

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
    collection->m_NamedStyles.Push(named_style);

    return named_style;
}

HFontCollection FontCollectionCreate()
{
    FontCollection* coll = new FontCollection;
    coll->m_LayoutType = TEXT_LAYOUT_TYPE_FULL;
    coll->m_NamedStyleRevision = 0;
    coll->m_FallbackCallback = 0;
    coll->m_FallbackContext = 0;

#if defined(FONT_USE_SKRIBIDI)
    coll->m_Collection = skb_font_collection_create();
#endif
    FontCollectionSetNamedStyle(coll, dmHashString64("link"), MakeColorStyle(0.10f, 0.45f, 0.90f, 1.0f));
    FontCollectionSetNamedStyle(coll, dmHashString64("link:hover"), MakeColorStyle(0.30f, 0.65f, 1.00f, 1.0f));
    FontCollectionSetNamedStyle(coll, dmHashString64("link:active"), MakeColorStyle(0.05f, 0.30f, 0.70f, 1.0f));
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

FontResult FontCollectionAddFont(HFontCollection coll, HFont hfont)
{
    if (coll->m_Fonts.Full())
        coll->m_Fonts.OffsetCapacity(1);
    coll->m_Fonts.Push(hfont);

    TextLayoutType layout_type = FontGetLayoutType(hfont);
    if (layout_type != TEXT_LAYOUT_TYPE_FULL)
        coll->m_LayoutType = TEXT_LAYOUT_TYPE_LEGACY;

    // We mustn't use the full layout code path if the font doesn't support it
    if (coll->m_LayoutType == TEXT_LAYOUT_TYPE_LEGACY)
    {
        return FONT_RESULT_OK;
    }

#if defined(FONT_USE_SKRIBIDI)
    hb_font_t* hb_font = FontGetHarfbuzzFontFromTTF(hfont);
    skb_font_handle_t skbfont = skb_font_collection_add_hb_font(coll->m_Collection,
                                                                FontGetPath(hfont),
                                                                hb_font,
                                                                SKB_FONT_FAMILY_DEFAULT,
                                                                0);

    if (skbfont)
    {
        HFont* pfont = coll->m_FontLookup.Get(skbfont);
        if (!pfont && coll->m_FontLookup.Full())
        {
            coll->m_FontLookup.OffsetCapacity(1);
        }
        coll->m_FontLookup.Put(skbfont, hfont);
    }
    return skbfont ? FONT_RESULT_OK : FONT_RESULT_ERROR;
#else
    return FONT_RESULT_OK;
#endif
}

FontResult FontCollectionRemoveFont(HFontCollection coll, HFont hfont)
{
    for (uint32_t i = 0; i < coll->m_Fonts.Size(); ++i)
    {
        if (coll->m_Fonts[i] == hfont)
        {
            coll->m_Fonts.EraseSwap(i);
            break;
        }
    }

#if defined(FONT_USE_SKRIBIDI)
     dmHashTable<skb_font_handle_t, HFont>::Iterator iter = coll->m_FontLookup.GetIterator();
     while(iter.Next())
     {
        if (iter.GetValue() == hfont)
        {
            skb_font_collection_remove_font(coll->m_Collection, iter.GetKey());
            coll->m_FontLookup.Erase(iter.GetKey());
            break;
        }
     }
#endif
    return FONT_RESULT_OK;
}

TextLayoutType FontCollectionGetLayoutType(HFontCollection coll)
{
    return coll->m_LayoutType;
}

void FontCollectionSetNamedStyle(HFontCollection collection, dmhash_t name, const TextRenderStyle& style)
{
    TextNamedStyle* named_style = GetOrCreateNamedStyle(collection, name);
    named_style->m_Style = style;
    named_style->m_Effects.SetCapacity(0);
    ++collection->m_NamedStyleRevision;
}

bool FontCollectionSetNamedStyleMarkup(HFontCollection collection, dmhash_t name, const char* definition, uint32_t definition_length, MarkupError* error)
{
    TextRenderStyle    style = {};
    dmArray<TextEffect> effects;

    if (!TextLayoutCompileStyleFragment(definition, definition_length, &style, &effects, error))
    {
        effects.SetCapacity(0);

        return false;
    }

    TextNamedStyle* named_style = GetOrCreateNamedStyle(collection, name);
    named_style->m_Style = style;
    named_style->m_Effects.Swap(effects);
    effects.SetCapacity(0);
    ++collection->m_NamedStyleRevision;

    return true;
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
