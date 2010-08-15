#ifndef RENDERINTERNAL_H
#define RENDERINTERNAL_H

#include <vectormath/cpp/vectormath_aos.h>
#include <ddf/ddf.h>
#include "render/material_ddf.h"
#include "render/model_ddf.h"
#include "render/mesh_ddf.h"
#include "render.h"


namespace dmRender
{
	using namespace Vectormath::Aos;

	struct RenderObject
	{
		Vector4				m_Colour[dmRender::MaterialDesc::MAX_COLOR];
		Point3				m_Pos;
		Quat				m_Rot;
		RenderObjectType	m_Type;
		void*				m_Go;
		void*				m_Data;
		uint16_t			m_MarkForDelete:1;
		uint16_t            m_Enabled:1;
	};

}

#endif

