#ifndef DM_GRAPHICS_ADAPTER_H
#define DM_GRAPHICS_ADAPTER_H

#include <dmsdk/graphics/graphics.h>

namespace dmGraphics
{
	struct GraphicsAdapterFunctionTable;
	struct GraphicsAdapter;
	extern GraphicsAdapter* g_adapter_list;


	typedef GraphicsAdapterFunctionTable (*GraphicsAdapterRegisterCb)();
	typedef HVertexBuffer                (*NewVertexBufferFn)(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage);
	typedef HContext                     (*NewContextFn)(const ContextParams& params);

	struct GraphicsAdapter
	{
		GraphicsAdapter()
		{
			memset(this, 0, sizeof(*this));
		}

		GraphicsAdapter(GraphicsAdapterRegisterCb cb, int priority)
		{
			m_Next         = g_adapter_list;
			g_adapter_list = this;
			m_RegisterCb   = cb;
			m_Priority     = priority;
		}

		struct GraphicsAdapter*   m_Next;
		GraphicsAdapterRegisterCb m_RegisterCb;
		int                       m_Priority;
	};

	struct GraphicsAdapterFunctionTable
	{
		NewVertexBufferFn m_NewVertexBuffer;
		NewContextFn      m_NewContext;
	};
}

#endif
