#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

#include <simd/simd.h>
#include <vector>
#include <string.h>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../engine/font/src/external/stb_truetype.h"

struct FeatherUniforms
{
    vector_float2 m_Resolution;
    float         m_FeatherRadius;
    float         m_Padding0;
    vector_float4 m_BackgroundColor;
    vector_float4 m_ShapeColor;
};

struct HarnessVertex
{
    vector_float2 m_Position;
    vector_float2 m_UV;
};

struct FeatherPatchVertex
{
    float         m_LocalVertexID;
    float         m_Outset;
    float         m_FillCoverage;
    float         m_Params;
    float         m_MirroredLocalVertexID;
    float         m_MirroredOutset;
    float         m_MirroredFillCoverage;
    int32_t       m_Padding;
    vector_float2 m_PositionPx;
    vector_float4 m_PatchData;
    vector_float4 m_MirroredPatchData;
    vector_uint2  m_Indices;
};

enum PatchRole : uint32_t
{
    kPatchRoleStroke = 0u,
    kPatchRoleFan = 1u,
    kPatchRoleFanMidpoint = 2u
};

struct QuadraticRecord
{
    float m_P0X;
    float m_P0Y;
    float m_P1X;
    float m_P1Y;
    float m_P2X;
    float m_P2Y;
};

struct CubicRecord
{
    float m_P0X;
    float m_P0Y;
    float m_P1X;
    float m_P1Y;
    float m_P2X;
    float m_P2Y;
    float m_P3X;
    float m_P3Y;
    uint32_t m_SourceSegmentId;
};

struct CornerRecord
{
    float m_X;
    float m_Y;
    float m_T0X;
    float m_T0Y;
    float m_T1X;
    float m_T1Y;
    float m_OrientationSign;
};

struct EdgeRecord
{
    float m_X;
    float m_Y;
    float m_Tx;
    float m_Ty;
    float m_Nx;
    float m_Ny;
    float m_Length;
    float m_ContourMidX;
    float m_ContourMidY;
    uint32_t m_SourceCurveIndex;
};

struct GlyphPoint
{
    float m_X;
    float m_Y;
};

struct RiveContourDataRecord
{
    uint32_t m_MidpointXBits;
    uint32_t m_MidpointYBits;
    uint32_t m_PathID;
    uint32_t m_VertexIndex0;
};

struct RivePathDataRecord
{
    uint32_t m_Matrix0Bits;
    uint32_t m_Matrix1Bits;
    uint32_t m_Matrix2Bits;
    uint32_t m_Matrix3Bits;
    uint32_t m_TranslateXBits;
    uint32_t m_TranslateYBits;
    uint32_t m_StrokeRadiusBits;
    uint32_t m_FeatherRadiusBits;
    uint32_t m_PathID;
    uint32_t m_ZIndex;
    uint32_t m_Padding0;
    uint32_t m_Padding1;
    uint32_t m_Padding2;
    uint32_t m_Padding3;
    uint32_t m_Padding4;
    uint32_t m_Padding5;
};

struct RiveTessVertexRecord
{
    uint32_t m_P0XBits;
    uint32_t m_P0YBits;
    uint32_t m_P1XBits;
    uint32_t m_P1YBits;
    uint32_t m_P2XBits;
    uint32_t m_P2YBits;
    uint32_t m_P3XBits;
    uint32_t m_P3YBits;
    uint32_t m_JoinTangentXBits;
    uint32_t m_JoinTangentYBits;
    uint32_t m_YBits;
    uint32_t m_ReflectionYBits;
    uint32_t m_X0X1;
    uint32_t m_ReflectionX0X1;
    uint32_t m_SegmentCounts;
    uint32_t m_ContourIDWithFlags;
};

struct RivePatchVertex
{
    float   m_LocalVertexID;
    float   m_Outset;
    float   m_FillCoverage;
    int32_t m_Params;
    float   m_MirroredVertexID;
    float   m_MirroredOutset;
    float   m_MirroredFillCoverage;
    int32_t m_Padding = 0;
};

static_assert(sizeof(RivePatchVertex) == sizeof(float) * 8, "Unexpected Rive patch vertex size");

static float PackParamsAsFloat(uint32_t patch_segment_span, uint32_t vertex_type);
static int32_t PackParamsAsInt(uint32_t patch_segment_span, uint32_t vertex_type);
static float UIntBitsToFloat(uint32_t value);
static FeatherPatchVertex MakePatchVertex(float local_vertex_id,
                                          float outset,
                                          float fill_coverage,
                                          float params,
                                          float mirrored_local_vertex_id,
                                          float mirrored_outset,
                                          float mirrored_fill_coverage,
                                          float mirrored_padding,
                                          vector_float2 position_px,
                                          vector_float4 patch_data,
                                          vector_float4 mirrored_patch_data,
                                          uint32_t index0 = 0u,
                                          uint32_t index1 = 0u);

static void GenerateRivePatchBufferData(RivePatchVertex vertices[],
                                        uint16_t indices[],
                                        uint32_t patch_segment_span,
                                        bool midpoint_fan_center_aa,
                                        bool outer_curves)
{
    size_t vertex_count = 0;
    uint32_t patchSegmentSpan = outer_curves ? 17u : patch_segment_span;
    for (uint32_t i = 0; i < patchSegmentSpan; ++i)
    {
        int32_t params = PackParamsAsInt(patchSegmentSpan, kPatchRoleStroke);
        float l = (float)i;
        float r = l + 1.0f;
        if (outer_curves)
        {
            vertices[vertex_count + 0] = { l, 0.0f, 0.5f, params, r, 0.0f, 0.5f, 0 };
            vertices[vertex_count + 1] = { l, 1.0f, 0.0f, params, l, 1.0f, 0.0f, 0 };
            vertices[vertex_count + 2] = { r, 0.0f, 0.5f, params, r, 0.0f, 0.5f, 0 };
            vertices[vertex_count + 3] = { r, 1.0f, 0.0f, params, l, 1.0f, 0.0f, 0 };
        }
        else if (midpoint_fan_center_aa)
        {
            vertices[vertex_count + 0] = { l, 0.0f, 0.5f, params, r - 1.0f, 0.0f, 0.5f, 0 };
            vertices[vertex_count + 1] = { l, 1.0f, 0.0f, params, l - 1.0f, 1.0f, 0.0f, 0 };
            vertices[vertex_count + 2] = { r, 0.0f, 0.5f, params, r - 1.0f, 0.0f, 0.5f, 0 };
            vertices[vertex_count + 3] = { r, 1.0f, 0.0f, params, l - 1.0f, 1.0f, 0.0f, 0 };
        }
        else
        {
            vertices[vertex_count + 0] = { l, -1.0f, 1.0f, params, r - 1.0f, -1.0f, 1.0f, 0 };
            vertices[vertex_count + 1] = { l,  1.0f, 0.0f, params, l - 1.0f,  1.0f, 0.0f, 0 };
            vertices[vertex_count + 2] = { r, -1.0f, 1.0f, params, r - 1.0f, -1.0f, 1.0f, 0 };
            vertices[vertex_count + 3] = { r,  1.0f, 0.0f, params, l - 1.0f,  1.0f, 0.0f, 0 };
        }
        vertex_count += 4;
    }

    size_t fan_vertices_idx = vertex_count;
    size_t fan_segment_span = outer_curves ? patchSegmentSpan - 1 : patchSegmentSpan;
    assert((fan_segment_span & (fan_segment_span - 1)) == 0);
    for (uint32_t i = 0; i <= fan_segment_span; ++i)
    {
        int32_t params = PackParamsAsInt(patchSegmentSpan, kPatchRoleFan);
        if (outer_curves)
        {
            vertices[vertex_count] = { (float)i, 0.0f, 1.0f, params, (float)i, 0.0f, 1.0f, 0 };
        }
        else if (midpoint_fan_center_aa)
        {
            vertices[vertex_count] = { (float)i, 0.0f, 1.0f, params, (float)i - 1.0f, 0.0f, 1.0f, 0 };
        }
        else
        {
            vertices[vertex_count] = { (float)i, -1.0f, 1.0f, params, (float)i - 1.0f, -1.0f, 1.0f, 0 };
        }
        ++vertex_count;
    }

    size_t midpoint_idx = vertex_count;
    if (!outer_curves)
    {
        vertices[vertex_count++] = { 0.0f, 0.0f, 1.0f, PackParamsAsInt(patchSegmentSpan, kPatchRoleFanMidpoint), 0.0f, 0.0f, 1.0f, 0 };
    }

    constexpr size_t kBorderPatternIndexCount = 6;
    constexpr uint16_t kBorderPattern[kBorderPatternIndexCount] = {0, 1, 2, 2, 1, 3};
    constexpr uint16_t kNegativeBorderPattern[kBorderPatternIndexCount] = {0, 2, 1, 1, 2, 3};

    size_t index_count = 0;
    size_t border_edge_vertices_idx = 0;
    for (uint32_t border_segment_idx = 0; border_segment_idx < patchSegmentSpan; ++border_segment_idx)
    {
        for (size_t i = 0; i < kBorderPatternIndexCount; ++i)
        {
            indices[index_count++] = (uint16_t)(border_edge_vertices_idx + kBorderPattern[i]);
        }
        border_edge_vertices_idx += 4;
    }

    if (!outer_curves)
    {
        for (uint32_t border_segment_idx = 0; border_segment_idx < patchSegmentSpan; ++border_segment_idx)
        {
            for (size_t i = 0; i < kBorderPatternIndexCount; ++i)
            {
                indices[index_count++] = (uint16_t)(border_edge_vertices_idx + kNegativeBorderPattern[i]);
            }
            border_edge_vertices_idx += 4;
        }
    }

    for (uint32_t step = 1; step < fan_segment_span; step <<= 1)
    {
        for (uint32_t i = 0; i < fan_segment_span; i += step * 2)
        {
            indices[index_count++] = (uint16_t)(fan_vertices_idx + i);
            indices[index_count++] = (uint16_t)(fan_vertices_idx + i + step);
            indices[index_count++] = (uint16_t)(fan_vertices_idx + i + step * 2);
        }
    }

    if (!outer_curves)
    {
        indices[index_count++] = (uint16_t)fan_vertices_idx;
        indices[index_count++] = (uint16_t)(fan_vertices_idx + fan_segment_span);
        indices[index_count++] = (uint16_t)midpoint_idx;
    }
}

static void GenerateRivePatchBufferData(std::vector<RivePatchVertex>& vertices,
                                        std::vector<uint16_t>& indices)
{
    constexpr uint32_t kMidpointFanPatchSegmentSpan = 8u;
    constexpr uint32_t kOuterCurvePatchSegmentSpan = 17u;

    constexpr uint32_t kMidpointFanPatchVertexCount =
        kMidpointFanPatchSegmentSpan * 4u +
        (kMidpointFanPatchSegmentSpan + 1u) +
        1u;
    constexpr uint32_t kMidpointFanPatchBorderIndexCount =
        kMidpointFanPatchSegmentSpan * 6u;
    constexpr uint32_t kMidpointFanPatchIndexCount =
        kMidpointFanPatchBorderIndexCount +
        (kMidpointFanPatchSegmentSpan - 1u) * 3u +
        3u;

    constexpr uint32_t kMidpointFanCenterAAPatchVertexCount =
        kMidpointFanPatchSegmentSpan * 8u +
        (kMidpointFanPatchSegmentSpan + 1u) +
        1u;
    constexpr uint32_t kMidpointFanCenterAAPatchBorderIndexCount =
        kMidpointFanPatchSegmentSpan * 12u;
    constexpr uint32_t kMidpointFanCenterAAPatchIndexCount =
        kMidpointFanCenterAAPatchBorderIndexCount +
        (kMidpointFanPatchSegmentSpan - 1u) * 3u +
        3u;

    constexpr uint32_t kOuterCurvePatchVertexCount =
        kOuterCurvePatchSegmentSpan * 8u +
        kOuterCurvePatchSegmentSpan;
    constexpr uint32_t kOuterCurvePatchBorderIndexCount =
        kOuterCurvePatchSegmentSpan * 12u;
    constexpr uint32_t kOuterCurvePatchIndexCount =
        kOuterCurvePatchBorderIndexCount +
        (kOuterCurvePatchSegmentSpan - 2u) * 3u;

    vertices.clear();
    indices.clear();
    vertices.resize(kMidpointFanPatchVertexCount +
                    kMidpointFanCenterAAPatchVertexCount +
                    kOuterCurvePatchVertexCount);
    indices.resize(kMidpointFanPatchIndexCount +
                   kMidpointFanCenterAAPatchIndexCount +
                   kOuterCurvePatchIndexCount);

    GenerateRivePatchBufferData(vertices.data(),
                                indices.data(),
                                kMidpointFanPatchSegmentSpan,
                                false,
                                false);
    GenerateRivePatchBufferData(vertices.data() + kMidpointFanPatchVertexCount,
                                indices.data() + kMidpointFanPatchIndexCount,
                                kMidpointFanPatchSegmentSpan,
                                true,
                                false);
    GenerateRivePatchBufferData(vertices.data() + kMidpointFanPatchVertexCount +
                                kMidpointFanCenterAAPatchVertexCount,
                                indices.data() + kMidpointFanPatchIndexCount +
                                kMidpointFanCenterAAPatchIndexCount,
                                kOuterCurvePatchSegmentSpan,
                                false,
                                true);
}

static float UIntBitsToFloat(uint32_t value)
{
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float TriangleOrientationSign(const GlyphPoint& a,
                                     const GlyphPoint& b,
                                     const GlyphPoint& c);

static float CpuApproxErf(float x)
{
    float sign_x = x < 0.0f ? -1.0f : 1.0f;
    float ax = fabsf(x);
    float xx = ax * ax;
    float a = 0.147f;
    float inner = 1.0f - expf(-xx * (4.0f / (float)M_PI + a * xx) / (1.0f + a * xx));
    return sign_x * sqrtf(fmaxf(0.0f, inner));
}

static float CpuNormalCDF(float x)
{
    const float inv_sqrt2 = 0.7071067811865475f;
    return 0.5f * (1.0f + CpuApproxErf(x * inv_sqrt2));
}

static NSString* ResolveShaderPath(const char* argv0)
{
    NSString* explicit_path = @"feather_shader.metal";
    if ([[NSFileManager defaultManager] fileExistsAtPath:explicit_path])
    {
        return explicit_path;
    }

    NSString* executable = [[NSString stringWithUTF8String:argv0] stringByStandardizingPath];
    NSString* executable_dir = [executable stringByDeletingLastPathComponent];

    NSArray<NSString*>* candidates =
    @[
        [executable_dir stringByAppendingPathComponent:@"feather_shader.metal"],
        [[executable_dir stringByAppendingPathComponent:@".."] stringByAppendingPathComponent:@"feather_shader.metal"],
        [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:@"feather_shader.metal"]
    ];

    for (NSString* candidate in candidates)
    {
        NSString* standardized = [candidate stringByStandardizingPath];
        if ([[NSFileManager defaultManager] fileExistsAtPath:standardized])
        {
            return standardized;
        }
    }

    return nil;
}

static NSString* ResolveFontPath(void)
{
    NSString* explicit_path = @"Roboto-Medium.ttf";
    if ([[NSFileManager defaultManager] fileExistsAtPath:explicit_path])
    {
        return explicit_path;
    }

    NSString* cwd_candidate = [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:@"Roboto-Medium.ttf"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:cwd_candidate])
    {
        return cwd_candidate;
    }

    return nil;
}

static void PrintUsage(const char* argv0)
{
    printf("Usage: %s [--interactive] [--output path] [--width px] [--height px] [--feather radius]\n", argv0);
    printf("Defaults:\n");
    printf("  --output      build/l_shape.png\n");
    printf("  --width       512\n");
    printf("  --height      512\n");
    printf("  --feather     28\n");
    printf("  --interactive false\n");
    printf("\n");
    printf("Interactive controls:\n");
    printf("  Left/Down     decrease feather\n");
    printf("  Right/Up      increase feather\n");
    printf("  Mouse drag    adjust feather horizontally\n");
    printf("  S             save a PNG with a unique iteration name\n");
}

static bool WritePNG(NSString* output_path, const uint8_t* rgba, uint32_t width, uint32_t height)
{
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    if (!color_space)
    {
        return false;
    }

    size_t bytes_per_row = width * 4;
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, rgba, bytes_per_row * height);
    if (!data)
    {
        CGColorSpaceRelease(color_space);
        return false;
    }

    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    if (!provider)
    {
        CFRelease(data);
        CGColorSpaceRelease(color_space);
        return false;
    }

    CGImageRef image = CGImageCreate(width,
                                     height,
                                     8,
                                     32,
                                     bytes_per_row,
                                     color_space,
                                     kCGImageAlphaPremultipliedLast | kCGBitmapByteOrderDefault,
                                     provider,
                                     nullptr,
                                     false,
                                     kCGRenderingIntentDefault);
    if (!image)
    {
        CGDataProviderRelease(provider);
        CFRelease(data);
        CGColorSpaceRelease(color_space);
        return false;
    }

    NSURL* url = [NSURL fileURLWithPath:output_path];
    CGImageDestinationRef destination = CGImageDestinationCreateWithURL((CFURLRef)url, CFSTR("public.png"), 1, nullptr);
    if (!destination)
    {
        CGImageRelease(image);
        CGDataProviderRelease(provider);
        CFRelease(data);
        CGColorSpaceRelease(color_space);
        return false;
    }

    CGImageDestinationAddImage(destination, image, nullptr);
    bool ok = CGImageDestinationFinalize(destination);

    CFRelease(destination);
    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CFRelease(data);
    CGColorSpaceRelease(color_space);
    return ok;
}

static NSString* MakeNextIterationPath(NSString* template_path)
{
    NSString* directory = [template_path stringByDeletingLastPathComponent];
    NSString* filename = [[template_path lastPathComponent] stringByDeletingPathExtension];
    NSString* extension = [template_path pathExtension];
    if (extension.length == 0)
    {
        extension = @"png";
    }

    NSFileManager* fm = [NSFileManager defaultManager];
    [fm createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];

    for (int i = 1; i < 10000; ++i)
    {
        NSString* candidate =
            [directory stringByAppendingPathComponent:
                [NSString stringWithFormat:@"%@_iter_%03d.%@", filename, i, extension]];
        if (![fm fileExistsAtPath:candidate])
        {
            return candidate;
        }
    }

    return [directory stringByAppendingPathComponent:
        [NSString stringWithFormat:@"%@_iter_overflow.%@", filename, extension]];
}

static GlyphPoint MakeGlyphPoint(float pen_x, float x, float y, float scale)
{
    GlyphPoint p;
    p.m_X = pen_x + x * scale;
    p.m_Y = -y * scale;
    return p;
}

static void AppendQuadratic(std::vector<QuadraticRecord>& quadratics,
                            const GlyphPoint& p0,
                            const GlyphPoint& p1,
                            const GlyphPoint& p2)
{
    QuadraticRecord quad;
    quad.m_P0X = p0.m_X;
    quad.m_P0Y = p0.m_Y;
    quad.m_P1X = p1.m_X;
    quad.m_P1Y = p1.m_Y;
    quad.m_P2X = p2.m_X;
    quad.m_P2Y = p2.m_Y;
    quadratics.push_back(quad);
}

static void AppendCubic(std::vector<CubicRecord>& cubics,
                        const GlyphPoint& p0,
                        const GlyphPoint& p1,
                        const GlyphPoint& p2,
                        const GlyphPoint& p3,
                        uint32_t source_segment_id)
{
    CubicRecord cubic;
    cubic.m_P0X = p0.m_X;
    cubic.m_P0Y = p0.m_Y;
    cubic.m_P1X = p1.m_X;
    cubic.m_P1Y = p1.m_Y;
    cubic.m_P2X = p2.m_X;
    cubic.m_P2Y = p2.m_Y;
    cubic.m_P3X = p3.m_X;
    cubic.m_P3Y = p3.m_Y;
    cubic.m_SourceSegmentId = source_segment_id;
    cubics.push_back(cubic);
}

static void AppendQuadraticAsCubic(std::vector<CubicRecord>& cubics,
                                   const GlyphPoint& p0,
                                   const GlyphPoint& p1,
                                   const GlyphPoint& p2,
                                   uint32_t source_segment_id)
{
    GlyphPoint c1 = {
        p0.m_X + (p1.m_X - p0.m_X) * (2.0f / 3.0f),
        p0.m_Y + (p1.m_Y - p0.m_Y) * (2.0f / 3.0f)
    };
    GlyphPoint c2 = {
        p2.m_X + (p1.m_X - p2.m_X) * (2.0f / 3.0f),
        p2.m_Y + (p1.m_Y - p2.m_Y) * (2.0f / 3.0f)
    };
    AppendCubic(cubics, p0, c1, c2, p2, source_segment_id);
}

static GlyphPoint Lerp(const GlyphPoint& a, const GlyphPoint& b, float t)
{
    GlyphPoint p;
    p.m_X = a.m_X + (b.m_X - a.m_X) * t;
    p.m_Y = a.m_Y + (b.m_Y - a.m_Y) * t;
    return p;
}

static GlyphPoint MakePoint(float x, float y)
{
    GlyphPoint p;
    p.m_X = x;
    p.m_Y = y;
    return p;
}

static GlyphPoint Normalize(const GlyphPoint& v)
{
    float len_sq = v.m_X * v.m_X + v.m_Y * v.m_Y;
    if (len_sq <= 1e-8f)
    {
        return MakePoint(0.0f, 0.0f);
    }
    float inv_len = 1.0f / sqrtf(len_sq);
    return MakePoint(v.m_X * inv_len, v.m_Y * inv_len);
}

static GlyphPoint LeftNormal(const GlyphPoint& v)
{
    return MakePoint(-v.m_Y, v.m_X);
}

static GlyphPoint RightNormal(const GlyphPoint& v)
{
    return MakePoint(v.m_Y, -v.m_X);
}

static GlyphPoint QuadraticStartTangent(const QuadraticRecord& q)
{
    return Normalize(MakePoint(q.m_P1X - q.m_P0X, q.m_P1Y - q.m_P0Y));
}

static GlyphPoint QuadraticEndTangent(const QuadraticRecord& q)
{
    return Normalize(MakePoint(q.m_P2X - q.m_P1X, q.m_P2Y - q.m_P1Y));
}

static void AppendCornersForContour(const std::vector<QuadraticRecord>& quadratics,
                                    size_t contour_start,
                                    size_t contour_end,
                                    std::vector<CornerRecord>& corners)
{
    if (contour_end <= contour_start)
    {
        return;
    }

    float twice_area = 0.0f;
    for (size_t i = contour_start; i < contour_end; ++i)
    {
        const QuadraticRecord& q = quadratics[i];
        twice_area += q.m_P0X * q.m_P2Y - q.m_P2X * q.m_P0Y;
    }

    bool ccw = twice_area >= 0.0f;

    for (size_t i = contour_start; i < contour_end; ++i)
    {
        const QuadraticRecord& prev = quadratics[i == contour_start ? contour_end - 1 : i - 1];
        const QuadraticRecord& curr = quadratics[i];

        GlyphPoint e0 = Normalize(MakePoint(-(QuadraticEndTangent(prev).m_X),
                                            -(QuadraticEndTangent(prev).m_Y)));
        GlyphPoint e1 = QuadraticStartTangent(curr);

        float len0_sq = e0.m_X * e0.m_X + e0.m_Y * e0.m_Y;
        float len1_sq = e1.m_X * e1.m_X + e1.m_Y * e1.m_Y;
        if (len0_sq <= 1e-8f || len1_sq <= 1e-8f)
        {
            continue;
        }

        CornerRecord corner;
        corner.m_X = curr.m_P0X;
        corner.m_Y = curr.m_P0Y;
        corner.m_T0X = e0.m_X;
        corner.m_T0Y = e0.m_Y;
        corner.m_T1X = e1.m_X;
        corner.m_T1Y = e1.m_Y;
        corner.m_OrientationSign = ccw ? 1.0f : -1.0f;
        corners.push_back(corner);
    }
}

static void AppendEdge(std::vector<EdgeRecord>& edges,
                       const GlyphPoint& a,
                       const GlyphPoint& b,
                       const GlyphPoint& inward_normal,
                       uint32_t source_curve_index)
{
    GlyphPoint segment = MakePoint(b.m_X - a.m_X, b.m_Y - a.m_Y);
    GlyphPoint tangent = Normalize(segment);
    float length = sqrtf(segment.m_X * segment.m_X + segment.m_Y * segment.m_Y);
    if (length <= 1e-5f)
    {
        return;
    }

    EdgeRecord edge;
    edge.m_X = a.m_X;
    edge.m_Y = a.m_Y;
    edge.m_Tx = tangent.m_X;
    edge.m_Ty = tangent.m_Y;
    edge.m_Nx = inward_normal.m_X;
    edge.m_Ny = inward_normal.m_Y;
    edge.m_Length = length;
    edge.m_ContourMidX = 0.0f;
    edge.m_ContourMidY = 0.0f;
    edge.m_SourceCurveIndex = source_curve_index;
    edges.push_back(edge);
}

static void BuildEdgePatchVertices(const std::vector<EdgeRecord>& edges,
                                   const std::vector<uint32_t>& edge_start_indices,
                                   const std::vector<uint32_t>& edge_end_indices,
                                   const std::vector<uint32_t>& edge_contour_indices,
                                   float feather_radius,
                                   std::vector<FeatherPatchVertex>& vertices)
{
    vertices.clear();
    if (feather_radius <= 0.0f)
    {
        return;
    }

    vertices.reserve(edges.size() * 12);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        const EdgeRecord& edge = edges[i];
        GlyphPoint p0 = MakePoint(edge.m_X, edge.m_Y);
        GlyphPoint tangent = MakePoint(edge.m_Tx, edge.m_Ty);
        GlyphPoint normal = MakePoint(edge.m_Nx, edge.m_Ny);
        GlyphPoint p1 = MakePoint(p0.m_X + tangent.m_X * edge.m_Length,
                                  p0.m_Y + tangent.m_Y * edge.m_Length);
        uint32_t start_index = i < edge_start_indices.size() ? edge_start_indices[i] : 0u;
        uint32_t end_index = i < edge_end_indices.size() ? edge_end_indices[i] : start_index;
        uint32_t contour_index = i < edge_contour_indices.size() ? edge_contour_indices[i] : 0u;

        GlyphPoint outer0 = MakePoint(p0.m_X - normal.m_X * feather_radius,
                                      p0.m_Y - normal.m_Y * feather_radius);
        GlyphPoint outer1 = MakePoint(p1.m_X - normal.m_X * feather_radius,
                                      p1.m_Y - normal.m_Y * feather_radius);
        GlyphPoint center0 = p0;
        GlyphPoint center1 = p1;
        GlyphPoint inner0 = MakePoint(p0.m_X + normal.m_X * feather_radius,
                                      p0.m_Y + normal.m_Y * feather_radius);
        GlyphPoint inner1 = MakePoint(p1.m_X + normal.m_X * feather_radius,
                                      p1.m_Y + normal.m_Y * feather_radius);

        float positive_sign0 = TriangleOrientationSign(outer0, center0, outer1);
        float positive_sign1 = TriangleOrientationSign(outer1, center0, center1);
        float negative_sign0 = -TriangleOrientationSign(center0, inner0, center1);
        float negative_sign1 = -TriangleOrientationSign(center1, inner0, inner1);

        float stroke_params = PackParamsAsFloat(2u, kPatchRoleStroke);
        FeatherPatchVertex p0_outer = MakePatchVertex(0.0f, 0.0f, 0.0f, stroke_params, 0.0f, -1.0f, positive_sign0, positive_sign0, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.0f, 0.0f, -1.0f, positive_sign0 }, start_index, contour_index);
        FeatherPatchVertex p0_center = MakePatchVertex(0.5f, 0.0f, 0.0f, stroke_params, 0.5f, 0.0f, positive_sign0, positive_sign0, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.5f, 0.0f,  0.0f, positive_sign0 }, start_index, contour_index);
        FeatherPatchVertex p1_outer = MakePatchVertex(0.0f, 0.0f, 0.0f, stroke_params, 0.0f, -1.0f, positive_sign0, positive_sign0, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.0f, 0.0f, -1.0f, positive_sign0 }, end_index, contour_index);
        FeatherPatchVertex p1_outer_b = MakePatchVertex(0.0f, 0.0f, 0.0f, stroke_params, 0.0f, -1.0f, positive_sign1, positive_sign1, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.0f, 0.0f, -1.0f, positive_sign1 }, end_index, contour_index);
        FeatherPatchVertex p0_center_b = MakePatchVertex(0.5f, 0.0f, 0.0f, stroke_params, 0.5f, 0.0f, positive_sign1, positive_sign1, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.5f, 0.0f,  0.0f, positive_sign1 }, start_index, contour_index);
        FeatherPatchVertex p1_center = MakePatchVertex(0.5f, 0.0f, 0.0f, stroke_params, 0.5f, 0.0f, positive_sign1, positive_sign1, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.5f, 0.0f,  0.0f, positive_sign1 }, end_index, contour_index);

        FeatherPatchVertex n0_center = MakePatchVertex(0.5f, 0.0f, 0.0f, stroke_params, 0.5f, 0.0f, negative_sign0, negative_sign0, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.5f, 0.0f,  0.0f, negative_sign0 }, start_index, contour_index);
        FeatherPatchVertex n0_inner = MakePatchVertex(0.0f, 0.0f, 0.0f, stroke_params, 0.0f, 1.0f, negative_sign0, negative_sign0, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.0f, 0.0f,  1.0f, negative_sign0 }, start_index, contour_index);
        FeatherPatchVertex n1_center = MakePatchVertex(0.5f, 0.0f, 0.0f, stroke_params, 0.5f, 0.0f, negative_sign0, negative_sign0, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.5f, 0.0f,  0.0f, negative_sign0 }, end_index, contour_index);
        FeatherPatchVertex n1_center_b = MakePatchVertex(0.5f, 0.0f, 0.0f, stroke_params, 0.5f, 0.0f, negative_sign1, negative_sign1, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.5f, 0.0f,  0.0f, negative_sign1 }, end_index, contour_index);
        FeatherPatchVertex n0_inner_b = MakePatchVertex(0.0f, 0.0f, 0.0f, stroke_params, 0.0f, 1.0f, negative_sign1, negative_sign1, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.0f, 0.0f,  1.0f, negative_sign1 }, start_index, contour_index);
        FeatherPatchVertex n1_inner = MakePatchVertex(0.0f, 0.0f, 0.0f, stroke_params, 0.0f, 1.0f, negative_sign1, negative_sign1, vector_float2{ 0.0f, 0.0f }, vector_float4{ 0.0f, 0.0f, 0.0f, stroke_params }, vector_float4{ 0.0f, 0.0f,  1.0f, negative_sign1 }, end_index, contour_index);

        vertices.push_back(p0_outer);
        vertices.push_back(p0_center);
        vertices.push_back(p1_outer);
        vertices.push_back(p1_outer_b);
        vertices.push_back(p0_center_b);
        vertices.push_back(p1_center);

        vertices.push_back(n0_center);
        vertices.push_back(n0_inner);
        vertices.push_back(n1_center);
        vertices.push_back(n1_center_b);
        vertices.push_back(n0_inner_b);
        vertices.push_back(n1_inner);
    }
}

static float WrapAngleDelta(float delta)
{
    while (delta > (float)M_PI)
    {
        delta -= 2.0f * (float)M_PI;
    }
    while (delta < -(float)M_PI)
    {
        delta += 2.0f * (float)M_PI;
    }
    return delta;
}

struct FeatherJoinStep
{
    bool  m_Valid;
    float m_Theta;
    float m_Outset;
};

static float CalcPolarSegmentsPerRadian(float approx_dev_stroke_radius)
{
    constexpr float kPolarPrecision = 8.0f;
    float radius = fmaxf(approx_dev_stroke_radius, 1.0f);
    float cos_theta = 1.0f - (1.0f / kPolarPrecision) / radius;
    cos_theta = fmaxf(cos_theta, -1.0f);
    return 0.5f / acosf(cos_theta);
}

static uint32_t FeatherJoinSegmentCount(float feather_radius)
{
    constexpr uint32_t kFeatherJoinHelperVertexCount  = 3u;
    constexpr uint32_t kFeatherJoinHelperSegmentCount = kFeatherJoinHelperVertexCount + 1u;
    constexpr uint32_t kFeatherJoinMinSegmentCount    = 2u + kFeatherJoinHelperSegmentCount;

    float polar_segments_per_radian = CalcPolarSegmentsPerRadian(feather_radius);
    uint32_t n = (uint32_t)ceilf(polar_segments_per_radian * (float)M_PI) + kFeatherJoinHelperSegmentCount;
    return std::max(n, kFeatherJoinMinSegmentCount);
}

static FeatherJoinStep ResolveFeatherJoinStep(uint32_t join_vertex_id,
                                              uint32_t join_segment_count,
                                              float edge0_angle,
                                              float corner_theta,
                                              float outset)
{
    FeatherJoinStep step;
    step.m_Valid  = true;
    step.m_Theta  = edge0_angle;
    step.m_Outset = outset;

    float join_vertex = (float)join_vertex_id;
    float segment_count = (float)join_segment_count;
    float edge1_angle = edge0_angle + corner_theta;

    float non_helper_segment_count = fmaxf(segment_count + 1.0f - 3.0f, 2.0f);
    float forward_segment_count = fminf(fmaxf(roundf(fabsf(corner_theta) / (float)M_PI * non_helper_segment_count),
                                              1.0f),
                                        non_helper_segment_count - 1.0f);
    float backward_segment_count = non_helper_segment_count - forward_segment_count;

    if (join_vertex <= backward_segment_count)
    {
        corner_theta = -((float)M_PI * (corner_theta >= 0.0f ? 1.0f : -1.0f) - corner_theta);
        segment_count = backward_segment_count;
        if (join_vertex == backward_segment_count)
        {
            step.m_Outset = -step.m_Outset;
        }
    }
    else if (join_vertex == backward_segment_count + 1.0f)
    {
        step.m_Valid = false;
        return step;
    }
    else
    {
        join_vertex -= backward_segment_count + 2.0f;
        segment_count = forward_segment_count;
    }

    if (segment_count <= 0.0f)
    {
        step.m_Valid = false;
        return step;
    }

    step.m_Theta = join_vertex == segment_count
        ? edge1_angle
        : edge0_angle + corner_theta * (join_vertex / segment_count);
    return step;
}

static void AppendCornerPatchVertices(const std::vector<CornerRecord>& corners,
                                      float feather_radius,
                                      std::vector<FeatherPatchVertex>& vertices)
{
    if (feather_radius <= 0.0f)
    {
        return;
    }

    constexpr uint32_t kCornerSubdivisions = 8;

    for (const CornerRecord& corner : corners)
    {
        GlyphPoint e0 = MakePoint(corner.m_T0X, corner.m_T0Y);
        GlyphPoint e1 = MakePoint(corner.m_T1X, corner.m_T1Y);

        GlyphPoint incoming = MakePoint(-e0.m_X, -e0.m_Y);
        float corner_cos = incoming.m_X * e1.m_X + incoming.m_Y * e1.m_Y;
        if (corner_cos > 0.5f)
        {
            continue;
        }

        float turn = incoming.m_X * e1.m_Y - incoming.m_Y * e1.m_X;
        if (corner.m_OrientationSign * turn <= 1e-4f)
        {
            continue;
        }

        GlyphPoint outward0;
        GlyphPoint outward1;
        if (corner.m_OrientationSign > 0.0f)
        {
            outward0 = Normalize(MakePoint(-RightNormal(e0).m_X, -RightNormal(e0).m_Y));
            outward1 = Normalize(MakePoint(-LeftNormal(e1).m_X,  -LeftNormal(e1).m_Y));
        }
        else
        {
            outward0 = Normalize(MakePoint(-LeftNormal(e0).m_X,  -LeftNormal(e0).m_Y));
            outward1 = Normalize(MakePoint(-RightNormal(e1).m_X, -RightNormal(e1).m_Y));
        }

        float angle0 = atan2f(outward0.m_Y, outward0.m_X);
        float delta = WrapAngleDelta(atan2f(outward1.m_Y, outward1.m_X) - angle0);
        if (fabsf(delta) <= 1e-4f)
        {
            continue;
        }

        GlyphPoint center = MakePoint(corner.m_X, corner.m_Y);
        FeatherPatchVertex center_vertex =
            MakePatchVertex(0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            0.0f,
                            1.0f,
                            0.0f,
                            0.0f,
                            vector_float2{ center.m_X, center.m_Y },
                            vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                            vector_float4{ 0.0f, 1.0f, 0.0f, 0.0f });

        for (uint32_t i = 0; i < kCornerSubdivisions; ++i)
        {
            float t0 = (float)i / (float)kCornerSubdivisions;
            float t1 = (float)(i + 1) / (float)kCornerSubdivisions;

            float a0 = angle0 + delta * t0;
            float a1 = angle0 + delta * t1;

            GlyphPoint p0 = MakePoint(center.m_X + cosf(a0) * feather_radius,
                                      center.m_Y + sinf(a0) * feather_radius);
            GlyphPoint p1 = MakePoint(center.m_X + cosf(a1) * feather_radius,
                                      center.m_Y + sinf(a1) * feather_radius);

            FeatherPatchVertex v0 =
                MakePatchVertex(0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                1.0f,
                                0.0f,
                                0.0f,
                                vector_float2{ p0.m_X, p0.m_Y },
                                vector_float4{ 0.0f, angle0, delta, -1.0f },
                                vector_float4{ 0.0f, 1.0f, 0.0f, 0.0f });
            FeatherPatchVertex v1 =
                MakePatchVertex(0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                0.0f,
                                1.0f,
                                0.0f,
                                0.0f,
                                vector_float2{ p1.m_X, p1.m_Y },
                                vector_float4{ 0.0f, angle0, delta, -1.0f },
                                vector_float4{ 0.0f, 1.0f, 0.0f, 0.0f });

            vertices.push_back(center_vertex);
            vertices.push_back(v0);
            vertices.push_back(v1);
        }
    }
}

static GlyphPoint EdgeEndPoint(const EdgeRecord& edge)
{
    return MakePoint(edge.m_X + edge.m_Tx * edge.m_Length,
                     edge.m_Y + edge.m_Ty * edge.m_Length);
}

static bool ArePointsNear(const GlyphPoint& a, const GlyphPoint& b, float epsilon_sq)
{
    GlyphPoint delta = MakePoint(b.m_X - a.m_X, b.m_Y - a.m_Y);
    return delta.m_X * delta.m_X + delta.m_Y * delta.m_Y <= epsilon_sq;
}

static uint32_t FloatToBits(float value)
{
    union FloatBits
    {
        float    m_Float;
        uint32_t m_Bits;
    };
    FloatBits fb;
    fb.m_Float = value;
    return fb.m_Bits;
}

static uint32_t PackUint16x2(uint32_t lo, uint32_t hi)
{
    return (hi << 16) | (lo & 0xffffu);
}

static float BitsToFloat(uint32_t bits)
{
    union BitsFloat
    {
        uint32_t m_Bits;
        float    m_Float;
    };
    BitsFloat bf;
    bf.m_Bits = bits;
    return bf.m_Float;
}

static float PackParamsAsFloat(uint32_t patch_segment_span, uint32_t vertex_type)
{
    return BitsToFloat((patch_segment_span << 2) | (vertex_type & 3u));
}

static int32_t PackParamsAsInt(uint32_t patch_segment_span, uint32_t vertex_type)
{
    return (int32_t)((patch_segment_span << 2) | (vertex_type & 3u));
}

static uint32_t FloatBitsToUInt(float value)
{
    union FloatBits
    {
        float    m_Float;
        uint32_t m_Bits;
    };
    FloatBits fb;
    fb.m_Float = value;
    return fb.m_Bits;
}

static uint32_t PatchRoleFromPackedParams(float params)
{
    return FloatBitsToUInt(params) & 3u;
}

static FeatherPatchVertex MakePatchVertex(float local_vertex_id,
                                          float outset,
                                          float fill_coverage,
                                          float params,
                                          float mirrored_local_vertex_id,
                                          float mirrored_outset,
                                          float mirrored_fill_coverage,
                                          float mirrored_padding,
                                          vector_float2 position_px,
                                          vector_float4 patch_data,
                                          vector_float4 mirrored_patch_data,
                                          uint32_t index0,
                                          uint32_t index1)
{
    (void)mirrored_padding;
    FeatherPatchVertex vertex;
    vertex.m_LocalVertexID = local_vertex_id;
    vertex.m_Outset = outset;
    vertex.m_FillCoverage = fill_coverage;
    vertex.m_Params = params;
    vertex.m_MirroredLocalVertexID = mirrored_local_vertex_id;
    vertex.m_MirroredOutset = mirrored_outset;
    vertex.m_MirroredFillCoverage = mirrored_fill_coverage;
    vertex.m_Padding = 0;
    vertex.m_PositionPx = position_px;
    vertex.m_PatchData = patch_data;
    vertex.m_MirroredPatchData = mirrored_patch_data;
    vertex.m_Indices = { index0, index1 };
    return vertex;
}

static RiveTessVertexRecord BuildRiveTessVertexRecord(const CubicRecord& cubic,
                                                     const GlyphPoint& join_tangent,
                                                     uint32_t y,
                                                     uint32_t reflection_y,
                                                     int32_t x0,
                                                     int32_t x1,
                                                     int32_t reflection_x0,
                                                     int32_t reflection_x1,
                                                     uint32_t parametric_segment_count,
                                                     uint32_t polar_segment_count,
                                                     uint32_t join_segment_count,
                                                     uint32_t contour_id_with_flags)
{
    RiveTessVertexRecord record = {};
    record.m_P0XBits = FloatToBits(cubic.m_P0X);
    record.m_P0YBits = FloatToBits(cubic.m_P0Y);
    record.m_P1XBits = FloatToBits(cubic.m_P1X);
    record.m_P1YBits = FloatToBits(cubic.m_P1Y);
    record.m_P2XBits = FloatToBits(cubic.m_P2X);
    record.m_P2YBits = FloatToBits(cubic.m_P2Y);
    record.m_P3XBits = FloatToBits(cubic.m_P3X);
    record.m_P3YBits = FloatToBits(cubic.m_P3Y);
    record.m_JoinTangentXBits = FloatToBits(join_tangent.m_X);
    record.m_JoinTangentYBits = FloatToBits(join_tangent.m_Y);
    record.m_YBits = FloatToBits((float)y);
    record.m_ReflectionYBits = FloatToBits((float)reflection_y);
    record.m_X0X1 = (uint32_t)((x1 << 16) | (x0 & 0xffff));
    record.m_ReflectionX0X1 = (uint32_t)((reflection_x1 << 16) | (reflection_x0 & 0xffff));
    record.m_SegmentCounts = (join_segment_count << 20) |
                             (polar_segment_count << 10) |
                             parametric_segment_count;
    record.m_ContourIDWithFlags = contour_id_with_flags;
    return record;
}

static RivePathDataRecord BuildRivePathDataRecord(float feather_radius)
{
    RivePathDataRecord record = {};
    record.m_Matrix0Bits = FloatToBits(1.0f);
    record.m_Matrix1Bits = FloatToBits(0.0f);
    record.m_Matrix2Bits = FloatToBits(0.0f);
    record.m_Matrix3Bits = FloatToBits(1.0f);
    record.m_TranslateXBits = FloatToBits(0.0f);
    record.m_TranslateYBits = FloatToBits(0.0f);
    record.m_StrokeRadiusBits = FloatToBits(0.0f);
    record.m_FeatherRadiusBits = FloatToBits(feather_radius);
    record.m_PathID = 1u;
    record.m_ZIndex = 0u;
    return record;
}

static float EdgeThetaForRive(const EdgeRecord& edge)
{
    GlyphPoint norm = Normalize(MakePoint(edge.m_Nx, edge.m_Ny));
    return atan2f(norm.m_X, -norm.m_Y);
}

static GlyphPoint ComputeRiveContourMidpoint(const std::vector<GlyphPoint>& contour)
{
    if (contour.empty())
    {
        return MakePoint(0.0f, 0.0f);
    }

    GlyphPoint sum = MakePoint(0.0f, 0.0f);
    for (const GlyphPoint& p : contour)
    {
        sum.m_X += p.m_X;
        sum.m_Y += p.m_Y;
    }

    float inv_count = 1.0f / (float)contour.size();
    return MakePoint(sum.m_X * inv_count, sum.m_Y * inv_count);
}

static bool BuildRiveLikeContourAndTessData(const std::vector<std::vector<GlyphPoint> >& contours,
                                            const std::vector<CubicRecord>& cubics,
                                            const std::vector<std::pair<size_t, size_t> >& contour_cubic_ranges,
                                            float feather_radius,
                                            std::vector<RiveContourDataRecord>& contour_records,
                                            std::vector<RiveTessVertexRecord>& tess_records,
                                            std::vector<uint32_t>& edge_start_indices,
                                            std::vector<uint32_t>& edge_end_indices,
                                            std::vector<uint32_t>& edge_contour_indices)
{
    contour_records.clear();
    tess_records.clear();
    edge_start_indices.assign(cubics.size(), 0u);
    edge_end_indices.assign(cubics.size(), 0u);
    edge_contour_indices.assign(cubics.size(), 0u);

    if (cubics.empty() || contour_cubic_ranges.empty())
    {
        return false;
    }

    size_t contour_index = 0;
    for (const std::pair<size_t, size_t>& contour_range : contour_cubic_ranges)
    {
        size_t contour_start = contour_range.first;
        size_t contour_end = contour_range.second;
        if (contour_start >= contour_end)
        {
            ++contour_index;
            continue;
        }

        GlyphPoint midpoint = contour_index < contours.size()
            ? ComputeRiveContourMidpoint(contours[contour_index])
            : MakePoint(cubics[contour_start].m_P0X, cubics[contour_start].m_P0Y);

        uint32_t contour_id = (uint32_t)contour_records.size() + 1u;
        uint32_t vertex_index0 = (uint32_t)tess_records.size();
        contour_records.push_back({
            FloatToBits(midpoint.m_X),
            FloatToBits(midpoint.m_Y),
            0u,
            vertex_index0
        });

        for (size_t cubic_i = contour_start; cubic_i < contour_end; ++cubic_i)
        {
            uint32_t tess_index = (uint32_t)tess_records.size();
            const CubicRecord& cubic = cubics[cubic_i];
            GlyphPoint p0 = MakePoint(cubic.m_P0X, cubic.m_P0Y);
            GlyphPoint p1 = MakePoint(cubic.m_P1X, cubic.m_P1Y);
            GlyphPoint p2 = MakePoint(cubic.m_P2X, cubic.m_P2Y);
            GlyphPoint p3 = MakePoint(cubic.m_P3X, cubic.m_P3Y);
            GlyphPoint join_tangent = Normalize(MakePoint(p3.m_X - p2.m_X,
                                                          p3.m_Y - p2.m_Y));
            GlyphPoint tan0 = Normalize(MakePoint(p1.m_X - p0.m_X, p1.m_Y - p0.m_Y));
            GlyphPoint tan1 = Normalize(MakePoint(p3.m_X - p2.m_X, p3.m_Y - p2.m_Y));
            float tangent_dot = fmaxf(-1.0f, fminf(1.0f, tan0.m_X * tan1.m_X + tan0.m_Y * tan1.m_Y));
            float rotation = acosf(tangent_dot);
            float polar_segments_per_radian = CalcPolarSegmentsPerRadian(feather_radius);
            uint32_t parametric_segment_count = std::max(17u, (uint32_t)ceilf(rotation * polar_segments_per_radian * 4.0f));
            uint32_t polar_segment_count = 0u;
            uint32_t join_segment_count = 1u;
            int32_t x0 = (int32_t)(tess_index * parametric_segment_count);
            int32_t x1 = x0 + (int32_t)parametric_segment_count;
            tess_records.push_back(BuildRiveTessVertexRecord(cubic,
                                                             join_tangent,
                                                             (uint32_t)contour_index,
                                                             0u,
                                                             x0,
                                                             x1,
                                                             x0,
                                                             x1,
                                                             parametric_segment_count,
                                                             polar_segment_count,
                                                             join_segment_count,
                                                             contour_id));

            edge_start_indices[cubic_i] = tess_index;
            edge_end_indices[cubic_i] = tess_index;
            edge_contour_indices[cubic_i] = contour_id - 1u;
        }

        contour_index++;
    }

    return !contour_records.empty() && !tess_records.empty();
}

static float TriangleOrientationSign(const GlyphPoint& a,
                                     const GlyphPoint& b,
                                     const GlyphPoint& c)
{
    float twice_area = (b.m_X - a.m_X) * (c.m_Y - a.m_Y) -
                       (b.m_Y - a.m_Y) * (c.m_X - a.m_X);
    return twice_area >= 0.0f ? 1.0f : -1.0f;
}

static void AppendSingleEdgeJoinPatch(const EdgeRecord& prev,
                                      const EdgeRecord& next,
                                      float feather_radius,
                                      std::vector<FeatherPatchVertex>& vertices)
{
    GlyphPoint prev_tangent = MakePoint(prev.m_Tx, prev.m_Ty);
    GlyphPoint next_tangent = MakePoint(next.m_Tx, next.m_Ty);
    float tangent_dot = prev_tangent.m_X * next_tangent.m_X + prev_tangent.m_Y * next_tangent.m_Y;
    float tangent_threshold = prev.m_SourceCurveIndex == next.m_SourceCurveIndex ? 0.995f : 0.90f;
    if (tangent_dot > tangent_threshold)
    {
        return;
    }

    GlyphPoint join = EdgeEndPoint(prev);
    GlyphPoint n0 = Normalize(MakePoint(prev.m_Nx, prev.m_Ny));
    GlyphPoint n1 = Normalize(MakePoint(next.m_Nx, next.m_Ny));
    GlyphPoint outward0 = MakePoint(-n0.m_X, -n0.m_Y);
    GlyphPoint outward1 = MakePoint(-n1.m_X, -n1.m_Y);

    float angle0 = atan2f(outward0.m_Y, outward0.m_X);
    float angle1 = atan2f(outward1.m_Y, outward1.m_X);
    float corner_theta = WrapAngleDelta(angle1 - angle0);
    if (corner_theta < 0.0f)
    {
        corner_theta = -corner_theta;
        float tmp = angle0;
        angle0 = angle1;
        angle1 = tmp;
        GlyphPoint tmp_vec = outward0;
        outward0 = outward1;
        outward1 = tmp_vec;
    }

    if (corner_theta <= 1e-4f)
    {
        return;
    }

    uint32_t join_segment_count = FeatherJoinSegmentCount(feather_radius);
    for (uint32_t i = 0; i < join_segment_count; ++i)
    {
        FeatherJoinStep step0 = ResolveFeatherJoinStep(i,     join_segment_count, angle0, corner_theta, -1.0f);
        FeatherJoinStep step1 = ResolveFeatherJoinStep(i + 1, join_segment_count, angle0, corner_theta, -1.0f);
        if (!step0.m_Valid || !step1.m_Valid)
        {
            continue;
        }

        GlyphPoint p0 = MakePoint(join.m_X + cosf(step0.m_Theta) * feather_radius * fabsf(step0.m_Outset),
                                  join.m_Y + sinf(step0.m_Theta) * feather_radius * fabsf(step0.m_Outset));
        GlyphPoint p1 = MakePoint(join.m_X + cosf(step1.m_Theta) * feather_radius * fabsf(step1.m_Outset),
                                  join.m_Y + sinf(step1.m_Theta) * feather_radius * fabsf(step1.m_Outset));

        float sign = TriangleOrientationSign(join, p0, p1);

        FeatherPatchVertex v0 =
            MakePatchVertex((float)i,
                            -1.0f,
                            1.0f,
                            PackParamsAsFloat((uint32_t)join_segment_count, kPatchRoleFan),
                            (float)i,
                            -1.0f,
                            1.0f,
                            0.0f,
                            vector_float2{ p0.m_X, p0.m_Y },
                            vector_float4{ (float)i, (float)join_segment_count, angle0, corner_theta },
                            vector_float4{ 0.0f, 1.0f, -1.0f, sign });
        FeatherPatchVertex v1 =
            MakePatchVertex((float)(i + 1),
                            -1.0f,
                            1.0f,
                            PackParamsAsFloat((uint32_t)join_segment_count, kPatchRoleFan),
                            (float)(i + 1),
                            -1.0f,
                            1.0f,
                            0.0f,
                            vector_float2{ p1.m_X, p1.m_Y },
                            vector_float4{ (float)(i + 1), (float)join_segment_count, angle0, corner_theta },
                            vector_float4{ 0.0f, 1.0f, -1.0f, sign });
        FeatherPatchVertex center_vertex =
            MakePatchVertex((float)i + 0.5f,
                            -1.0f,
                            1.0f,
                            PackParamsAsFloat((uint32_t)join_segment_count, kPatchRoleFan),
                            (float)i + 0.5f,
                            -1.0f,
                            1.0f,
                            0.0f,
                            vector_float2{ join.m_X, join.m_Y },
                            vector_float4{ (float)i + 0.5f, (float)join_segment_count, angle0, corner_theta },
                            vector_float4{ 0.0f, 1.0f, 0.0f, sign });

        vertices.push_back(center_vertex);
        vertices.push_back(v0);
        vertices.push_back(v1);
    }
}

static void AppendEdgeJoinPatchVertices(const std::vector<EdgeRecord>& edges,
                                        float feather_radius,
                                        std::vector<FeatherPatchVertex>& vertices)
{
    if (feather_radius <= 0.0f || edges.size() < 2)
    {
        return;
    }

    constexpr float kJoinEpsilonSq = 1e-4f;

    size_t contour_start = 0;
    while (contour_start < edges.size())
    {
        size_t contour_end = contour_start + 1;
        while (contour_end < edges.size())
        {
            GlyphPoint prev_end = EdgeEndPoint(edges[contour_end - 1]);
            GlyphPoint next_start = MakePoint(edges[contour_end].m_X, edges[contour_end].m_Y);
            if (!ArePointsNear(prev_end, next_start, kJoinEpsilonSq))
            {
                break;
            }
            ++contour_end;
        }

        for (size_t i = contour_start; i + 1 < contour_end; ++i)
        {
            AppendSingleEdgeJoinPatch(edges[i], edges[i + 1], feather_radius, vertices);
        }

        if (contour_end - contour_start > 1)
        {
            GlyphPoint last_end = EdgeEndPoint(edges[contour_end - 1]);
            GlyphPoint first_start = MakePoint(edges[contour_start].m_X, edges[contour_start].m_Y);
            if (ArePointsNear(last_end, first_start, kJoinEpsilonSq))
            {
                AppendSingleEdgeJoinPatch(edges[contour_end - 1], edges[contour_start], feather_radius, vertices);
            }
        }

        contour_start = contour_end;
    }
}

static float SignedArea(const std::vector<GlyphPoint>& polygon)
{
    if (polygon.size() < 3)
    {
        return 0.0f;
    }

    float area = 0.0f;
    for (size_t i = 0; i < polygon.size(); ++i)
    {
        const GlyphPoint& a = polygon[i];
        const GlyphPoint& b = polygon[(i + 1) % polygon.size()];
        area += a.m_X * b.m_Y - b.m_X * a.m_Y;
    }
    return area * 0.5f;
}

static bool PointInTriangle(const GlyphPoint& p,
                            const GlyphPoint& a,
                            const GlyphPoint& b,
                            const GlyphPoint& c)
{
    float s1 = TriangleOrientationSign(a, b, p);
    float s2 = TriangleOrientationSign(b, c, p);
    float s3 = TriangleOrientationSign(c, a, p);
    bool has_neg = s1 < 0.0f || s2 < 0.0f || s3 < 0.0f;
    bool has_pos = s1 > 0.0f || s2 > 0.0f || s3 > 0.0f;
    return !(has_neg && has_pos);
}

static bool IsEar(const std::vector<GlyphPoint>& polygon,
                  const std::vector<int>& indices,
                  size_t i,
                  float orientation_sign)
{
    int ia = indices[(i + indices.size() - 1) % indices.size()];
    int ib = indices[i];
    int ic = indices[(i + 1) % indices.size()];

    const GlyphPoint& a = polygon[ia];
    const GlyphPoint& b = polygon[ib];
    const GlyphPoint& c = polygon[ic];

    float tri_sign = TriangleOrientationSign(a, b, c);
    if (tri_sign != orientation_sign)
    {
        return false;
    }

    for (size_t j = 0; j < indices.size(); ++j)
    {
        int ip = indices[j];
        if (ip == ia || ip == ib || ip == ic)
        {
            continue;
        }
        if (PointInTriangle(polygon[ip], a, b, c))
        {
            return false;
        }
    }
    return true;
}

static void AppendInteriorPatchVertices(const std::vector<std::vector<GlyphPoint> >& contours,
                                        std::vector<FeatherPatchVertex>& vertices)
{
    if (contours.empty())
    {
        return;
    }

    for (const std::vector<GlyphPoint>& polygon : contours)
    {
        if (polygon.size() >= 3)
        {
            float area = SignedArea(polygon);
            float orientation_sign = area >= 0.0f ? 1.0f : -1.0f;
            GlyphPoint center = MakePoint(0.0f, 0.0f);
            for (const GlyphPoint& p : polygon)
            {
                center.m_X += p.m_X;
                center.m_Y += p.m_Y;
            }
            float inv_count = 1.0f / (float)polygon.size();
            center.m_X *= inv_count;
            center.m_Y *= inv_count;

            float fan_params = PackParamsAsFloat(2u, kPatchRoleFanMidpoint);
            FeatherPatchVertex center_vertex = MakePatchVertex(0.0f,
                                                               0.0f,
                                                               0.0f,
                                                               fan_params,
                                                               0.0f,
                                                               1.0f,
                                                               orientation_sign,
                                                               0.0f,
                                                               vector_float2{ center.m_X, center.m_Y },
                                                               vector_float4{ 0.0f, 0.0f, 0.0f, fan_params },
                                                               vector_float4{ 0.0f, 1.0f, 0.0f, orientation_sign },
                                                               0u,
                                                               0u);

            for (size_t i = 0; i < polygon.size(); ++i)
            {
                size_t j = (i + 1) % polygon.size();
                FeatherPatchVertex v0 = MakePatchVertex(0.0f,
                                                         0.0f,
                                                         0.0f,
                                                         fan_params,
                                                         0.0f,
                                                         2.0f,
                                                         orientation_sign,
                                                         0.0f,
                                                         vector_float2{ polygon[i].m_X, polygon[i].m_Y },
                                                         vector_float4{ 0.0f, 0.0f, 0.0f, fan_params },
                                                         vector_float4{ 0.0f, 2.0f, 0.0f, orientation_sign },
                                                         0u,
                                                         0u);
                FeatherPatchVertex v1 = MakePatchVertex(0.0f,
                                                         0.0f,
                                                         0.0f,
                                                         fan_params,
                                                         0.0f,
                                                         2.0f,
                                                         orientation_sign,
                                                         0.0f,
                                                         vector_float2{ polygon[j].m_X, polygon[j].m_Y },
                                                         vector_float4{ 0.0f, 0.0f, 0.0f, fan_params },
                                                         vector_float4{ 0.0f, 2.0f, 0.0f, orientation_sign },
                                                         0u,
                                                         0u);
                vertices.push_back(center_vertex);
                vertices.push_back(v0);
                vertices.push_back(v1);
            }
        }
    }
}

static GlyphPoint InwardNormalForContourSegment(const GlyphPoint& a,
                                               const GlyphPoint& b,
                                               float orientation_sign)
{
    GlyphPoint seg = MakePoint(b.m_X - a.m_X, b.m_Y - a.m_Y);
    if (orientation_sign >= 0.0f)
    {
        return Normalize(LeftNormal(seg));
    }
    return Normalize(RightNormal(seg));
}

static float Dot(const GlyphPoint& a, const GlyphPoint& b)
{
    return a.m_X * b.m_X + a.m_Y * b.m_Y;
}

static float Cross(const GlyphPoint& a, const GlyphPoint& b)
{
    return a.m_X * b.m_Y - a.m_Y * b.m_X;
}

static GlyphPoint OffsetContourPoint(const GlyphPoint& prev,
                                     const GlyphPoint& curr,
                                     const GlyphPoint& next,
                                     float orientation_sign,
                                     float feather_radius,
                                     bool outward)
{
    GlyphPoint seg0 = Normalize(MakePoint(curr.m_X - prev.m_X, curr.m_Y - prev.m_Y));
    GlyphPoint seg1 = Normalize(MakePoint(next.m_X - curr.m_X, next.m_Y - curr.m_Y));
    GlyphPoint n0 = InwardNormalForContourSegment(prev, curr, orientation_sign);
    GlyphPoint n1 = InwardNormalForContourSegment(curr, next, orientation_sign);

    float offset_sign = outward ? -1.0f : 1.0f;
    GlyphPoint p0 = MakePoint(curr.m_X + n0.m_X * feather_radius * offset_sign,
                              curr.m_Y + n0.m_Y * feather_radius * offset_sign);
    GlyphPoint p1 = MakePoint(curr.m_X + n1.m_X * feather_radius * offset_sign,
                              curr.m_Y + n1.m_Y * feather_radius * offset_sign);

    GlyphPoint delta = MakePoint(p1.m_X - p0.m_X, p1.m_Y - p0.m_Y);
    float denom = Cross(seg0, seg1);
    GlyphPoint fallback_normal = Normalize(MakePoint(n0.m_X + n1.m_X, n0.m_Y + n1.m_Y));
    if (fallback_normal.m_X == 0.0f && fallback_normal.m_Y == 0.0f)
    {
        fallback_normal = n1;
    }
    GlyphPoint fallback = MakePoint(curr.m_X + fallback_normal.m_X * feather_radius * offset_sign,
                                    curr.m_Y + fallback_normal.m_Y * feather_radius * offset_sign);

    if (fabsf(denom) < 1e-5f)
    {
        return fallback;
    }

    float t = Cross(delta, seg1) / denom;
    GlyphPoint intersection = MakePoint(p0.m_X + seg0.m_X * t,
                                        p0.m_Y + seg0.m_Y * t);

    GlyphPoint miter = MakePoint(intersection.m_X - curr.m_X, intersection.m_Y - curr.m_Y);
    float miter_len = sqrtf(Dot(miter, miter));
    const float kMiterLimit = 4.0f;
    if (miter_len > feather_radius * kMiterLimit)
    {
        return fallback;
    }

    if (Dot(miter, fallback_normal) * offset_sign <= 0.0f)
    {
        return fallback;
    }

    return intersection;
}

static void AppendContourFeatherBandVertices(const std::vector<std::vector<GlyphPoint> >& contours,
                                             float feather_radius,
                                             std::vector<FeatherPatchVertex>& vertices)
{
    if (feather_radius <= 0.0f)
    {
        return;
    }

    for (const std::vector<GlyphPoint>& contour : contours)
    {
        if (contour.size() < 3)
        {
            continue;
        }

        float orientation_sign = SignedArea(contour) >= 0.0f ? 1.0f : -1.0f;
        size_t count = contour.size();
        std::vector<GlyphPoint> inner(count);
        std::vector<GlyphPoint> outer(count);

        for (size_t i = 0; i < count; ++i)
        {
            const GlyphPoint& prev = contour[(i + count - 1) % count];
            const GlyphPoint& curr = contour[i];
            const GlyphPoint& next = contour[(i + 1) % count];

            inner[i] = OffsetContourPoint(prev, curr, next, orientation_sign, feather_radius, false);
            outer[i] = OffsetContourPoint(prev, curr, next, orientation_sign, feather_radius, true);
        }

        for (size_t i = 0; i < count; ++i)
        {
            size_t j = (i + 1) % count;

            float sign0 = TriangleOrientationSign(outer[i], inner[i], outer[j]);
            float sign1 = TriangleOrientationSign(outer[j], inner[i], inner[j]);

            FeatherPatchVertex v0 =
                MakePatchVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                vector_float2{ outer[i].m_X, outer[i].m_Y },
                                vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                                vector_float4{ 0.0f, 0.0f, 0.0f, sign0 });
            FeatherPatchVertex v1 =
                MakePatchVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                vector_float2{ inner[i].m_X, inner[i].m_Y },
                                vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                                vector_float4{ 1.0f, 0.0f, 0.0f, sign0 });
            FeatherPatchVertex v2 =
                MakePatchVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                vector_float2{ outer[j].m_X, outer[j].m_Y },
                                vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                                vector_float4{ 0.0f, 0.0f, 0.0f, sign0 });
            FeatherPatchVertex v2b =
                MakePatchVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                vector_float2{ outer[j].m_X, outer[j].m_Y },
                                vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                                vector_float4{ 0.0f, 0.0f, 0.0f, sign1 });
            FeatherPatchVertex v1b =
                MakePatchVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                vector_float2{ inner[i].m_X, inner[i].m_Y },
                                vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                                vector_float4{ 1.0f, 0.0f, 0.0f, sign1 });
            FeatherPatchVertex v3 =
                MakePatchVertex(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                vector_float2{ inner[j].m_X, inner[j].m_Y },
                                vector_float4{ 0.0f, 0.0f, 0.0f, 0.0f },
                                vector_float4{ 1.0f, 0.0f, 0.0f, sign1 });

            vertices.push_back(v0);
            vertices.push_back(v1);
            vertices.push_back(v2);
            vertices.push_back(v2b);
            vertices.push_back(v1b);
            vertices.push_back(v3);
        }
    }
}

static void AppendSoftenedEdgesForContour(const std::vector<CubicRecord>& cubics,
                                          size_t contour_start,
                                          size_t contour_end,
                                          float feather_radius,
                                          std::vector<EdgeRecord>& edges)
{
    if (contour_end <= contour_start)
    {
        return;
    }

    float twice_area = 0.0f;
    for (size_t i = contour_start; i < contour_end; ++i)
    {
        const CubicRecord& c = cubics[i];
        twice_area += c.m_P0X * c.m_P3Y - c.m_P3X * c.m_P0Y;
    }

    bool ccw = twice_area >= 0.0f;
    float polar_segments_per_radian = CalcPolarSegmentsPerRadian(feather_radius);

    for (size_t i = contour_start; i < contour_end; ++i)
    {
        const CubicRecord& c = cubics[i];
        GlyphPoint p0 = MakePoint(c.m_P0X, c.m_P0Y);
        GlyphPoint p1 = MakePoint(c.m_P1X, c.m_P1Y);
        GlyphPoint p2 = MakePoint(c.m_P2X, c.m_P2Y);
        GlyphPoint p3 = MakePoint(c.m_P3X, c.m_P3Y);

        GlyphPoint tan0 = Normalize(MakePoint(p1.m_X - p0.m_X, p1.m_Y - p0.m_Y));
        GlyphPoint tan1 = Normalize(MakePoint(p3.m_X - p2.m_X, p3.m_Y - p2.m_Y));
        float tangent_dot = fmaxf(-1.0f, fminf(1.0f, tan0.m_X * tan1.m_X + tan0.m_Y * tan1.m_Y));
        float rotation = acosf(tangent_dot);
        uint32_t subdivisions = std::max(17u, (uint32_t)ceilf(rotation * polar_segments_per_radian * 4.0f));

        GlyphPoint prev = p0;
        for (uint32_t s = 1; s <= subdivisions; ++s)
        {
            float t = (float)s / (float)subdivisions;
            float inv_t = 1.0f - t;
            GlyphPoint curr = MakePoint(inv_t * inv_t * inv_t * p0.m_X +
                                            3.0f * inv_t * inv_t * t * p1.m_X +
                                            3.0f * inv_t * t * t * p2.m_X +
                                            t * t * t * p3.m_X,
                                        inv_t * inv_t * inv_t * p0.m_Y +
                                            3.0f * inv_t * inv_t * t * p1.m_Y +
                                            3.0f * inv_t * t * t * p2.m_Y +
                                            t * t * t * p3.m_Y);
            GlyphPoint seg = MakePoint(curr.m_X - prev.m_X, curr.m_Y - prev.m_Y);
            GlyphPoint inward = ccw ? LeftNormal(seg) : RightNormal(seg);
            inward = Normalize(inward);
            AppendEdge(edges, prev, curr, inward, c.m_SourceSegmentId);
            prev = curr;
        }
    }
}

static GlyphPoint ComputeContourMidpoint(const std::vector<QuadraticRecord>& quadratics,
                                         size_t contour_start,
                                         size_t contour_end)
{
    GlyphPoint midpoint = { 0.0f, 0.0f };
    if (contour_end <= contour_start)
    {
        return midpoint;
    }

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    float count = 0.0f;
    for (size_t i = contour_start; i < contour_end; ++i)
    {
        sum_x += quadratics[i].m_P0X;
        sum_y += quadratics[i].m_P0Y;
        count += 1.0f;
    }

    midpoint.m_X = sum_x / count;
    midpoint.m_Y = sum_y / count;
    return midpoint;
}

static void AppendContourPolygon(const std::vector<QuadraticRecord>& quadratics,
                                 size_t contour_start,
                                 size_t contour_end,
                                 std::vector<std::vector<GlyphPoint> >& contours)
{
    if (contour_end <= contour_start)
    {
        return;
    }

    std::vector<GlyphPoint> polygon;
    constexpr uint32_t kContourSubdivisions = 8;
    polygon.reserve((contour_end - contour_start) * kContourSubdivisions);
    for (size_t i = contour_start; i < contour_end; ++i)
    {
        const QuadraticRecord& q = quadratics[i];
        GlyphPoint p0 = MakePoint(q.m_P0X, q.m_P0Y);
        GlyphPoint p1 = MakePoint(q.m_P1X, q.m_P1Y);
        GlyphPoint p2 = MakePoint(q.m_P2X, q.m_P2Y);

        if (polygon.empty())
        {
            polygon.push_back(p0);
        }

        for (uint32_t step = 1; step <= kContourSubdivisions; ++step)
        {
            float t = (float)step / (float)kContourSubdivisions;
            float s = 1.0f - t;
            polygon.push_back(MakePoint(s * s * p0.m_X + 2.0f * s * t * p1.m_X + t * t * p2.m_X,
                                        s * s * p0.m_Y + 2.0f * s * t * p1.m_Y + t * t * p2.m_Y));
        }
    }
    contours.push_back(polygon);
}

static void SplitQuadratic(const GlyphPoint& p0,
                           const GlyphPoint& p1,
                           const GlyphPoint& p2,
                           float t,
                           GlyphPoint out_left[3],
                           GlyphPoint out_right[3])
{
    GlyphPoint p01  = Lerp(p0, p1, t);
    GlyphPoint p12  = Lerp(p1, p2, t);
    GlyphPoint p012 = Lerp(p01, p12, t);

    out_left[0] = p0;
    out_left[1] = p01;
    out_left[2] = p012;

    out_right[0] = p012;
    out_right[1] = p12;
    out_right[2] = p2;
}

static void AppendMonotonicQuadratic(std::vector<QuadraticRecord>& quadratics,
                                     std::vector<CubicRecord>& cubics,
                                     const GlyphPoint& p0,
                                     const GlyphPoint& p1,
                                     const GlyphPoint& p2,
                                     uint32_t source_segment_id)
{
    float denom = p0.m_Y - 2.0f * p1.m_Y + p2.m_Y;
    if (fabsf(denom) > 1e-5f)
    {
        float t = (p0.m_Y - p1.m_Y) / denom;
        if (t > 1e-4f && t < 1.0f - 1e-4f)
        {
            GlyphPoint left[3];
            GlyphPoint right[3];
            SplitQuadratic(p0, p1, p2, t, left, right);
            AppendQuadratic(quadratics, left[0], left[1], left[2]);
            AppendQuadratic(quadratics, right[0], right[1], right[2]);
            AppendQuadraticAsCubic(cubics, left[0], left[1], left[2], source_segment_id);
            AppendQuadraticAsCubic(cubics, right[0], right[1], right[2], source_segment_id);
            return;
        }
    }

    AppendQuadratic(quadratics, p0, p1, p2);
    AppendQuadraticAsCubic(cubics, p0, p1, p2, source_segment_id);
}

static void FinalizeContour(std::vector<QuadraticRecord>& quadratics,
                            std::vector<CubicRecord>& cubics,
                            size_t contour_start_index,
                            size_t contour_start_cubic_index,
                            const GlyphPoint& current,
                            const GlyphPoint& contour_start,
                            float feather_radius,
                            uint32_t& next_source_segment_id,
                            std::vector<std::vector<GlyphPoint> >& contours,
                            std::vector<std::pair<size_t, size_t> >& contour_cubic_ranges,
                            std::vector<CornerRecord>& corners,
                            std::vector<EdgeRecord>& edges)
{
    if (contour_start_index >= quadratics.size())
    {
        return;
    }

    if (current.m_X != contour_start.m_X || current.m_Y != contour_start.m_Y)
    {
        GlyphPoint control =
        {
            (current.m_X + contour_start.m_X) * 0.5f,
            (current.m_Y + contour_start.m_Y) * 0.5f
        };
        AppendMonotonicQuadratic(quadratics,
                                 cubics,
                                 current,
                                 control,
                                 contour_start,
                                 next_source_segment_id++);
    }

    AppendContourPolygon(quadratics, contour_start_index, quadratics.size(), contours);
    AppendCornersForContour(quadratics, contour_start_index, quadratics.size(), corners);
    size_t edge_start_index = edges.size();
    AppendSoftenedEdgesForContour(cubics, contour_start_cubic_index, cubics.size(), feather_radius, edges);
    contour_cubic_ranges.push_back(std::make_pair(contour_start_cubic_index, cubics.size()));
    GlyphPoint contour_midpoint =
        ComputeContourMidpoint(quadratics, contour_start_index, quadratics.size());
    for (size_t i = edge_start_index; i < edges.size(); ++i)
    {
        edges[i].m_ContourMidX = contour_midpoint.m_X;
        edges[i].m_ContourMidY = contour_midpoint.m_Y;
    }
}

static bool BuildTextQuadratics(NSString* font_path,
                                const char* text,
                                uint32_t canvas_width,
                                uint32_t canvas_height,
                                float feather_radius,
                                std::vector<QuadraticRecord>& quadratics,
                                std::vector<CubicRecord>& cubics,
                                std::vector<std::vector<GlyphPoint> >& contours,
                                std::vector<std::pair<size_t, size_t> >& contour_cubic_ranges,
                                std::vector<CornerRecord>& corners,
                                std::vector<EdgeRecord>& edges)
{
    NSData* font_data = [NSData dataWithContentsOfFile:font_path];
    if (font_data == nil)
    {
        return false;
    }

    stbtt_fontinfo font_info;
    if (!stbtt_InitFont(&font_info, (const unsigned char*)font_data.bytes, 0))
    {
        return false;
    }

    float pixel_height = (float)canvas_height * 0.62f;
    float scale = stbtt_ScaleForPixelHeight(&font_info, pixel_height);
    float pen_x = 0.0f;
    size_t text_len = strlen(text);
    for (size_t i = 0; i < text_len; ++i)
    {
        int codepoint = (unsigned char)text[i];
        int glyph_index = stbtt_FindGlyphIndex(&font_info, codepoint);

        stbtt_vertex* vertices = nullptr;
        int vertex_count = stbtt_GetGlyphShape(&font_info, glyph_index, &vertices);
        if (vertex_count < 0)
        {
            return false;
        }

        bool contour_open = false;
        size_t contour_start_index = quadratics.size();
        size_t contour_start_cubic_index = cubics.size();
        uint32_t next_source_segment_id = 0;
        GlyphPoint current = { 0.0f, 0.0f };
        GlyphPoint contour_start = { 0.0f, 0.0f };

        for (int vi = 0; vi < vertex_count; ++vi)
        {
            const stbtt_vertex& vertex = vertices[vi];
            switch (vertex.type)
            {
                case STBTT_vmove:
                {
                    if (contour_open)
                    {
                        FinalizeContour(quadratics,
                                        cubics,
                                        contour_start_index,
                                        contour_start_cubic_index,
                        current,
                        contour_start,
                        feather_radius,
                        next_source_segment_id,
                        contours,
                        contour_cubic_ranges,
                        corners,
                        edges);
                    }
                    current = MakeGlyphPoint(pen_x, (float)vertex.x, (float)vertex.y, scale);
                    contour_start = current;
                    contour_open = true;
                    contour_start_index = quadratics.size();
                    contour_start_cubic_index = cubics.size();
                    break;
                }
                case STBTT_vline:
                {
                    GlyphPoint next = MakeGlyphPoint(pen_x, (float)vertex.x, (float)vertex.y, scale);
                    GlyphPoint control =
                    {
                        (current.m_X + next.m_X) * 0.5f,
                        (current.m_Y + next.m_Y) * 0.5f
                    };
                    AppendMonotonicQuadratic(quadratics,
                                             cubics,
                                             current,
                                             control,
                                             next,
                                             next_source_segment_id++);
                    current = next;
                    break;
                }
                case STBTT_vcurve:
                {
                    GlyphPoint control = MakeGlyphPoint(pen_x, (float)vertex.cx, (float)vertex.cy, scale);
                    GlyphPoint next = MakeGlyphPoint(pen_x, (float)vertex.x, (float)vertex.y, scale);
                    AppendMonotonicQuadratic(quadratics,
                                             cubics,
                                             current,
                                             control,
                                             next,
                                             next_source_segment_id++);
                    current = next;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        if (contour_open)
        {
            FinalizeContour(quadratics,
                            cubics,
                            contour_start_index,
                            contour_start_cubic_index,
                            current,
                            contour_start,
                            feather_radius,
                            next_source_segment_id,
                            contours,
                            contour_cubic_ranges,
                            corners,
                            edges);
        }

        stbtt_FreeShape(&font_info, vertices);

        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&font_info, codepoint, &advance, &lsb);
        (void)lsb;

        float kern = 0.0f;
        if (i + 1 < text_len)
        {
            kern = (float)stbtt_GetCodepointKernAdvance(&font_info, codepoint, (unsigned char)text[i + 1]);
        }

        pen_x += (advance + kern) * scale;
    }

    if (quadratics.empty())
    {
        return false;
    }

    float min_x = quadratics[0].m_P0X;
    float min_y = quadratics[0].m_P0Y;
    float max_x = quadratics[0].m_P0X;
    float max_y = quadratics[0].m_P0Y;

    for (const QuadraticRecord& quad : quadratics)
    {
        min_x = fminf(min_x, fminf(quad.m_P0X, fminf(quad.m_P1X, quad.m_P2X)));
        min_y = fminf(min_y, fminf(quad.m_P0Y, fminf(quad.m_P1Y, quad.m_P2Y)));
        max_x = fmaxf(max_x, fmaxf(quad.m_P0X, fmaxf(quad.m_P1X, quad.m_P2X)));
        max_y = fmaxf(max_y, fmaxf(quad.m_P0Y, fmaxf(quad.m_P1Y, quad.m_P2Y)));
    }

    float offset_x = ((float)canvas_width - (max_x - min_x)) * 0.5f - min_x;
    float offset_y = ((float)canvas_height - (max_y - min_y)) * 0.5f - min_y;
    for (QuadraticRecord& quad : quadratics)
    {
        quad.m_P0X += offset_x;
        quad.m_P0Y += offset_y;
        quad.m_P1X += offset_x;
        quad.m_P1Y += offset_y;
        quad.m_P2X += offset_x;
        quad.m_P2Y += offset_y;
    }

    for (CubicRecord& cubic : cubics)
    {
        cubic.m_P0X += offset_x;
        cubic.m_P0Y += offset_y;
        cubic.m_P1X += offset_x;
        cubic.m_P1Y += offset_y;
        cubic.m_P2X += offset_x;
        cubic.m_P2Y += offset_y;
        cubic.m_P3X += offset_x;
        cubic.m_P3Y += offset_y;
    }

    for (std::vector<GlyphPoint>& contour : contours)
    {
        for (GlyphPoint& point : contour)
        {
            point.m_X += offset_x;
            point.m_Y += offset_y;
        }
    }

    for (CornerRecord& corner : corners)
    {
        corner.m_X += offset_x;
        corner.m_Y += offset_y;
    }

    for (EdgeRecord& edge : edges)
    {
        edge.m_X += offset_x;
        edge.m_Y += offset_y;
    }

    fprintf(stderr, "BuildTextQuadratics: quads=%zu corners=%zu edges=%zu\n",
            quadratics.size(),
            corners.size(),
            edges.size());

    return true;
}

@interface FeatherRenderer : NSObject
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> queue;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@property (nonatomic, strong) id<MTLRenderPipelineState> patchPipeline;
@property (nonatomic, strong) id<MTLTexture> shapeTexture;
@property (nonatomic, strong) id<MTLTexture> featherTexture;
@property (nonatomic) float featherRadius;
@property (nonatomic) vector_float4 backgroundColor;
@property (nonatomic) vector_float4 shapeColor;
- (instancetype)initWithDevice:(id<MTLDevice>)device
                         argv0:(const char*)argv0
                         width:(uint32_t)width
                        height:(uint32_t)height
                         error:(NSError**)error;
- (void)encodeRenderPass:(MTLRenderPassDescriptor*)pass
           commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                    size:(CGSize)size;
- (BOOL)renderPNGToPath:(NSString*)outputPath width:(uint32_t)width height:(uint32_t)height;
@end

@implementation FeatherRenderer

static const HarnessVertex g_FullscreenQuadVertices[6] =
{
    { { -1.0f, -1.0f }, { 0.0f, 0.0f } },
    { { -1.0f,  1.0f }, { 0.0f, 1.0f } },
    { {  1.0f, -1.0f }, { 1.0f, 0.0f } },
    { {  1.0f, -1.0f }, { 1.0f, 0.0f } },
    { { -1.0f,  1.0f }, { 0.0f, 1.0f } },
    { {  1.0f,  1.0f }, { 1.0f, 1.0f } }
};

- (id<MTLTexture>)createShapeTextureWithWidth:(uint32_t)canvas_width height:(uint32_t)canvas_height
{
    NSString* font_path = ResolveFontPath();
    if (font_path == nil)
    {
        return nil;
    }

    std::vector<QuadraticRecord> quadratics;
    std::vector<CubicRecord> cubics;
    std::vector<std::vector<GlyphPoint> > contours;
    std::vector<std::pair<size_t, size_t> > contour_cubic_ranges;
    std::vector<CornerRecord> corners;
    std::vector<EdgeRecord> edges;
    if (!BuildTextQuadratics(font_path,
                             "Lf",
                             canvas_width,
                             canvas_height,
                             self.featherRadius,
                             quadratics,
                             cubics,
                             contours,
                             contour_cubic_ranges,
                             corners,
                             edges))
    {
        return nil;
    }

    std::vector<float> curve_texels;
    curve_texels.resize(quadratics.size() * 8);
    for (size_t i = 0; i < quadratics.size(); ++i)
    {
        const QuadraticRecord& quad = quadratics[i];
        float* dst = curve_texels.data() + i * 8;
        dst[0] = quad.m_P0X;
        dst[1] = quad.m_P0Y;
        dst[2] = quad.m_P1X;
        dst[3] = quad.m_P1Y;
        dst[4] = quad.m_P2X;
        dst[5] = quad.m_P2Y;
        dst[6] = 0.0f;
        dst[7] = 0.0f;
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                           width:(NSUInteger)(quadratics.size() * 2)
                                                          height:1
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, (NSUInteger)(quadratics.size() * 2), 1);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:curve_texels.data()
               bytesPerRow:sizeof(float) * curve_texels.size()];
    return texture;
}

- (id<MTLTexture>)createCornerTextureWithWidth:(uint32_t)canvas_width height:(uint32_t)canvas_height
{
    NSString* font_path = ResolveFontPath();
    if (font_path == nil)
    {
        return nil;
    }

    std::vector<QuadraticRecord> quadratics;
    std::vector<CubicRecord> cubics;
    std::vector<std::vector<GlyphPoint> > contours;
    std::vector<std::pair<size_t, size_t> > contour_cubic_ranges;
    std::vector<CornerRecord> corners;
    std::vector<EdgeRecord> edges;
    if (!BuildTextQuadratics(font_path,
                             "Lf",
                             canvas_width,
                             canvas_height,
                             self.featherRadius,
                             quadratics,
                             cubics,
                             contours,
                             contour_cubic_ranges,
                             corners,
                             edges))
    {
        return nil;
    }

    if (corners.empty())
    {
        corners.push_back(CornerRecord{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f });
    }

    std::vector<float> texels;
    texels.resize(corners.size() * 8);
    for (size_t i = 0; i < corners.size(); ++i)
    {
        const CornerRecord& corner = corners[i];
        float* dst = texels.data() + i * 8;
        dst[0] = corner.m_X;
        dst[1] = corner.m_Y;
        dst[2] = corner.m_T0X;
        dst[3] = corner.m_T0Y;
        dst[4] = corner.m_T1X;
        dst[5] = corner.m_T1Y;
        dst[6] = corner.m_OrientationSign;
        dst[7] = 0.0f;
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                           width:(NSUInteger)(corners.size() * 2)
                                                          height:1
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, (NSUInteger)(corners.size() * 2), 1);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:texels.data()
               bytesPerRow:sizeof(float) * texels.size()];
    return texture;
}

- (id<MTLTexture>)createEdgeTextureWithWidth:(uint32_t)canvas_width height:(uint32_t)canvas_height
{
    NSString* font_path = ResolveFontPath();
    if (font_path == nil)
    {
        return nil;
    }

    std::vector<QuadraticRecord> quadratics;
    std::vector<CubicRecord> cubics;
    std::vector<std::vector<GlyphPoint> > contours;
    std::vector<std::pair<size_t, size_t> > contour_cubic_ranges;
    std::vector<CornerRecord> corners;
    std::vector<EdgeRecord> edges;
    if (!BuildTextQuadratics(font_path,
                             "Lf",
                             canvas_width,
                             canvas_height,
                             self.featherRadius,
                             quadratics,
                             cubics,
                             contours,
                             contour_cubic_ranges,
                             corners,
                             edges))
    {
        return nil;
    }

    if (edges.empty())
    {
        edges.push_back(EdgeRecord{ 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0u });
    }

    std::vector<float> texels;
    texels.resize(edges.size() * 8);
    for (size_t i = 0; i < edges.size(); ++i)
    {
        const EdgeRecord& edge = edges[i];
        float* dst = texels.data() + i * 8;
        dst[0] = edge.m_X;
        dst[1] = edge.m_Y;
        dst[2] = edge.m_Tx;
        dst[3] = edge.m_Ty;
        dst[4] = edge.m_Nx;
        dst[5] = edge.m_Ny;
        dst[6] = edge.m_Length;
        dst[7] = (float)edge.m_SourceCurveIndex;
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                           width:(NSUInteger)(edges.size() * 2)
                                                          height:1
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, (NSUInteger)(edges.size() * 2), 1);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:texels.data()
               bytesPerRow:sizeof(float) * texels.size()];
    return texture;
}

- (id<MTLTexture>)createFeatherTexture
{
    const uint32_t kTextureWidth = 512;
    const uint32_t kTextureHeight = 2;
    const float kStdDevs = 3.0f;

    std::vector<float> data;
    data.resize(kTextureWidth * kTextureHeight);

    for (uint32_t x = 0; x < kTextureWidth; ++x)
    {
        float u = ((float)x + 0.5f) / (float)kTextureWidth;
        float stddev_x = (u * 2.0f - 1.0f) * kStdDevs;
        data[x] = CpuNormalCDF(stddev_x);
    }

    for (uint32_t x = 0; x < kTextureWidth; ++x)
    {
        float target = ((float)x + 0.5f) / (float)kTextureWidth;
        uint32_t best_index = 0;
        while (best_index + 1 < kTextureWidth && data[best_index] < target)
        {
            ++best_index;
        }
        data[kTextureWidth + x] = ((float)best_index + 0.5f) / (float)kTextureWidth;
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                                           width:kTextureWidth
                                                          height:kTextureHeight
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, kTextureWidth, kTextureHeight);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:data.data()
               bytesPerRow:sizeof(float) * kTextureWidth];
    return texture;
}

- (id<MTLTexture>)createRiveContourTexture:(const std::vector<RiveContourDataRecord>&)records
{
    NSUInteger width = std::max<size_t>(records.size(), 1);
    std::vector<uint32_t> texels(width * 4, 0u);
    if (!records.empty())
    {
        memcpy(texels.data(), records.data(), records.size() * sizeof(RiveContourDataRecord));
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Uint
                                                           width:width
                                                          height:1
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, 1);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:texels.data()
               bytesPerRow:sizeof(uint32_t) * width * 4];
    return texture;
}

- (id<MTLTexture>)createRivePathTexture:(const RivePathDataRecord&)record
{
    std::vector<uint32_t> texels(16, 0u);
    memcpy(texels.data(), &record, sizeof(record));

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Uint
                                                           width:4
                                                          height:1
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, 4, 1);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:texels.data()
               bytesPerRow:sizeof(uint32_t) * 4 * 4];
    return texture;
}

- (id<MTLTexture>)createRiveTessTexture:(const std::vector<RiveTessVertexRecord>&)records
{
    NSUInteger width = std::max<size_t>(records.size() * 4, 4);
    std::vector<uint32_t> texels(width * 4, 0u);
    if (!records.empty())
    {
        memcpy(texels.data(), records.data(), records.size() * sizeof(RiveTessVertexRecord));
    }

    MTLTextureDescriptor* desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Uint
                                                           width:width
                                                          height:1
                                                       mipmapped:NO];
    desc.usage = MTLTextureUsageShaderRead;
    desc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> texture = [self.device newTextureWithDescriptor:desc];
    if (texture == nil)
    {
        return nil;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, 1);
        [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:texels.data()
               bytesPerRow:sizeof(uint32_t) * width * 4];
    return texture;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device
                         argv0:(const char*)argv0
                         width:(uint32_t)width
                        height:(uint32_t)height
                         error:(NSError**)error
{
    self = [super init];
    if (!self)
    {
        return nil;
    }

    self.device = device;
    self.queue = [device newCommandQueue];
    self.featherRadius = 28.0f;
    self.backgroundColor = (vector_float4){ 0.25f, 0.25f, 0.25f, 1.0f };
    self.shapeColor = (vector_float4){ 1.0f, 1.0f, 1.0f, 1.0f };
    self.shapeTexture = [self createShapeTextureWithWidth:width height:height];
    self.featherTexture = [self createFeatherTexture];

    if (self.shapeTexture == nil || self.featherTexture == nil)
    {
        if (error)
        {
            *error = [NSError errorWithDomain:@"FeatherHarness"
                                         code:3
                                     userInfo:@{NSLocalizedDescriptionKey : @"Failed to create glyph shape or feather texture"}];
        }
        return nil;
    }

    NSString* shaderPath = ResolveShaderPath(argv0);
    if (shaderPath == nil)
    {
        if (error)
        {
            *error = [NSError errorWithDomain:@"FeatherHarness"
                                         code:1
                                     userInfo:@{NSLocalizedDescriptionKey : @"Failed to locate feather_shader.metal"}];
        }
        return nil;
    }

    NSError* readError = nil;
    NSString* shaderSource = [NSString stringWithContentsOfFile:shaderPath
                                                       encoding:NSUTF8StringEncoding
                                                          error:&readError];
    if (shaderSource == nil)
    {
        if (error)
        {
            *error = readError;
        }
        return nil;
    }

    MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource options:options error:error];
    if (library == nil)
    {
        return nil;
    }

    id<MTLFunction> vertexFn = [library newFunctionWithName:@"vertex_fill_main"];
    id<MTLFunction> fragmentFn = [library newFunctionWithName:@"fragment_fill_main"];
    id<MTLFunction> patchVertexFn = [library newFunctionWithName:@"vertex_patch_main"];
    id<MTLFunction> patchFragmentFn = [library newFunctionWithName:@"fragment_patch_main"];
    if (vertexFn == nil || fragmentFn == nil || patchVertexFn == nil || patchFragmentFn == nil)
    {
        if (error)
        {
            *error = [NSError errorWithDomain:@"FeatherHarness"
                                         code:2
                                     userInfo:@{NSLocalizedDescriptionKey : @"Failed to find Metal shader entry points"}];
        }
        return nil;
    }

    MTLRenderPipelineDescriptor* pipelineDesc = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDesc.vertexFunction = vertexFn;
    pipelineDesc.fragmentFunction = fragmentFn;
    pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    self.pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:error];
    if (self.pipeline == nil)
    {
        return nil;
    }

    MTLRenderPipelineDescriptor* patchDesc = [[MTLRenderPipelineDescriptor alloc] init];
    patchDesc.vertexFunction = patchVertexFn;
    patchDesc.fragmentFunction = patchFragmentFn;
    patchDesc.colorAttachments[0].pixelFormat = MTLPixelFormatR32Float;
    patchDesc.colorAttachments[0].blendingEnabled = YES;
    patchDesc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    patchDesc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    patchDesc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorOne;
    patchDesc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
    patchDesc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOne;
    patchDesc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;

    self.patchPipeline = [device newRenderPipelineStateWithDescriptor:patchDesc error:error];
    if (self.patchPipeline == nil)
    {
        return nil;
    }

    return self;
}

- (void)encodeRenderPass:(MTLRenderPassDescriptor*)pass
           commandBuffer:(id<MTLCommandBuffer>)commandBuffer
                    size:(CGSize)size
{
    FeatherUniforms uniforms;
    uniforms.m_Resolution = { (float)size.width, (float)size.height };
    uniforms.m_FeatherRadius = self.featherRadius;
    uniforms.m_Padding0 = 0.0f;
    uniforms.m_BackgroundColor = self.backgroundColor;
    uniforms.m_ShapeColor = self.shapeColor;

    std::vector<RiveContourDataRecord> contour_records;
    RivePathDataRecord path_record = BuildRivePathDataRecord(self.featherRadius);
    std::vector<RiveTessVertexRecord> tess_records;
    std::vector<uint32_t> edge_start_indices;
    std::vector<uint32_t> edge_end_indices;
    std::vector<uint32_t> edge_contour_indices;
    std::vector<EdgeRecord> edges;
    NSString* font_path = ResolveFontPath();
    if (font_path != nil)
    {
        std::vector<QuadraticRecord> quadratics;
        std::vector<CubicRecord> cubics;
        std::vector<std::vector<GlyphPoint> > contours;
        std::vector<std::pair<size_t, size_t> > contour_cubic_ranges;
        std::vector<CornerRecord> corners;
        if (BuildTextQuadratics(font_path,
                                "Lf",
                                (uint32_t)size.width,
                                (uint32_t)size.height,
                                self.featherRadius,
                                quadratics,
                                cubics,
                                contours,
                                contour_cubic_ranges,
                                corners,
                                edges))
        {
            if (BuildRiveLikeContourAndTessData(contours,
                                                cubics,
                                                contour_cubic_ranges,
                                                self.featherRadius,
                                                contour_records,
                                                tess_records,
                                                edge_start_indices,
                                                edge_end_indices,
                                                edge_contour_indices))
            {
            }
        }
    }
    fprintf(stderr, "patch tess records: %zu contour records: %zu edges: %zu\n",
            tess_records.size(), contour_records.size(), edges.size());

    std::vector<RivePatchVertex> rive_template_vertices;
    std::vector<uint16_t> rive_template_indices;
    GenerateRivePatchBufferData(rive_template_vertices, rive_template_indices);

    std::vector<RivePatchVertex> patch_vertices = rive_template_vertices;

    id<MTLTexture> contour_texture = contour_records.empty() ? nil : [self createRiveContourTexture:contour_records];
    id<MTLTexture> path_texture = [self createRivePathTexture:path_record];
    id<MTLTexture> tess_texture = tess_records.empty() ? nil : [self createRiveTessTexture:tess_records];

    MTLTextureDescriptor* coverage_desc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                                           width:(NSUInteger)size.width
                                                          height:(NSUInteger)size.height
                                                       mipmapped:NO];
    coverage_desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    coverage_desc.storageMode = MTLStorageModeManaged;
    id<MTLTexture> coverage_texture = [self.device newTextureWithDescriptor:coverage_desc];

    if (coverage_texture != nil && contour_texture != nil && path_texture != nil && tess_texture != nil && !patch_vertices.empty())
    {
        MTLRenderPassDescriptor* coverage_pass = [MTLRenderPassDescriptor renderPassDescriptor];
        coverage_pass.colorAttachments[0].texture = coverage_texture;
        coverage_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        coverage_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        coverage_pass.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);

        id<MTLRenderCommandEncoder> coverage_encoder =
            [commandBuffer renderCommandEncoderWithDescriptor:coverage_pass];
        id<MTLBuffer> patch_buffer = [self.device newBufferWithBytes:patch_vertices.data()
                                                              length:patch_vertices.size() * sizeof(RivePatchVertex)
                                                             options:MTLResourceStorageModeShared];
        if (patch_buffer != nil)
        {
            id<MTLBuffer> patch_index_buffer = [self.device newBufferWithBytes:rive_template_indices.data()
                                                                        length:rive_template_indices.size() * sizeof(uint16_t)
                                                                       options:MTLResourceStorageModeShared];
            [coverage_encoder setRenderPipelineState:self.patchPipeline];
            [coverage_encoder setVertexBuffer:patch_buffer offset:0 atIndex:0];
            [coverage_encoder setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:1];
            [coverage_encoder setVertexTexture:tess_texture atIndex:0];
            [coverage_encoder setVertexTexture:contour_texture atIndex:1];
            [coverage_encoder setVertexTexture:path_texture atIndex:2];
            [coverage_encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
            [coverage_encoder setFragmentTexture:self.featherTexture atIndex:1];
            if (patch_index_buffer != nil)
            {
                // Use the full Rive patch template so the family roles can
                // participate with the span-based tess data.
                [coverage_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                             indexCount:(NSUInteger)rive_template_indices.size()
                                              indexType:MTLIndexTypeUInt16
                                            indexBuffer:patch_index_buffer
                                      indexBufferOffset:0
                                        instanceCount:(NSUInteger)tess_records.size()];
            }
        }
        [coverage_encoder endEncoding];
    }

    id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
    [encoder setRenderPipelineState:self.pipeline];
    [encoder setVertexBytes:g_FullscreenQuadVertices length:sizeof(g_FullscreenQuadVertices) atIndex:0];
    [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
    [encoder setFragmentTexture:self.shapeTexture atIndex:0];
    [encoder setFragmentTexture:coverage_texture atIndex:1];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
    [encoder endEncoding];
}

- (BOOL)renderPNGToPath:(NSString*)outputPath width:(uint32_t)width height:(uint32_t)height
{
    MTLTextureDescriptor* textureDesc =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                           width:width
                                                          height:height
                                                       mipmapped:NO];
    textureDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    textureDesc.storageMode = MTLStorageModeManaged;

    id<MTLTexture> renderTexture = [self.device newTextureWithDescriptor:textureDesc];
    if (renderTexture == nil)
    {
        return NO;
    }

    MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = renderTexture;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(self.backgroundColor[0],
                                                            self.backgroundColor[1],
                                                            self.backgroundColor[2],
                                                            self.backgroundColor[3]);

    id<MTLCommandBuffer> commandBuffer = [self.queue commandBuffer];
    [self encodeRenderPass:pass commandBuffer:commandBuffer size:CGSizeMake(width, height)];
    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    size_t bytesPerRow = width * 4;
    size_t dataSize = bytesPerRow * height;
    NSMutableData* pixelData = [NSMutableData dataWithLength:dataSize];
    if (pixelData == nil)
    {
        return NO;
    }

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    [renderTexture getBytes:[pixelData mutableBytes] bytesPerRow:bytesPerRow fromRegion:region mipmapLevel:0];

    NSString* directory = [outputPath stringByDeletingLastPathComponent];
    if (directory.length > 0)
    {
        [[NSFileManager defaultManager] createDirectoryAtPath:directory
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:nil];
    }

    return WritePNG(outputPath, (const uint8_t*)[pixelData bytes], width, height);
}

@end

@interface FeatherInteractiveView : MTKView <MTKViewDelegate>
@property (nonatomic, strong) FeatherRenderer* renderer;
@property (nonatomic, strong) NSTextField* label;
@property (nonatomic, copy) NSString* outputTemplatePath;
@property (nonatomic) BOOL dragging;
@property (nonatomic) CGFloat dragStartX;
@property (nonatomic) float dragStartFeather;
- (instancetype)initWithFrame:(NSRect)frame
                       device:(id<MTLDevice>)device
                     renderer:(FeatherRenderer*)renderer
           outputTemplatePath:(NSString*)outputTemplatePath;
@end

@implementation FeatherInteractiveView

- (instancetype)initWithFrame:(NSRect)frame
                       device:(id<MTLDevice>)device
                     renderer:(FeatherRenderer*)renderer
           outputTemplatePath:(NSString*)outputTemplatePath
{
    self = [super initWithFrame:frame device:device];
    if (!self)
    {
        return nil;
    }

    self.renderer = renderer;
    self.outputTemplatePath = outputTemplatePath;
    self.delegate = self;
    self.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.clearColor = MTLClearColorMake(renderer.backgroundColor[0],
                                        renderer.backgroundColor[1],
                                        renderer.backgroundColor[2],
                                        renderer.backgroundColor[3]);
    self.enableSetNeedsDisplay = NO;
    self.paused = NO;
    self.preferredFramesPerSecond = 60;

    NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 180, 28)];
    label.bezeled = NO;
    label.drawsBackground = YES;
    label.backgroundColor = [NSColor colorWithWhite:0.0 alpha:0.30];
    label.textColor = [NSColor whiteColor];
    label.editable = NO;
    label.selectable = NO;
    label.font = [NSFont monospacedDigitSystemFontOfSize:13 weight:NSFontWeightMedium];
    label.alignment = NSTextAlignmentRight;
    label.autoresizingMask = NSViewMinXMargin | NSViewMinYMargin;
    self.label = label;
    [self addSubview:label];
    [self updateLabel];

    return self;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self.window makeFirstResponder:self];
}

- (void)layout
{
    [super layout];
    CGFloat padding = 16.0;
    CGFloat labelWidth = 180.0;
    CGFloat labelHeight = 28.0;
    NSRect bounds = self.bounds;
    self.label.frame = NSMakeRect(NSMaxX(bounds) - labelWidth - padding,
                                  NSMaxY(bounds) - labelHeight - padding,
                                  labelWidth,
                                  labelHeight);
}

- (void)updateLabel
{
    self.label.stringValue = [NSString stringWithFormat:@"Feather: %.1f", self.renderer.featherRadius];
}

- (void)adjustFeatherBy:(float)delta
{
    self.renderer.featherRadius = fmaxf(0.0f, self.renderer.featherRadius + delta);
    [self updateLabel];
}

- (void)saveSnapshot
{
    NSString* outputPath = MakeNextIterationPath(self.outputTemplatePath);
    CGSize size = self.drawableSize;
    if ([self.renderer renderPNGToPath:outputPath width:(uint32_t)size.width height:(uint32_t)size.height])
    {
        self.label.stringValue = [NSString stringWithFormat:@"Saved: %@", [outputPath lastPathComponent]];
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            [self updateLabel];
        });
    }
}

- (void)keyDown:(NSEvent*)event
{
    switch (event.keyCode)
    {
        case 123: // left
        case 125: // down
            [self adjustFeatherBy:-1.0f];
            return;
        case 124: // right
        case 126: // up
            [self adjustFeatherBy:1.0f];
            return;
        default:
            break;
    }

    NSString* chars = event.charactersIgnoringModifiers.lowercaseString;
    if ([chars isEqualToString:@"s"])
    {
        [self saveSnapshot];
        return;
    }
    if ([chars isEqualToString:@"+"] || [chars isEqualToString:@"="])
    {
        [self adjustFeatherBy:1.0f];
        return;
    }
    if ([chars isEqualToString:@"-"] || [chars isEqualToString:@"_"])
    {
        [self adjustFeatherBy:-1.0f];
        return;
    }
}

- (void)mouseDown:(NSEvent*)event
{
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    self.dragging = YES;
    self.dragStartX = p.x;
    self.dragStartFeather = self.renderer.featherRadius;
}

- (void)mouseDragged:(NSEvent*)event
{
    if (!self.dragging)
    {
        return;
    }

    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    float delta = (float)(p.x - self.dragStartX) * 0.1f;
    self.renderer.featherRadius = fmaxf(0.0f, self.dragStartFeather + delta);
    [self updateLabel];
}

- (void)mouseUp:(NSEvent*)event
{
    (void)event;
    self.dragging = NO;
}

- (void)scrollWheel:(NSEvent*)event
{
    [self adjustFeatherBy:(float)event.deltaY];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size
{
    (void)view;
    (void)size;
}

- (void)drawInMTKView:(MTKView*)view
{
    id<CAMetalDrawable> drawable = view.currentDrawable;
    MTLRenderPassDescriptor* pass = view.currentRenderPassDescriptor;
    if (drawable == nil || pass == nil)
    {
        return;
    }

    pass.colorAttachments[0].clearColor = MTLClearColorMake(self.renderer.backgroundColor[0],
                                                            self.renderer.backgroundColor[1],
                                                            self.renderer.backgroundColor[2],
                                                            self.renderer.backgroundColor[3]);

    id<MTLCommandBuffer> commandBuffer = [self.renderer.queue commandBuffer];
    [self.renderer encodeRenderPass:pass commandBuffer:commandBuffer size:view.drawableSize];
    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
}

@end

@interface FeatherAppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) FeatherRenderer* renderer;
@property (nonatomic) uint32_t width;
@property (nonatomic) uint32_t height;
@property (nonatomic, copy) NSString* outputTemplatePath;
@property (nonatomic, strong) NSWindow* window;
@end

@implementation FeatherAppDelegate

- (instancetype)initWithRenderer:(FeatherRenderer*)renderer
                           width:(uint32_t)width
                          height:(uint32_t)height
              outputTemplatePath:(NSString*)outputTemplatePath
{
    self = [super init];
    if (!self)
    {
        return nil;
    }

    self.renderer = renderer;
    self.width = width;
    self.height = height;
    self.outputTemplatePath = outputTemplatePath;
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification
{
    (void)notification;

    NSRect frame = NSMakeRect(0, 0, self.width, self.height);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:(NSWindowStyleMaskTitled |
                                                         NSWindowStyleMaskClosable |
                                                         NSWindowStyleMaskResizable |
                                                         NSWindowStyleMaskMiniaturizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.title = @"Feather Harness";
    self.window.minSize = NSMakeSize(320, 240);

    FeatherInteractiveView* view =
        [[FeatherInteractiveView alloc] initWithFrame:frame
                                               device:self.renderer.device
                                             renderer:self.renderer
                                   outputTemplatePath:self.outputTemplatePath];
    view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.window.contentView = view;
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender
{
    (void)sender;
    return YES;
}

@end

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        uint32_t width = 512;
        uint32_t height = 512;
        float featherRadius = 28.0f;
        NSString* outputPath = @"build/l_shape.png";
        bool interactive = false;

        for (int i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            {
                PrintUsage(argv[0]);
                return 0;
            }
            else if (strcmp(argv[i], "--interactive") == 0)
            {
                interactive = true;
            }
            else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc)
            {
                width = (uint32_t)strtoul(argv[++i], nullptr, 10);
            }
            else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc)
            {
                height = (uint32_t)strtoul(argv[++i], nullptr, 10);
            }
            else if (strcmp(argv[i], "--feather") == 0 && i + 1 < argc)
            {
                featherRadius = strtof(argv[++i], nullptr);
            }
            else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
            {
                outputPath = [NSString stringWithUTF8String:argv[++i]];
            }
            else
            {
                fprintf(stderr, "Unknown argument: %s\n", argv[i]);
                PrintUsage(argv[0]);
                return 1;
            }
        }

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil)
        {
            fprintf(stderr, "Failed to create Metal device.\n");
            return 1;
        }

        NSError* rendererError = nil;
        FeatherRenderer* renderer = [[FeatherRenderer alloc] initWithDevice:device
                                                                      argv0:argv[0]
                                                                      width:width
                                                                     height:height
                                                                      error:&rendererError];
        if (renderer == nil)
        {
            fprintf(stderr, "Failed to initialize renderer: %s\n", [[rendererError localizedDescription] UTF8String]);
            return 1;
        }
        renderer.featherRadius = featherRadius;

        if (interactive)
        {
            [NSApplication sharedApplication];
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

            FeatherAppDelegate* delegate = [[FeatherAppDelegate alloc] initWithRenderer:renderer
                                                                                  width:width
                                                                                 height:height
                                                                     outputTemplatePath:outputPath];
            [NSApp setDelegate:delegate];
            [NSApp run];
            return 0;
        }

        if (![renderer renderPNGToPath:outputPath width:width height:height])
        {
            fprintf(stderr, "Failed to render PNG.\n");
            return 1;
        }

        printf("Wrote %s\n", [outputPath UTF8String]);
        return 0;
    }
}
