#include <assert.h>
#include <string.h>
#include <Box2D/Common/b2Settings.h>
#include <Box2D/Collision/Shapes/b2GridShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>
#include <Box2D/Collision/Shapes/b2EdgeShape.h>
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

uint32 b2GridShape::GetCellVertices(uint32 index, b2Vec2* vertices) const
{
    const b2GridShape::Cell& cell = m_cells[index];
    if (cell.m_Index == B2GRIDSHAPE_EMPTY_CELL)
        return 0;

    const b2HullSet::Hull& hull = m_hullSet->m_hulls[cell.m_Index];

    b2Assert(hull.m_Count <= b2_maxPolygonVertices);

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
        vertices[i] = m_hullSet->m_vertices[hull.m_Index + i];
        vertices[i].x *= m_cellWidth;
        vertices[i].y *= m_cellHeight;
        vertices[i] += t;
    }

    return hull.m_Count;
}

void b2GridShape::GetPolygonShapeForCell(uint32 index, b2PolygonShape& polyShape) const
{
    const b2GridShape::Cell& cell = m_cells[index];
    const b2HullSet::Hull& hull = m_hullSet->m_hulls[cell.m_Index];
    b2Assert(hull.m_Count <= b2_maxPolygonVertices);

    b2Vec2 vertices[b2_maxPolygonVertices];
    GetCellVertices(index, vertices);

    polyShape.Set(vertices, hull.m_Count);
    polyShape.m_radius = m_radius;
}

uint32 b2GridShape::GetEdgeShapesForCell(uint32 index, b2EdgeShape* edgeShapes, uint32 edgeShapeCount, uint32 edgeMask) const
{
    const b2GridShape::Cell& cell = m_cells[index];
    const b2HullSet::Hull& hull = m_hullSet->m_hulls[cell.m_Index];
    b2Assert(hull.m_Count <= b2_maxPolygonVertices);

    b2Vec2 v[b2_maxPolygonVertices];
    uint32 vc = GetCellVertices(index, v);

    uint32 vp = vc - 1;
    uint32 v0 = 0;
    uint32 v1 = 1;
    uint32 vn = 2;
    uint32 edgeCount = 0;
    for (uint32 i = 0; i < vc && i < edgeShapeCount; ++i)
    {
        if (edgeMask & (1 << v0))
        {
            b2EdgeShape& edge = edgeShapes[edgeCount];
            edge.Set(v[v0], v[v1]);
            edge.m_hasVertex0 = true;
            if (edgeMask & (1 << vp))
            {
                edge.m_vertex0 = v[vp];
            }
            else
            {
                edge.m_vertex0 = 2.0f * v[v0] - v[v1];
            }
            edge.m_hasVertex3 = true;
            if (edgeMask & (1 << v1))
            {
                edge.m_vertex3 = v[vn];
            }
            else
            {
                edge.m_vertex3 = 2.0f * v[v1] - v[v0];
            }
            ++edgeCount;
        }
        vp = v0;
        v0 = v1;
        v1 = vn;
        vn = (vn + 1) % vc;
    }
    return edgeCount;
}

static bool hasEdge(b2Vec2 p0, b2Vec2 p1, b2Vec2* vertices, uint32 n)
{
    const float32 epsilon = 0.01f;
    // Calculate absolute tolerance values by scaling epsilon with the magnitude
    // of the compared. We assume that b2Max(p0.x, p0.y) is closed to b2Max of all vertices compared
    const float32 absTol = epsilon * b2Max(1.0f, b2Max(p0.x, p0.y));

    for (uint32 i = 0; i < n; ++i)
    {
        uint32 i1 = i % n;
        uint32 i2 = (i+1) % n;
        float32 d1 = b2DistanceSquared(p0, vertices[i1]);
        float32 d2 = b2DistanceSquared(p1, vertices[i2]);

        if (d1 < absTol && d2 < absTol)
        {
            return true;
        }
    }

    return false;
}

uint32 b2GridShape::CalculateCellMask(b2Fixture* fixture, uint32 row, uint32 column)
{
    uint32 index = row * m_columnCount + column;
    const b2Filter& filter = fixture->GetFilterData(index);
    uint32 mask = 0xffffffff;

    b2Vec2 vertices[b2_maxPolygonVertices];
    b2Vec2 neighbourVertices[b2_maxPolygonVertices];

    uint32 nv = GetCellVertices(index, vertices);

    static const int32 deltas[4][2] = { {0, 1}, {1, 0}, {0, -1}, {-1, 0} };

    for (uint32 idelta = 0; idelta < 4; ++idelta)
    {
        int32 dr = deltas[idelta][0];
        int32 dc = deltas[idelta][1];

        int32 r = ((int32) row) + dr;
        int32 c = ((int32) column) + dc;
        if (r >= 0 && r < (int32) m_rowCount &&
            c >= 0 && c < (int32) m_columnCount)
        {
            uint32 neighbourIndex = r * m_columnCount + c;
            uint32 nnv = GetCellVertices(neighbourIndex, neighbourVertices);

            const b2Filter& neighbourFilter = fixture->GetFilterData(neighbourIndex);

            if (filter.categoryBits == neighbourFilter.categoryBits)
            {
                for (uint32 i = 0; i < nv; ++i)
                {
                    uint32 i1 = i % nv;
                    uint32 i2 = (i+1) % nv;

                    if (hasEdge(vertices[i2], vertices[i1], neighbourVertices, nnv))
                    {
                        mask &= ~(1 << i);
                    }
                }
            }
        }
    }

    return mask;
}

void b2GridShape::SetCellHull(b2Body* body, uint32 row, uint32 column, uint32 hull)
{
    assert(m_type == b2Shape::e_grid);

    uint32 index = row * m_columnCount + column;
    b2Assert(index < m_rowCount * m_columnCount);
    b2GridShape::Cell* cell = &m_cells[index];
    cell->m_Index = hull;

    body->SynchronizeSingle(index);
}
