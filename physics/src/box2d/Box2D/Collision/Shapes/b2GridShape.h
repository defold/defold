#ifndef B2_TILE_SHAPE_H
#define B2_TILE_SHAPE_H

#include <Box2D/Common/b2Math.h>
#include <Box2D/Collision/Shapes/b2Shape.h>
#include <Box2D/Dynamics/b2Body.h>
#include <string.h>

struct b2HullSet
{
    struct Hull
    {
        uint16 m_Index;
        uint16 m_Count;
    };

    b2HullSet(const b2Vec2* vertices, uint32 vertex_count,
              const Hull* hulls, uint32 hull_count)
    {
        uint32 vertices_size = vertex_count * sizeof(vertices[0]);
        m_vertices = (b2Vec2*) b2Alloc(vertices_size);
        memcpy(m_vertices, vertices, vertices_size);
        m_vertexCount = vertex_count;

        uint32 hulls_size = hull_count * sizeof(hulls[0]);
        m_hulls = (Hull*) b2Alloc(hulls_size);
        memcpy(m_hulls, hulls, hulls_size);
        m_hullCount = hull_count;
    }

    ~b2HullSet()
    {
        b2Free(m_vertices);
        b2Free(m_hulls);
    }

    b2Vec2* m_vertices;
    uint32  m_vertexCount;
    Hull*   m_hulls;
    uint32  m_hullCount;

private:
    b2HullSet(const b2HullSet&);
    b2HullSet& operator=(const b2HullSet&);
};

const uint32 B2GRIDSHAPE_EMPTY_CELL = 0xffffffff;

class b2GridShape : public b2Shape
{
public:
    b2GridShape(const b2HullSet* hullSet,
                const b2Vec2 position,
                uint32 cellWidth, uint32 cellHeight,
                uint32 rowCount, uint32 columnCount);

    virtual ~b2GridShape();

    virtual b2Shape* Clone(b2BlockAllocator* allocator) const;

    Type GetType() const;

    virtual int32 GetChildCount() const;

    virtual bool TestPoint(const b2Transform& xf, const b2Vec2& p) const;

    virtual bool RayCast(b2RayCastOutput* output, const b2RayCastInput& input,
                        const b2Transform& transform, int32 childIndex) const;

    virtual void ComputeAABB(b2AABB* aabb, const b2Transform& xf, int32 childIndex) const;

    virtual void ComputeMass(b2MassData* massData, float32 density) const;

    void GetPolygonShapeForCell(uint32 index, b2PolygonShape& polyShape) const;

    void SetCellHull(b2Body* body, uint32 row, uint32 column, uint32 hull);

    uint32 CalculateCellMask(b2Fixture* fixture, uint32 row, uint32 column);

    struct Cell
    {
        // Index to hull in hull-set
        // NOTE: If you add members to this struct memset(m_Cells, 0xff, size); in constructor must be replaced
        uint32 m_Index;
    };

    b2Vec2  m_position;
    Cell*   m_cells;
    const b2HullSet* m_hullSet;
    uint32  m_cellWidth;
    uint32  m_cellHeight;
    uint32  m_rowCount;
    uint32  m_columnCount;

private:
    uint32 GetCellVertices(uint32 index, b2Vec2* vertices) const;
};

#endif // B2_TILE_SHAPE_H
