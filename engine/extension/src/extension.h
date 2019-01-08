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
