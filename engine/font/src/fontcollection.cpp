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

#include <dmsdk/dlib/array.h>
#include <dmsdk/font/fontcollection.h>

#include "font_private.h"

#if defined(FONT_USE_SKRIBIDI)
    #include <dmsdk/dlib/hashtable.h>
    #include <skribidi/skb_font_collection.h>
#endif

struct FontCollection
{
    dmArray<HFont> m_Fonts;
    TextLayoutType m_LayoutType;

#if defined(FONT_USE_SKRIBIDI)
    skb_font_collection_t* m_Collection;
    dmHashTable<skb_font_handle_t, HFont> m_FontLookup;
#endif
};


HFontCollection FontCollectionCreate()
{
    FontCollection* coll = new FontCollection;
    coll->m_LayoutType = TEXT_LAYOUT_TYPE_FULL;

#if defined(FONT_USE_SKRIBIDI)
    coll->m_Collection = skb_font_collection_create();
#endif
    return coll;
};

void FontCollectionDestroy(HFontCollection coll)
{
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
                                                hb_font,
                                                FontGetPath(hfont),
                                                SKB_FONT_FAMILY_DEFAULT);

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

uint32_t FontCollectionGetFontCount(HFontCollection coll)
{
    return coll->m_Fonts.Size();
}

HFont FontCollectionGetFont(HFontCollection coll, uint32_t index)
{
    return coll->m_Fonts[index];
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
