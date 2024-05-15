// Copyright 2020-2024 The Defold Foundation
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

#ifndef DISPLAY_PROFILES_H
#define DISPLAY_PROFILES_H

#include <stdint.h>

#include <ddf/ddf.h>

#include "render.h"
#include "render/render_ddf.h"

namespace dmRender
{
    /**
     * Display profile descriptor
     */
    struct DisplayProfileDesc
    {
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Dpi;
    };

    /**
     * Parameters supplied to NewDisplayProfiles
     */
    struct DisplayProfilesParams
    {
        /// Default constructor
        DisplayProfilesParams();

        /// Pointer to DDF buffer
        dmRenderDDF::DisplayProfiles* m_DisplayProfilesDDF;
        dmhash_t m_NameHash;
    };

    /**
     * Create a new display profiles instance
     */
    HDisplayProfiles NewDisplayProfiles();

    /**
     * Update the display profiles with the specified parameters.
     * @param profiles Display profiles handle
     * @param params Parameters to update
     * @returns Number of profiles
     */
    uint32_t SetDisplayProfiles(HDisplayProfiles profiles, DisplayProfilesParams& params);

    /**
     * Delete display profiles
     * @param profiles Display profiles handle
     */
    void DeleteDisplayProfiles(HDisplayProfiles profiles);

    /**
     * Get display profiles best matching profile based on width, height and dpi
     * @param profiles Display profiles handle
     * @param width Width
     * @param height Height
     * @param dpi Dots per inch. Ignored if zero
     * @param id_choices dmhash_t array of id's that's included in the matching algorithm. If 0, all are included
     * @return dmhash_t id of best matching display profile
     */
    dmhash_t GetOptimalDisplayProfile(HDisplayProfiles profiles, uint32_t width, uint32_t height, uint32_t dpi, const dmArray<dmhash_t>* id_choices);

    /**
     * Get display profile descriptor
     * @param profiles Display profiles handle
     * @param id hashed id
     * @param desc_out result
     * @return RESULT_OK or RESULT_INVALID_PARAMETER if id was not found in profiles
     */
    Result GetDisplayProfileDesc(HDisplayProfiles profiles, dmhash_t id, DisplayProfileDesc& desc_out);

    /**
     * Get display profiles name
     * @param profiles Display profiles handle
     * @return hashed name
     */
    dmhash_t GetDisplayProfilesName(HDisplayProfiles profiles);

}

#endif // DISPLAY_PROFILES_H

