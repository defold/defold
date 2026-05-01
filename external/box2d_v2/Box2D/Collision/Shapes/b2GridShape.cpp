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
                         float32 cellWidth, float32 cellHeight,
                         uint32 rowCount, uint32 columnCount)
    : m_hullSet(hullSet),
      m_cellWidth(cellWidth), m_cellHeight(cellHeight),
      m_rowCount(rowCount), m_columnCount(columnCount),
      m_enabled(1)
{
    uint32 cellCount = m_rowCount * m_columnCount;
    uint32 size = sizeof(Cell) * cellCount;
    m_cells = (Cell*) b2Alloc(size);
    memset(m_cells, 0xff, size); // NOTE: This will set all Cell#m_Index to 0xffffffff
    size = sizeof(CellFlags) * cellCount;
    m_cellFlags = (CellFlags*) b2Alloc(size);
    memset(m_cellFlags, 0x0, size);

    m_position = position;
    m_type = e_grid;
    m_radius = b2_polygonRadius;
    m_filterPerChild = 1;
}

b2GridShape::~b2GridShape()
{
    b2Free(m_cells);
    b2Free(m_cellFlags);
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
    if (!m_enabled)
        return false;
    const b2GridShape::Cell& cell = m_cells[childIndex];
    if (cell.m_Index == B2GRIDSHAPE_EMPTY_CELL)
    {
        return false;
    }

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

    b2Vec2 halfDims(m_cellWidth * m_columnCount * 0.5f, m_cellHeight * m_rowCount * 0.5f);
    b2Vec2 offset = m_position - halfDims;

    float32 x0 = m_cellWidth * col - m_radius;
    float32 x1 = m_cellWidth * (col + 1) + m_radius;
    float32 y0 = m_cellHeight * row - m_radius;
    float32 y1 = m_cellHeight * (row + 1) + m_radius;

    b2Vec2 v00 = b2Mul(transform, b2Vec2(x0, y0) + offset);
    b2Vec2 v10 = b2Mul(transform, b2Vec2(x1, y0) + offset);
    b2Vec2 v01 = b2Mul(transform, b2Vec2(x0, y1) + offset);
    b2Vec2 v11 = b2Mul(transform, b2Vec2(x1, y1) + offset);

    b2Vec2 lower(b2Min(b2Min(v00.x, v01.x), b2Min(v10.x, v11.x)), b2Min(b2Min(v00.y, v01.y), b2Min(v10.y, v11.y)));
    b2Vec2 upper(b2Max(b2Max(v00.x, v01.x), b2Max(v10.x, v11.x)), b2Max(b2Max(v00.y, v01.y), b2Max(v10.y, v11.y)));

    aabb->lowerBound = lower;
    aabb->upperBound = upper;
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
    if (!m_enabled)
        return false;
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

    const b2GridShape::CellFlags& flags = m_cellFlags[index];
    float32 xScale = flags.m_FlipHorizontal ? -1.0f : 1.0f;
    float32 yScale = flags.m_FlipVertical ? -1.0f : 1.0f;

    float32 tmpX = 0.0;

    for (uint32 i = 0; i < hull.m_Count; ++i)
    {
        vertices[i] = m_hullSet->m_vertices[hull.m_Index + i];
        if (flags.m_Rotate90)
        {
            // Clockwise rotation (x, y) -> (y, -x)
            // Also tile isn't necessary a square, that's why we should swap width and height 
            // We apply flip before rotation, which means scale is part of size and should be swapped as well
            tmpX = vertices[i].x;
            vertices[i].x = vertices[i].y * (yScale * m_cellWidth);
            vertices[i].y = -1 * tmpX * (xScale * m_cellHeight);
        }
        else
        {
            vertices[i].x *= xScale * m_cellWidth;
            vertices[i].y *= yScale * m_cellHeight;
        }
        vertices[i] += t;
    }

    // Reverse order when single flipped
    if (flags.m_FlipHorizontal != flags.m_FlipVertical)
    {
        uint16 halfCount = hull.m_Count / 2;
        for (uint32 i = 0; i < halfCount; ++i)
        {
            b2Vec2& v1 = vertices[i];
            b2Vec2& v2 = vertices[hull.m_Count - 1 - i];
            b2Vec2 tmp = v1;
            v1 = v2;
            v2 = tmp;
        }
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

b2Vec2 b2GridShape::GetGhostPoint(uint32 index, b2Vec2 v0, b2Vec2 v1, bool fwdDirection) const
{
    int32 row = index / m_columnCount;
    int32 col = index - m_columnCount * row;
    b2Vec2 delta = v1 - v0;
    // the negated normal will point in the direction of the neighboring cell which has the ghost point in its edge
    b2Vec2 negNormal = b2Vec2(delta.y, -delta.x);
    b2Vec2 mag = b2Vec2(negNormal.x * negNormal.x, negNormal.y * negNormal.y);
    int32 dRow = 0;
    int32 dCol = 0;
    if (mag.x == b2Max(mag.x, mag.y))
    {
        // no b2Select
        if (negNormal.x >= 0.0f)
            dCol = 1;
        else
            dCol = -1;
    }
    else
    {
        // no b2Select
        if (negNormal.y >= 0.0f)
            dRow = 1;
        else
            dRow = -1;
    }
    row += dRow;
    col += dCol;

    uint32 neighborIndex = row * m_columnCount + col;
    const b2GridShape::Cell& cell = m_cells[neighborIndex];
    if (cell.m_Index == B2GRIDSHAPE_EMPTY_CELL)
    {
        if (fwdDirection)
        {
            return 2.0f * (v0 - v1);
        }
        else
        {
            return 2.0f * (v1 - v0);
        }
    }
    const b2HullSet::Hull& hull = m_hullSet->m_hulls[cell.m_Index];

    // find the closes hull point
    b2Vec2 vn[b2_maxPolygonVertices];
    uint32 vnCount = GetCellVertices(neighborIndex, vn);
    float minD = b2_maxFloat;
    b2Vec2 orig = fwdDirection ? v1 : v0;
    uint32 ghostIndex = 0;
    for (uint16 i = 0; i < hull.m_Count; ++i)
    {
        b2Vec2 diff = vn[i] - orig;
        float d = b2Dot(diff, diff);
        if (d < minD)
        {
            ghostIndex = i;
            minD = d;
        }
    }
    int32 deltaIndex = fwdDirection ? -1 : 1;
    return vn[(ghostIndex + vnCount + deltaIndex) % vnCount];
}

uint32 b2GridShape::GetEdgeShapesForCell(uint32 index, b2EdgeShape* edgeShapes, uint32 edgeShapeCount, uint32 edgeMask) const
{
    const b2GridShape::Cell& cell = m_cells[index];
    const b2HullSet::Hull& hull = m_hullSet->m_hulls[cell.m_Index];
    b2Assert(hull.m_Count <= b2_maxPolygonVertices);

    b2Vec2 v[b2_maxPolygonVertices];
    uint32 vCount = GetCellVertices(index, v);

    uint32 vPrev = vCount - 1;
    uint32 v0 = 0;
    uint32 v1 = 1;
    uint32 vNext = 2;
    uint32 edgeCount = 0;
    for (uint32 i = 0; i < vCount && i < edgeShapeCount; ++i)
    {
        if (edgeMask & (1 << v0))
        {
            b2EdgeShape& edge = edgeShapes[edgeCount];
            edge.Set(v[v0], v[v1]);
            edge.m_hasVertex0 = true;
            if (edgeMask & (1 << vPrev))
            {
                edge.m_vertex0 = v[vPrev];
            }
            else
            {
                edge.m_vertex0 = GetGhostPoint(index, v[vPrev], v[v0], true);
            }
            edge.m_hasVertex3 = true;
            if (edgeMask & (1 << v1))
            {
                edge.m_vertex3 = v[vNext];
            }
            else
            {
                edge.m_vertex3 = GetGhostPoint(index, v[v1], v[vNext], false);
            }
            ++edgeCount;
        }
        vPrev = v0;
        v0 = v1;
        v1 = vNext;
        vNext = (vNext + 1) % vCount;
    }
    return edgeCount;
}

static bool hasEdge(b2Vec2 p0, b2Vec2 p1, b2Vec2* vertices, uint32 n, float32 cellWidth, float32 cellHeight)
{
    const float32 epsilon = 0.01f;
    // Calculate absolute tolerance values by scaling epsilon with the magnitude
    // of the compared. We assume that the cell dimensions will be close to the max magnitude of the compared values
    float32 absTol = epsilon * b2Max(cellWidth, cellHeight);
    // Square it since we compare with squared distances below
    absTol *= absTol;

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

                    if (hasEdge(vertices[i2], vertices[i1], neighbourVertices, nnv, m_cellWidth, m_cellHeight))
                    {
                        mask &= ~(1 << i);
                    }
                }
            }
        }
    }

    return mask;
}

void b2GridShape::ClearCellData()
{
    uint32 cellCount = m_rowCount * m_columnCount;
    memset(m_cells, B2GRIDSHAPE_EMPTY_CELL, sizeof(Cell) * cellCount);
    memset(m_cellFlags, 0x0, sizeof(CellFlags) * cellCount);
}

void b2GridShape::SetCellHull(b2Body* body, uint32 row, uint32 column, uint32 hull, b2GridShape::CellFlags flags)
{
    assert(m_type == b2Shape::e_grid);

    uint32 index = row * m_columnCount + column;
    b2Assert(index < m_rowCount * m_columnCount);
    b2GridShape::Cell* cell = &m_cells[index];
    cell->m_Index = hull;
    m_cellFlags[index] = flags;
    // treat cells with an empty hull as an empty cell
    if (hull != B2GRIDSHAPE_EMPTY_CELL)
    {
        b2HullSet::Hull& h = m_hullSet->m_hulls[hull];
        if (h.m_Count == 0)
            cell->m_Index = B2GRIDSHAPE_EMPTY_CELL;
    }

    body->SynchronizeSingle(this, index);
}
