// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

#include <atlaspacker/convexhull.h>
#include <atlaspacker/atlaspacker.h>

#include <stdlib.h> // malloc
#include <stdio.h> // printf
#include <math.h> // cosf/sinf
#ifndef  M_PI
#define  M_PI  3.1415926535897932384626433
#endif

static const uint8_t TILE_VALUE_DEFAULT = 1;
static const uint8_t TILE_VALUE_VISITED = 2;

typedef struct
{
    int num_planes;
    apPosf* normals;
    float* distances;   // plane distances
} apConvexHull;


static int apConvexHullCalculatePlanes(apConvexHull* hull, uint8_t* image, int width, int height)
{
    int empty = 1;

    int num_planes = hull->num_planes;
    for (int i = 0; i < num_planes; ++i)
    {
        hull->distances[i] = -1000000.0f;

        float angle = i * 2.0f * M_PI / (float)num_planes;
        hull->normals[i].x = cosf(angle);
        hull->normals[i].y = sinf(angle);
        hull->normals[i] = apMathNormalize(hull->normals[i]);
    }

    apPos center = { width / 2.0f, height / 2.0f };
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (!image[y * width + x])
                continue;

            empty = 0;

            apPosf pos = { x + 0.5f - center.x, y + 0.5f - center.y };
            for (int p = 0; p < num_planes; ++p)
            {
                apPosf dir = hull->normals[p];

                float distance = apMathDot(dir, pos);
                if (distance > hull->distances[p])
                    hull->distances[p] = distance;
            }
        }
    }

    if (!empty)
    {
        // At this point, each place is centered at the middle of a texel
        // but we need it to contain the full texel corners

        apPosf corners[4] ={{ -0.5f, -0.5f },
                            { +0.5f, -0.5f },
                            { +0.5f, +0.5f },
                            { -0.5f, +0.5f }};

        for (int p = 0; p < num_planes; ++p)
        {
            apPosf dir = hull->normals[p];

            float max_distance = 0;
            for (int c = 0; c < 4; ++c)
            {
                float distance = apMathDot(dir, corners[c]);
                if (distance > max_distance)
                    max_distance = distance;
            }
            hull->distances[p] += max_distance;
        }
    }

    return empty ? 0 : 1;
}

static inline apPosf apConvexHullCalculatePoint(apPosf dir, float distance)
{
    apPosf p = { dir.x * distance, dir.y * distance};
    return p;
}

static inline float apConvexHullRoundEdge(float v)
{
    if (v >= -0.5f && v <= 0.5f)
        return v;
    const float epsilon = 0.001f;
    if (v < -0.5f && (v + 0.5f) > -epsilon)
        return -0.5f;
    if (v > 0.5f && (v - 0.5f) < epsilon)
        return 0.5f;
    return v; // outside of [-0.5, 0.5] range
}

static apPosf* apConvexHullCalculateVertices(apConvexHull* hull, int width, int height)
{
    int num_planes = hull->num_planes;

    apPosf* vertices = (apPosf*)malloc(sizeof(apPosf)*num_planes);

    float half_width = width/2.0f;
    float half_height = height/2.0f;

    for (int i = 0; i < num_planes; ++i)
    {
        int j = (i+1)%num_planes;

        // in texel space
        apPosf p0 = apConvexHullCalculatePoint(hull->normals[i], hull->distances[i]);
        apPosf p1 = apConvexHullCalculatePoint(hull->normals[j], hull->distances[j]);

        // directions for the lines
        apPosf d0 = { -hull->normals[i].y, hull->normals[i].x };
        apPosf d1 = { -hull->normals[j].y, hull->normals[j].x };

        // Calculate the line intersection
        float t = ((p0.y - p1.y) * d1.x - (p0.x - p1.x) * d1.y) / (d0.x * d1.y - d1.x * d0.y);
        apPosf vertex = { p0.x + d0.x * t, p0.y + d0.y * t };

        // to [(-0.5,-0.5), (0.5,0.5)]
        vertex.x = (vertex.x / half_width) * 0.5f;
        vertex.y = (vertex.y / half_height) * 0.5f;

        vertex.x = apConvexHullRoundEdge(vertex.x);
        vertex.y = apConvexHullRoundEdge(vertex.y);

        vertices[num_planes - i - 1] = vertex;
    }

    return vertices;
}

apPosf* apConvexHullFromImage(int num_planes, uint8_t* image, int width, int height, int* num_vertices)
{
    apConvexHull hull;
    hull.num_planes = num_planes;
    hull.normals = (apPosf*)malloc(sizeof(apPosf)*num_planes);
    hull.distances = (float*)malloc(sizeof(float)*num_planes);

    int valid = apConvexHullCalculatePlanes(&hull, image, width, height);
    if (!valid)
        return 0;

    apPosf* vertices = apConvexHullCalculateVertices(&hull, width, height);

    free((void*)hull.normals);
    free((void*)hull.distances);

    *num_vertices = num_planes; // until we simplify the hull
    return vertices;
}

// ************************************************************************************************************************

// Checks if the box can grow by one to the right and/or bottom
static int CanGrowBoxByOne(int width, int height, uint8_t* data, int bx, int by, int bwidth, int bheight, int* growx, int* growy)
{
    *growx = 0;
    *growy = 0;
    int x = bx + bwidth;
    if (x < width)
    {
        *growx = 1;
        for (int y = by; y < (by + bheight) && y < height; ++y)
        {
            if (data[y*width+x] == 1)
                continue;

             // if we find a zero, we can't expand the box in this direction
            *growx = 0;
            break;
        }
    }

    int y = by + bheight;
    if (y < height)
    {
        *growy = 1;
        for (int x = bx; x < (bx + bwidth) && x < width; ++x)
        {
            if (data[y*width+x] == 1)
                continue;

             // if we find a zero, we can't expand the box in this direction
            *growy = 0;
            break;
        }
    }

    int corner_x = bx+bwidth;
    int corner_y = by+bheight;
    int corner = (corner_x < width && corner_y < height) ? data[corner_y*width + corner_x] != 0 : 0;

    // It's possible the diagonal is not set, but we can expand in both x or y
    if (!corner && (*growx & *growy))
    {
        *growy = 0;
    }

    //printf("CanGrowBoxByOne: %d %d  %d %d -> %d %d\n", x, y, bwidth, bheight, *growx, *growy);

    return *growx | *growy;
}

static void ExpandBox(int width, int height, uint8_t* data, int bx, int by, int* out_bwidth, int* out_bheight)
{
    int bwidth = 1;
    int bheight = 1;
    while (1)
    {
        int growx;
        int growy;
        if (!CanGrowBoxByOne(width, height, data, bx, by, bwidth, bheight, &growx, &growy))
            break;

        bwidth += growx;
        bheight += growy;
    }
    *out_bwidth = bwidth;
    *out_bheight = bheight;
}

static void ClearBox(int width, int height, uint8_t* data, int bx, int by, int bwidth, int bheight)
{
    int maxx = apMathMin(width, bx+bwidth);
    int maxy = apMathMin(height, by+bheight);
    for (int y = by; y < maxy; ++y)
    {
        for (int x = bx; x < maxx; ++x)
        {
            data[y*width+x] = TILE_VALUE_VISITED;
        }
    }
}

static void ResetImage(int width, int height, uint8_t* data)
{
    for (int i = 0; i < width*height; ++i)
    {
        if (data[i])
            data[i] = TILE_VALUE_DEFAULT;
    }
}


// Takes a bitmap where 0 is empty, and 1 is occupied
// Modifies the bitmap (clears bits to 0 to designate the traversal being done)
void apHullFindLargestBoxes(int width, int height, uint8_t* data, APHullBoxCallback cbk, void* cbk_ctx)
{
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            if (data[y*width+x] == TILE_VALUE_DEFAULT)
            {
                int bwidth;
                int bheight;
                ExpandBox(width, height, data, x, y, &bwidth, &bheight);
                ClearBox(width, height, data, x, y, bwidth, bheight);
                cbk(cbk_ctx, x, y, bwidth, bheight);
            }
        }
    }
    ResetImage(width, height, data);
}

typedef struct
{
    int capacity;
    int size;
    void* mem;

    float width;
    float height;
} ContourCreateContext;

static void BoxToVertices(void* _ctx, int x, int y, int width, int height)
{
    ContourCreateContext* ctx = (ContourCreateContext*)_ctx;
    //printf("  BOX: %d, %d %d, %d\n", x, y, width, height);

    if (ctx->size + 2 >= ctx->capacity)
    {
        ctx->capacity += 6 * 4; // each box: 2 triangles = 6 vertices. Preallocate some boxes
        ctx->mem = realloc(ctx->mem, ctx->capacity * sizeof(apPosf));
    }

    float fwidth = (float)ctx->width;
    float fheight = (float)ctx->height;

    apPosf* vertices = (apPosf*)ctx->mem;

    apPosf box[4] = {
        { x,         y },
        { x + width, y },
        { x + width, y + height },
        { x,         y + height }
    };

    vertices[ctx->size++] = box[0];
    vertices[ctx->size++] = box[1];
    vertices[ctx->size++] = box[2];
    vertices[ctx->size++] = box[0];
    vertices[ctx->size++] = box[2];
    vertices[ctx->size++] = box[3];
}

apPosf* apHullFromImage(uint8_t* image, int width, int height, int* num_vertices)
{
    ContourCreateContext ctx;
    ctx.size = 0;
    ctx.capacity = 0;
    ctx.mem = 0;
    ctx.width = (float)width;
    ctx.height = (float)height;
    apHullFindLargestBoxes(width, height, image, BoxToVertices, &ctx);
    *num_vertices = ctx.size;
    return ctx.mem;
}

apPosf* apCreateBoxVertices(apPos pos, apSize size, int* num_vertices)
{
    apPosf* vertices = (apPosf*)malloc(sizeof(apPosf)*6);
    *num_vertices = 6;

    apPosf box[4] = {
        { pos.x,                pos.y },
        { pos.x + size.width,   pos.y },
        { pos.x + size.width,   pos.y + size.height },
        { pos.x,                pos.y + size.height }
    };

    vertices[0] = box[0];
    vertices[1] = box[1];
    vertices[2] = box[2];
    vertices[3] = box[0];
    vertices[4] = box[2];
    vertices[5] = box[3];
    return vertices;
}