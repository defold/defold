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

/*#
 * Handle that holds a collection of fonts to use during text shaping
 * @typedef
 * @name HFontCollection
 */
typedef struct FontCollection* HFontCollection;

/*#
 * Create a font collection
 * @name FontCollectionCreate
 * @return coll [type: HFontCollection] the font collection
 */
HFontCollection FontCollectionCreate();

/*# destroy a font collection
 * @name FontCollectionDestroy
 * @param coll [type: HFontCollection] the font collection
 */
void FontCollectionDestroy(HFontCollection coll);

/*# add a font to the font collection
 * @note No ownership transfer occurrs. HFont must be alive during the lifetime of the font collection
 * @name FontCollectionAddFont
 * @param coll [type: HFontCollection] the font collection
 * @param font [type: HFont] the font
 * @return result [type: FontResult] the result. FONT_RESULT_OK if successful
 */
FontResult FontCollectionAddFont(HFontCollection coll, HFont font);

/*# remove a font from the font collection
 * @name FontCollectionRemoveFont
 * @param coll [type: HFontCollection] the font collection
 * @param font [type: HFont] the font
 * @return result [type: FontResult] the result. FONT_RESULT_OK if successful
 */
FontResult FontCollectionRemoveFont(HFontCollection coll, HFont font);

/*# return number of fonts in the collection
 * @name FontCollectionGetFontCount
 * @param coll [type: HFontCollection] the font collection
 * @return count [type: uint32_t] the number of fonts
 */
uint32_t FontCollectionGetFontCount(HFontCollection coll);

/*# return the font associated with the given index
 * @name FontCollectionGetFont
 * @param coll [type: HFontCollection] the font collection
 * @return font [type: HFont] the font at the given index
 */
HFont FontCollectionGetFont(HFontCollection coll, uint32_t index);

#endif // DMSDK_FONTCOLLECTION_H
