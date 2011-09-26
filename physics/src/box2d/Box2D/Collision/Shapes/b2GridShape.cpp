#include <assert.h>
#include <string.h>
#include <Box2D/Common/b2Settings.h>
#include <Box2D/Collision/Shapes/b2GridShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Collision/b2BroadPhase.h>
#include <Box2D/Dynamics/b2Fixture.h>
#include <Box2D/Dynamics/b2World.h>
#include <new>
using namespace std;

b2GridShape::b2GridShape(const b2HullSet* hullSet,
                         const b2Vec2 position,
                         uint32 cellWidth, uint32 cellHeight,
                         uint32 rowCount, uint32 columnCount)
    : m_hullSet(hullSet),
      m_cellWidth(cellWidth), m_cellHeight(cellHeight),
      m_rowCount(rowCount), m_columnCount(columnCount)
{
    uint32 size = sizeof(Cell) * m_rowCount * m_columnCount;
    m_cells = (Cell*) b2Alloc(size);
    memset(m_cells, 0xff, size); // NOTE: This will set all Cell#m_Index to 0xffffffff

    m_position = position;
    m_type = e_grid;
    m_radius = b2_polygonRadius;
    m_filterPerChild = 1;
}

b2GridShape::~b2GridShape()
{
    b2Free(m_cells);
}

b2Shape* b2GridShape::Clone(b2BlockAllocator* allocator) const
{
    assert(false);
    return 0;
}

int32 b2GridShape::GetChildCount() const
{
    return m_rowCount * m_columnCount;
}

bool b2GridShape::TestPoint(const b2Transform& transform, const b2Vec2& p) const
{
    B2_NOT_USED(transform);
    B2_NOT_USED(p);
    // TestPoint should only be supported for convex shapes according to documentation
    return false;
}

bool b2GridShape::RayCast(b2RayCastOutput* output, const b2RayCastInput& input,
                            const b2Transform& transform, int32 childIndex) const
{
    b2PolygonShape polyShape;
    GetPolygonShapeForCell(childIndex, polyShape);
    return polyShape.RayCast(output, input, transform, childIndex);
}

void b2GridShape::ComputeAABB(b2AABB* aabb, const b2Transform& transform, int32 childIndex) const
{
    const b2GridShape::Cell& cell = m_cells[childIndex];
    if (cell.m_Index == B2GRIDSHAPE_EMPTY_CELL)
    {
        aabb->lowerBound = b2Vec2(FLT_MAX, FLT_MAX);
        aabb->upperBound = b2Vec2(-FLT_MAX, -FLT_MAX);
        return;
    }

    int row = childIndex / m_columnCount;
    int col = childIndex - m_columnCount * row;

    float32 halfWidth = m_cellWidth * m_columnCount * 0.5f;
    float32 halfHeight = m_cellHeight * m_rowCount * 0.5f;

    b2Vec2 min(m_cellWidth * col - halfWidth, m_cellHeight * row - halfHeight);
    b2Vec2 max(m_cellWidth * (col + 1) - halfWidth, m_cellHeight * (row + 1) - halfHeight);

    b2Vec2 v_min = b2Mul(transform, min);
    b2Vec2 v_max = b2Mul(transform, max);

    b2Vec2 lower = b2Min(v_min, v_max);
    b2Vec2 upper = b2Max(v_min, v_max);

    b2Vec2 r(m_radius, m_radius);
    aabb->lowerBound = lower - r + m_position;
    aabb->upperBound = upper + r + m_position;
}

void b2GridShape::ComputeMass(b2MassData* massData, float32 density) const
{
    // NOTE: Moment of inertia is approximated to a box
    // This is not correct but grid-shapes should only be used
    // for static geometry
    // This could be improved if required. Far fetched though.
    float32 w = m_cellHeight * m_rowCount;
    float32 h = m_cellWidth * m_columnCount;
    float32 area = w * h;

    massData->mass = area * density;
    massData->center = b2Vec2_zero;
    massData->I = massData->mass * (w * w + h * h + b2Dot(m_position, m_position)) / 12.0f;
}

void b2GridShape::GetPolygonShapeForCell(int index, b2PolygonShape& polyShape) const
{
    const b2GridShape::Cell& cell = m_cells[index];
    const b2HullSet::Hull& hull = m_hullSet->m_hulls[cell.m_Index];

    polyShape.Set(&m_hullSet->m_vertices[hull.m_Index], hull.m_Count);

    int row = index / m_columnCount;
    int col = index - (m_columnCount * row);

    float32 halfWidth = m_cellWidth * m_columnCount * 0.5f;
    float32 halfHeight = m_cellHeight * m_rowCount * 0.5f;

    b2Vec2 t(m_cellWidth * col - halfWidth, m_cellHeight * row - halfHeight);
    t.x += m_cellWidth * 0.5f;
    t.y += m_cellHeight * 0.5f;
    t += m_position;

    for (uint32 i = 0; i < hull.m_Count; ++i)
    {
        polyShape.m_vertices[i].x *= m_cellWidth;
        polyShape.m_vertices[i].y *= m_cellHeight;
        polyShape.m_vertices[i] += t;
    }
}

void b2GridShape::SetCellHull(b2Body* body, uint32 row, uint32 column, uint32 hull)
{
    assert(m_type == b2Shape::e_grid);

    uint32_t index = row * m_columnCount + column;
    b2Assert(index < m_rowCount * m_columnCount);
    b2GridShape::Cell* cell = &m_cells[index];
    cell->m_Index = hull;

    body->SynchronizeSingle(index);
}
