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

#ifndef DMSDK_FONTCOLLECTION_H
#define DMSDK_FONTCOLLECTION_H

#include <dmsdk/font/font.h> // for the enums

#include <stdint.h>

/*# SDK Font Collection API documentation
 *
 * Font API for grouping multiple fonts into a collection
 *
 * @document
 * @name FontCollection
 * @path engine/font/src/dmsdk/font/fontcollection.h
 * @language C++
 */

typedef struct FontCollection* HFontCollection;

HFontCollection FontCollectionCreate();
void            FontCollectionDestroy(HFontCollection coll);
FontResult      FontCollectionAddFont(HFontCollection coll, HFont font);
uint32_t        FontCollectionGetFontCount(HFontCollection coll);
HFont           FontCollectionGetFont(HFontCollection coll, uint32_t index);

#endif // DMSDK_FONTCOLLECTION_H
