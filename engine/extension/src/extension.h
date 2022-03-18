// Copyright 2020-2022 The Defold Foundation
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

#ifndef DM_EXTENSION
#define DM_EXTENSION

#include <dmsdk/extension/extension.h>


namespace dmExtension
{
    /**
     * Extension initialization desc
     */
    struct Desc
    {
        const char* m_Name;
        Result (*AppInitialize)(AppParams* params);
        extension_callback_t PreRender;
        extension_callback_t PostRender;
        Result (*AppFinalize)(AppParams* params);
        Result (*Initialize)(Params* params);
        Result (*Finalize)(Params* params);
        Result (*Update)(Params* params);
        void   (*OnEvent)(Params* params, const Event* event);
        const Desc* m_Next;
        bool        m_AppInitialized;
    };

    /**
     * Get first extension
     * @return
     */
    const Desc* GetFirstExtension();

    /**
     * Initialize all extends at application level
     * @param params parameters
     * @return RESULT_OK on success
     */
    Result AppInitialize(AppParams* params);

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

#endif // #ifndef DM_EXTENSION
