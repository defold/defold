#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>
#include "render.h"

namespace dmRender
{
	using namespace Vectormath::Aos;

	struct RenderObject
	{
		Point3				m_Pos;
		Quat				m_Rot;
		RenderObjectType	m_Type;
		void*				m_Go;
		void*				m_Data;
		uint16_t			m_MarkForDelete:1;
	};

}

#endif

