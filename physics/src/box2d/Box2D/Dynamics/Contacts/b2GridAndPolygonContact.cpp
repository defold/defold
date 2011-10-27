#include <Box2D/Dynamics/Contacts/b2GridAndPolygonContact.h>
#include <Box2D/Dynamics/b2Body.h>
#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/b2WorldCallbacks.h>
#include <Box2D/Common/b2BlockAllocator.h>
#include <Box2D/Collision/b2TimeOfImpact.h>
#include <Box2D/Collision/Shapes/b2GridShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Collision/Shapes/b2EdgeShape.h>

#include <new>
using namespace std;

b2Contact* b2GridAndPolygonContact::Create(b2Fixture* fixtureA, int32 indexA, b2Fixture* fixtureB, int32, b2BlockAllocator* allocator)
{
	void* mem = allocator->Allocate(sizeof(b2GridAndPolygonContact));
	return new (mem) b2GridAndPolygonContact(fixtureA, indexA, fixtureB);
}

void b2GridAndPolygonContact::Destroy(b2Contact* contact, b2BlockAllocator* allocator)
{
	((b2GridAndPolygonContact*)contact)->~b2GridAndPolygonContact();
	allocator->Free(contact, sizeof(b2GridAndPolygonContact));
}

b2GridAndPolygonContact::b2GridAndPolygonContact(b2Fixture* fixtureA, int32 indexA, b2Fixture* fixtureB)
	: b2Contact(fixtureA, indexA, fixtureB, 0)
{
	b2Assert(m_fixtureA->GetType() == b2Shape::e_grid);
	b2Assert(m_fixtureB->GetType() == b2Shape::e_polygon);

    b2GridShape* gridShape = (b2GridShape*)m_fixtureA->GetShape();
    int32 row = m_indexA / gridShape->m_columnCount;
    int32 col = m_indexA - (gridShape->m_columnCount * row);
    m_edgeMask = gridShape->CalculateCellMask(m_fixtureA, row, col);
}

void b2GridAndPolygonContact::Evaluate(b2Manifold* manifold, const b2Transform& xfA, const b2Transform& xfB)
{
    b2GridShape* gridShape = (b2GridShape*)m_fixtureA->GetShape();
    b2PolygonShape* polyB = (b2PolygonShape*)m_fixtureB->GetShape();

    const b2GridShape::Cell& cell = gridShape->m_cells[m_indexA];
    if (cell.m_Index == 0xffffffff)
    {
        return;
    }

    b2PolygonShape polyA;
    gridShape->GetPolygonShapeForCell(m_indexA, polyA);
    uint32 completeHull = ~0u;
    if (m_edgeMask == completeHull)
    {
        b2CollidePolygons(manifold, &polyA, xfA, polyB, xfB);
    }
    else
    {
        b2Manifold bestManifold = *manifold;
        float minDistance = b2_maxFloat;
        uint32 vc = (uint32)polyA.m_vertexCount;
        b2Vec2* v = polyA.m_vertices;
        uint32 vp = vc - 1;
        uint32 v0 = 0;
        uint32 v1 = 1;
        uint32 vn = 2;
        b2EdgeShape edge;
        for (uint32 i = 0; i < vc; ++i)
        {
            if (m_edgeMask & (1 << v0))
            {
                edge.Set(v[v0], v[v1]);
                edge.m_hasVertex0 = true;
                if (m_edgeMask & (1 << vp))
                {
                    edge.m_vertex0 = v[vp];
                }
                else
                {
                    edge.m_vertex0 = 2.0f * v[v0] - v[v1];
                }
                edge.m_hasVertex3 = true;
                if (m_edgeMask & (1 << v1))
                {
                    edge.m_vertex3 = v[vn];
                }
                else
                {
                    edge.m_vertex3 = 2.0f * v[v1] - v[v0];
                }
                b2CollideEdgeAndPolygon(manifold, &edge, xfA, polyB, xfB);
                int32 pc = manifold->pointCount;
                for (int32 j = 0; j < pc; ++j)
                {
                    float distance = manifold->points[j].distance;
                    if (distance < minDistance)
                    {
                        minDistance = distance;
                        bestManifold = *manifold;
                    }
                }
            }
            vp = v0;
            v0 = v1;
            v1 = vn;
            vn = (vn + 1) % vc;
        }
        *manifold = bestManifold;
    }
}
