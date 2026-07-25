#include <metal_stdlib>
using namespace metal;

constant float PI = 3.14159265359;
constant float _2PI = 6.28318530718;
constant float PI_OVER_2 = 1.57079632679;
constant float FEATHER_COVERAGE_BIAS = -2.0;
constant float FEATHER_COVERAGE_THRESHOLD = -1.5;
constant float FEATHER_X_COORD_BIAS = 0.25;
constant float HORIZONTAL_COTANGENT_THRESHOLD = 1e3;
constant float HORIZONTAL_COTANGENT_VALUE = HORIZONTAL_COTANGENT_THRESHOLD * HORIZONTAL_COTANGENT_THRESHOLD;
constant float FEATHER_JOIN_HELPER_VERTEX_COUNT = 3.0;

struct FeatherUniforms
{
    float2 resolution;
    float feather_radius;
    float padding0;
    float4 background_color;
    float4 shape_color;
};

struct FullscreenVertexIn
{
    float2 position;
    float2 uv;
};

struct FillVSOut
{
    float4 position [[position]];
    float2 uv;
};

struct PatchVertexIn
{
    float localVertexID;
    float outset;
    float fillCoverage;
    float params;
    float mirroredLocalVertexID;
    float mirroredOutset;
    float mirroredFillCoverage;
    int padding;
};

struct RiveTessSpan
{
    float2 origin;
    float  theta;
    uint   contourIDWithFlags;
};

struct PatchVSOut
{
    float4 position [[position]];
    float4 coverages;
    float patchRole;
};

static inline float4 pack_feathered_fill_coverages(float cornerTheta,
                                                   float2 spokeNorm,
                                                   float outset)
{
    float2 cornerLocalCoord = (1.0 - spokeNorm * abs(outset)) * 0.5;

    float cotTheta;
    float y0;
    if (abs(cornerTheta - PI_OVER_2) < 1.0 / HORIZONTAL_COTANGENT_THRESHOLD)
    {
        cotTheta = 0.0;
        y0 = 0.0;
    }
    else
    {
        float tanTheta = tan(cornerTheta);
        cotTheta = sign(PI_OVER_2 - cornerTheta) /
                   max(abs(tanTheta), 1.0 / HORIZONTAL_COTANGENT_VALUE);
        y0 = cotTheta >= 0.0
            ? cornerLocalCoord.y - (1.0 - cornerLocalCoord.x) * tanTheta
            : cornerLocalCoord.y + cornerLocalCoord.x * tanTheta;
    }

    float4 coverages;
    coverages.x = max(cornerLocalCoord.x, 0.0) + FEATHER_X_COORD_BIAS;
    coverages.y = -cornerLocalCoord.y + FEATHER_COVERAGE_BIAS;
    coverages.z = cotTheta;
    coverages.w = y0;
    return coverages;
}

vertex FillVSOut vertex_fill_main(const device FullscreenVertexIn* vertices [[buffer(0)]],
                                  uint vertex_id [[vertex_id]])
{
    FullscreenVertexIn v = vertices[vertex_id];

    FillVSOut out;
    out.position = float4(v.position, 0.0, 1.0);
    out.uv = v.uv;
    return out;
}

vertex PatchVSOut vertex_patch_main(const device PatchVertexIn* vertices [[buffer(0)]],
                                    constant FeatherUniforms& uniforms [[buffer(1)]],
                                    texture2d<uint, access::read> tess_texture [[texture(0)]],
                                    texture2d<uint, access::read> contour_texture [[texture(1)]],
                                    texture2d<uint, access::read> path_texture [[texture(2)]],
                                    uint vertex_id [[vertex_id]],
                                    uint instance_id [[instance_id]])
{
    PatchVertexIn v = vertices[vertex_id];

    PatchVSOut out;
    out.position = float4(0.0, 0.0, 0.0, 1.0);
    out.coverages = float4(0.0);

    uint patchParams = as_type<uint>(v.params);
    out.patchRole = float(patchParams & 3u);
    float patchSegmentSpan = float(patchParams >> 2);
    uint tess_base = instance_id * 4u;
    uint4 tess_texel0 = tess_texture.read(uint2(tess_base + 0u, 0u));
    uint4 tess_texel1 = tess_texture.read(uint2(tess_base + 1u, 0u));
    uint4 tess_texel2 = tess_texture.read(uint2(tess_base + 2u, 0u));
    uint4 tess_texel3 = tess_texture.read(uint2(tess_base + 3u, 0u));
    uint contour_id = max(tess_texel3.w & 0xffffu, 1u);
    uint4 contour_data = contour_texture.read(uint2(contour_id - 1u, 0u));
    float2 midpoint = float2(as_type<float>(contour_data.x), as_type<float>(contour_data.y));
    uint4 path_texel0 = path_texture.read(uint2(0u, 0u));
    uint4 path_texel1 = path_texture.read(uint2(1u, 0u));
    float2x2 path_matrix = float2x2(float2(as_type<float>(path_texel0.x),
                                           as_type<float>(path_texel0.y)),
                                    float2(as_type<float>(path_texel0.z),
                                           as_type<float>(path_texel0.w)));
    float2 p0 = float2(as_type<float>(tess_texel0.x), as_type<float>(tess_texel0.y));
    float2 p1 = float2(as_type<float>(tess_texel0.z), as_type<float>(tess_texel0.w));
    float2 p2 = float2(as_type<float>(tess_texel1.x), as_type<float>(tess_texel1.y));
    float2 p3 = float2(as_type<float>(tess_texel1.z), as_type<float>(tess_texel1.w));
    float2 join_tangent = normalize(float2(as_type<float>(tess_texel2.x), as_type<float>(tess_texel2.y)));
    uint parametric_segment_count = max(tess_texel3.z & 0x3ffu, 1u);
    float2 translate = float2(as_type<float>(path_texel1.x), as_type<float>(path_texel1.y));
    float strokeRadius = as_type<float>(path_texel1.z);
    float featherRadius = as_type<float>(path_texel1.w);
    float t = clamp(v.localVertexID / max(patchSegmentSpan - 1.0, 1.0), 0.0, 1.0);
    float2 ab = mix(p0, p1, t);
    float2 bc = mix(p1, p2, t);
    float2 cd = mix(p2, p3, t);
    float2 abc = mix(ab, bc, t);
    float2 bcd = mix(bc, cd, t);
    float2 origin = mix(abc, bcd, t);
    float2 tangent = normalize(bcd - abc);
    float theta = atan2(tangent.x, -tangent.y);
    float2 norm = float2(sin(theta), -cos(theta));
    if (out.patchRole > 1.5)
    {
        origin = midpoint;
    }
    else if (out.patchRole < 0.5)
    {
        origin += norm * (v.mirroredOutset * featherRadius);
    }
    out.coverages = float4(v.mirroredFillCoverage,
                           FEATHER_COVERAGE_BIAS,
                           HORIZONTAL_COTANGENT_VALUE,
                           v.mirroredFillCoverage);

    origin = float2(dot(path_matrix[0], origin), dot(path_matrix[1], origin)) +
             translate;

    float2 clip = float2(origin.x / uniforms.resolution.x * 2.0 - 1.0,
                         origin.y / uniforms.resolution.y * 2.0 - 1.0);
    out.position = float4(clip, 0.0, 1.0);
    return out;
}

static inline float feather(float signed_distance,
                            float feather_radius,
                            texture2d<float, access::sample> feather_texture)
{
    if (feather_radius <= 0.0)
    {
        return signed_distance >= 0.0 ? 1.0 : 0.0;
    }

    constexpr sampler feather_sampler(coord::normalized,
                                      address::clamp_to_edge,
                                      filter::linear);
    float u = clamp(0.5 + 0.5 * (signed_distance / feather_radius), 0.0, 1.0);
    return feather_texture.sample(feather_sampler, float2(u, 0.25)).x;
}

static inline float eval_feathered_fill(float4 coverages,
                                        texture2d<float, access::sample> feather_texture)
{
    float cotTheta = coverages.z;
    float y0 = max(coverages.w, 0.0);

    float featherCoverage = cotTheta >= 0.0 ? feather(y0 * 6.0 - 3.0, 3.0, feather_texture) : 0.0;

    if (abs(cotTheta) < HORIZONTAL_COTANGENT_THRESHOLD)
    {
        float x = abs(coverages.x) - FEATHER_X_COORD_BIAS;
        float y = -coverages.y + FEATHER_COVERAGE_BIAS;

        float dt = (y - y0) * 0.5984134206;

        float4 t = y0 + dt * float4(0.20888568955,
                                    0.62665706865,
                                    1.04442844776,
                                    1.46219982687);

        float4 u = t * -cotTheta + (y * cotTheta + x);
        float4 feathers = float4(feather(u.x * 6.0 - 3.0, 3.0, feather_texture),
                                 feather(u.y * 6.0 - 3.0, 3.0, feather_texture),
                                 feather(u.z * 6.0 - 3.0, 3.0, feather_texture),
                                 feather(u.w * 6.0 - 3.0, 3.0, feather_texture));

        float4 t_ = t * 5.09593080173 + -2.54796540086;
        float4 ddtFeather = exp2(-t_ * t_);

        featherCoverage += dot(feathers, ddtFeather) * dt;
    }

    return featherCoverage;
}

static inline void read_quadratic(texture2d<float, access::read> shape_texture,
                                  uint curve_index,
                                  thread float2& p0,
                                  thread float2& p1,
                                  thread float2& p2)
{
    float4 texel0 = shape_texture.read(uint2(curve_index * 2 + 0, 0));
    float4 texel1 = shape_texture.read(uint2(curve_index * 2 + 1, 0));
    p0 = texel0.xy;
    p1 = texel0.zw;
    p2 = texel1.xy;
}

static inline float2 eval_quadratic(float2 p0, float2 p1, float2 p2, float t)
{
    float s = 1.0 - t;
    return s * s * p0 + 2.0 * s * t * p1 + t * t * p2;
}

static inline float2 eval_quadratic_derivative(float2 p0, float2 p1, float2 p2, float t)
{
    return 2.0 * mix(p1 - p0, p2 - p1, t);
}

static inline bool point_in_quadratics(float2 p,
                                       texture2d<float, access::read> shape_texture)
{
    uint curve_count = shape_texture.get_width() / 2;
    int winding = 0;
    for (uint i = 0; i < curve_count; ++i)
    {
        float2 p0, p1, p2;
        read_quadratic(shape_texture, i, p0, p1, p2);

        float ay = p0.y - 2.0 * p1.y + p2.y;
        float by = 2.0 * (p1.y - p0.y);
        float cy = p0.y - p.y;

        float roots[2];
        uint root_count = 0;

        if (fabs(ay) < 1e-5)
        {
            if (fabs(by) < 1e-5)
            {
                continue;
            }
            roots[root_count++] = -cy / by;
        }
        else
        {
            float discriminant = by * by - 4.0 * ay * cy;
            if (discriminant < 0.0)
            {
                continue;
            }

            float sqrt_disc = sqrt(discriminant);
            roots[root_count++] = (-by + sqrt_disc) / (2.0 * ay);
            roots[root_count++] = (-by - sqrt_disc) / (2.0 * ay);
        }

        for (uint root_index = 0; root_index < root_count; ++root_index)
        {
            float t = roots[root_index];
            if (t < -1e-5 || t >= 1.0 - 1e-5)
            {
                continue;
            }

            t = clamp(t, 0.0, 1.0);

            float2 q = eval_quadratic(p0, p1, p2, t);
            if (p.x >= q.x)
            {
                continue;
            }

            float dydt = 2.0 * ((1.0 - t) * (p1.y - p0.y) + t * (p2.y - p1.y));
            if (fabs(dydt) < 1e-5)
            {
                continue;
            }

            winding += dydt > 0.0 ? 1 : -1;
        }
    }
    return winding != 0;
}

fragment float4 fragment_fill_main(FillVSOut in [[stage_in]],
                                   constant FeatherUniforms& uniforms [[buffer(0)]],
                                   texture2d<float, access::read> shape_texture [[texture(0)]],
                                   texture2d<float, access::read> coverage_texture [[texture(1)]])
{
    (void)shape_texture;
    float2 p = in.uv * uniforms.resolution;
    uint2 pixel = uint2(clamp((uint)p.x, 0u, coverage_texture.get_width()  - 1),
                        clamp((uint)p.y, 0u, coverage_texture.get_height() - 1));
    float coverage = coverage_texture.read(pixel).x;
    float alpha = clamp(coverage, 0.0, 1.0);
    if (alpha <= 0.0)
    {
        return uniforms.background_color;
    }

    return mix(uniforms.background_color, uniforms.shape_color, alpha);
}

fragment float fragment_patch_main(PatchVSOut in [[stage_in]],
                                   constant FeatherUniforms& uniforms [[buffer(0)]],
                                   texture2d<float, access::sample> feather_texture [[texture(1)]])
{
    (void)uniforms;
    return eval_feathered_fill(in.coverages, feather_texture);
}
