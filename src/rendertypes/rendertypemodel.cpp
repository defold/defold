#include <vectormath/cpp/vectormath_aos.h>
#include <graphics/graphics_device.h>
#include "render/render.h"
#include "renderinternal.h"
#include "rendertypemodel.h"
#include "../model/model.h"


namespace dmRender
{
	using namespace Vectormath::Aos;

	void RenderTypeModelSetup(RenderContext* rendercontext)
	{
        dmGraphics::HContext context = rendercontext->m_GFXContext;

		dmGraphics::SetDepthMask(context, true);
		dmGraphics::EnableState(context, dmGraphics::DEPTH_TEST);

        dmGraphics::DisableState(context, dmGraphics::BLEND);
	}


    void RenderTypeModelDraw(RenderContext* rendercontext, RenderObject* ro)
    {
    	dmModel::HModel model = (dmModel::HModel)ro->m_Data;
    	Quat rotation = ro->m_Rot;
    	Point3 position = ro->m_Pos;

    	dmGraphics::HMaterial material = dmModel::GetMaterial(model);

        dmModel::HMesh mesh = dmModel::GetMesh(model);
        if (dmModel::GetPrimitiveCount(mesh) == 0)
            return;

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        dmGraphics::SetTexture(context, dmModel::GetTexture0(model));


        dmGraphics::SetFragmentProgram(context, dmGraphics::GetMaterialFragmentProgram(material) );

#if 1
        for (uint32_t i=0; i<dmGraphics::MAX_MATERIAL_CONSTANTS; i++)
        {
            uint32_t mask = dmGraphics::GetMaterialFragmentConstantMask(material);
            if (mask & (1 << i) )
            {
                Vector4 v = dmGraphics::GetMaterialFragmentProgramConstant(material, i);
                dmGraphics::SetFragmentConstant(context, &v, i);
            }
        }
#endif

        dmGraphics::SetFragmentConstant(context, &ro->m_Colour[0], 0);

        dmGraphics::SetVertexProgram(context, dmGraphics::GetMaterialVertexProgram(material) );

        Matrix4 m(rotation, Vector3(position));

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&m, 4, 4);

        Matrix4 texturmatrix;
        texturmatrix = Matrix4::identity();
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&texturmatrix, 8, 4);


        for (uint32_t i=0; i<dmGraphics::MAX_MATERIAL_CONSTANTS; i++)
        {
            uint32_t mask = dmGraphics::GetMaterialVertexConstantMask(material);
            if (mask & (1 << i) )
            {
                Vector4 v = dmGraphics::GetMaterialVertexProgramConstant(material, i);
                dmGraphics::SetVertexConstantBlock(context, &v, i, 1);
            }
        }

        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) dmModel::GetPositions(mesh));

        if (dmModel::GetTexcoord0Count(mesh) > 0)
            dmGraphics::SetVertexStream(context, 1, 2, dmGraphics::TYPE_FLOAT, 0, (void*) dmModel::GetTexcoord0(mesh));
        else
            dmGraphics::DisableVertexStream(context, 1);

        if (dmModel::GetNormalCount(mesh) > 0)
            dmGraphics::SetVertexStream(context, 2, 3, dmGraphics::TYPE_FLOAT, 0, (void*) dmModel::GetNormals(mesh));
        else
            dmGraphics::DisableVertexStream(context, 2);

        dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, dmModel::GetPrimitiveCount(mesh)*3, dmGraphics::TYPE_UNSIGNED_INT, dmModel::GetIndices(mesh));

        dmGraphics::DisableVertexStream(context, 0);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);
    }


}
