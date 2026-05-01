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

#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>
#include <dmsdk/font/text_layout.h>

TextLayoutType FontCollectionGetLayoutType(HFontCollection coll);

#if defined(FONT_USE_SKRIBIDI)
#include <skribidi/skb_font_collection.h>

skb_font_collection_t* FontCollectionGetSkribidiPtr(HFontCollection coll);

HFont FontCollectionGetFontFromHandle(HFontCollection coll, skb_font_handle_t handle);
#endif

#endif // DM_FONTCOLLECTION_H
