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

#ifndef DM_EXTENSION_HPP
#define DM_EXTENSION_HPP

#include <dmsdk/extension/extension.hpp>

struct ExtensionDesc;

namespace dmExtension
{
    typedef const ExtensionDesc* HExtension;

    /**
     * Extension initialization desc
     */

    /**
     * Get first extension
     * @return extension [type:HExtension] The first extension, or 0.
     */
    HExtension GetFirstExtension();

    /**
     * Get next extension
     * @return extension [type:HExtension] The next extension, or 0.
     */
    HExtension GetNextExtension(HExtension extension);

    /**
     * Initialize all extends at application level
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result AppInitialize(AppParams* params);

    /**
     * Initialize all extends level
     * Called after AppInitialize has been called for all extensions
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result Initialize(Params* params);

    /**
     * Finalize all extensions
     * Called before any AppFinalize has been called for any extensions
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result Finalize(Params* params);

    /**
     * Update all extensions
     * Each extension must have been initialized ok
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result Update(Params* params);

    /**
     * Call pre render functions for extensions
     * @param params parameters
     */
    void PreRender(Params* params);

    /**
     * Call post render functions for extensions
     * @param params parameters
     */
    void PostRender(Params* params);

    /**
     * Initialize all extends at application level
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result AppFinalize(AppParams* params);

    /**
     * Dispatches an event to each extension's OnEvent callback
     * @param params parameters
     * @param event the app event
     */
    void DispatchEvent(Params* params, const Event* event);
}

#endif // #ifndef DM_EXTENSION_H
