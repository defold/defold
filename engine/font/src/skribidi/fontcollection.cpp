// Copyright 2020-2025 The Defold Foundation
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

#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/font/fontcollection.h>

#include "font_private.h"

#include <skribidi/skb_font_collection.h>

struct FontCollection
{
    skb_font_collection_t* m_Collection;
    dmHashTable<skb_font_handle_t, HFont> m_Fonts;
};


HFontCollection FontCollectionCreate()
{
    FontCollection* coll = new FontCollection;
    coll->m_Collection = skb_font_collection_create();
    return coll;
};

void FontCollectionDestroy(HFontCollection coll)
{
    skb_font_collection_destroy(coll->m_Collection);
    delete coll;
}

FontResult FontCollectionAddFont(HFontCollection coll, HFont hfont)
{
    hb_font_t* hb_font = FontGetHarfbuzzFontFromTTF(hfont);
    skb_font_handle_t skbfont = skb_font_collection_add_hb_font(coll->m_Collection,
                                                hb_font,
                                                FontGetPath(hfont),
                                                SKB_FONT_FAMILY_DEFAULT);

    if (skbfont)
    {
        HFont* pfont = coll->m_Fonts.Get(skbfont);
        if (!pfont && coll->m_Fonts.Full())
        {
            coll->m_Fonts.OffsetCapacity(1);
        }
        coll->m_Fonts.Put(skbfont, hfont);
    }
    return skbfont ? FONT_RESULT_OK : FONT_RESULT_ERROR;
}

uint32_t FontCollectionGetFontCount(HFontCollection coll)
{
    assert(false); // shouldn't be used/needed with this backend
    return 0;
}

HFont FontCollectionGetFont(HFontCollection coll, uint32_t index)
{
    assert(false); // shouldn't be used/needed with this backend
    return 0;
}

// Used by the text_layout.cpp
skb_font_collection_t* FontCollectionGetSkribidiPtr(HFontCollection coll)
{
    return coll->m_Collection;
}

HFont FontCollectionGetFontFromHandle(HFontCollection coll, skb_font_handle_t handle)
{
    HFont* pfont = coll->m_Fonts.Get(handle);
    return pfont ? *pfont : 0;
}
