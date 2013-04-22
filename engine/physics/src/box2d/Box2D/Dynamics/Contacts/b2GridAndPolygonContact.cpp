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

    manifold->pointCount = 0;

    const b2GridShape::Cell& cell = gridShape->m_cells[m_indexA];
    if (cell.m_Index == 0xffffffff)
    {
        return;
    }

    const uint32 completeHull = ~0u;
    // TODO Checking sensor here is a work-around for the budget edge-collisions below (see case 808)
    if (m_edgeMask == completeHull || m_fixtureB->IsSensor())
    {
        b2PolygonShape polyA;
        gridShape->GetPolygonShapeForCell(m_indexA, polyA);
        b2CollidePolygons(manifold, &polyA, xfA, polyB, xfB);
    }
    else
    {
        b2Manifold bestManifold = *manifold;
        bestManifold.pointCount = 0;
        float minDistance = b2_maxFloat;
        b2EdgeShape edgeShapes[b2_maxPolygonVertices];
        uint32 edgeCount = gridShape->GetEdgeShapesForCell(m_indexA, edgeShapes, b2_maxPolygonVertices, m_edgeMask);
        for (uint32 i = 0; i < edgeCount; ++i)
        {
            b2EdgeShape& edge = edgeShapes[i];
            manifold->pointCount = 0;
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
        *manifold = bestManifold;
    }
}
