#ifndef DM_GRAPHICS_ADAPTER_H
#define DM_GRAPHICS_ADAPTER_H

#include <dmsdk/graphics/graphics.h>

namespace dmGraphics
{
    struct GraphicsAdapterFunctionTable;
    struct GraphicsAdapter;
    extern GraphicsAdapter* g_adapter_list;


    typedef GraphicsAdapterFunctionTable (*GraphicsAdapterRegisterFunctionsCb)();
    typedef bool                         (*GraphicsAdapterIsSupportedCb)();
    typedef HVertexBuffer                (*NewVertexBufferFn)(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
    typedef HContext                     (*NewContextFn)(const ContextParams& params);

    struct GraphicsAdapter
    {
        GraphicsAdapter(GraphicsAdapterIsSupportedCb is_supported_cb,
            GraphicsAdapterRegisterFunctionsCb register_functions_cb, int priority);
        struct GraphicsAdapter*            m_Next;
        GraphicsAdapterRegisterFunctionsCb m_RegisterCb;
        GraphicsAdapterIsSupportedCb       m_IsSupportedCb;
        int                                m_Priority;
    };

    struct GraphicsAdapterFunctionTable
    {
        NewVertexBufferFn    m_NewVertexBuffer;
        NewContextFn         m_NewContext;
    };
}

#endif
