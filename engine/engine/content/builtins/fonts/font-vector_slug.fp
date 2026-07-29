#version 140

precision highp float;
precision highp int;

in highp vec2 var_texcoord;
in mediump vec4 var_color;
flat in highp vec4 var_banding;
flat in highp vec2 var_jacobian;
flat in highp vec2 var_glyph;

out vec4 out_fragColor;

uniform highp sampler2D curve_texture;
uniform highp sampler2D band_texture;

const float CURVE_TEXTURE_WIDTH = 512.0;
const float CURVE_TEXTURE_HEIGHT = 64.0;
const float BAND_TEXTURE_WIDTH = 2048.0;
const float BAND_TEXTURE_HEIGHT = 128.0;
const int MAX_VECTOR_BAND_CURVES = 256;

vec4 SampleCurveTexel(float texel_index)
{
    float texel_x = mod(texel_index, CURVE_TEXTURE_WIDTH);
    float texel_y = floor(texel_index / CURVE_TEXTURE_WIDTH);
    return textureLod(curve_texture, vec2((texel_x + 0.5) / CURVE_TEXTURE_WIDTH, (texel_y + 0.5) / CURVE_TEXTURE_HEIGHT), 0.0);
}

vec4 SampleBandTexel(float row, float column)
{
    return textureLod(band_texture, vec2((column + 0.5) / BAND_TEXTURE_WIDTH, (row + 0.5) / BAND_TEXTURE_HEIGHT), 0.0);
}

int CalcRootCode(float y1, float y2, float y3)
{
    int i1 = (y1 < 0.0) ? 1 : 0;
    int i2 = (y2 < 0.0) ? 2 : 0;
    int i3 = (y3 < 0.0) ? 4 : 0;
    int shift = i1 + i2 + i3;
    return (0x2E74 >> shift) & 0x0101;
}

vec2 SolveHorizPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.y;
    float rb = 0.5 / b.y;

    float d = sqrt(max(b.y * b.y - a.y * p12.y, 0.0));
    float t1 = (b.y - d) * ra;
    float t2 = (b.y + d) * ra;

    if (abs(a.y) < 1.0 / 65536.0)
    {
        t1 = t2 = p12.y * rb;
    }

    return vec2((a.x * t1 - b.x * 2.0) * t1 + p12.x, (a.x * t2 - b.x * 2.0) * t2 + p12.x);
}

vec2 SolveVertPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.x;
    float rb = 0.5 / b.x;

    float d = sqrt(max(b.x * b.x - a.x * p12.x, 0.0));
    float t1 = (b.x - d) * ra;
    float t2 = (b.x + d) * ra;

    if (abs(a.x) < 1.0 / 65536.0)
    {
        t1 = t2 = p12.x * rb;
    }

    return vec2((a.y * t1 - b.y * 2.0) * t1 + p12.y, (a.y * t2 - b.y * 2.0) * t2 + p12.y);
}

float CalcCoverage(float xcov, float ycov, float xwgt, float ywgt)
{
    float coverage = max(abs(xcov * xwgt + ycov * ywgt) / max(xwgt + ywgt, 1.0 / 65536.0), min(abs(xcov), abs(ycov)));
    return clamp(coverage, 0.0, 1.0);
}

float SlugRenderFiltered(vec2 render_coord, vec4 band_transform, float band_row, ivec2 band_max, vec2 filter_width)
{
    vec2 filters_per_em = 1.0 / max(filter_width, vec2(1.0 / 65536.0));
    ivec2 band_index = clamp(ivec2(render_coord * band_transform.xy + band_transform.zw), ivec2(0, 0), band_max);

    vec4 hband_raw = SampleBandTexel(band_row, float(band_index.y));
    int hband_count = int(hband_raw.x + 0.5);
    int hband_offset = int(hband_raw.y + 0.5);

    float xcov = 0.0;
    float xwgt = 0.0;

    for (int curve_index = 0; curve_index < MAX_VECTOR_BAND_CURVES; ++curve_index)
    {
        if (curve_index >= hband_count)
        {
            break;
        }

        vec4 loc_raw = SampleBandTexel(band_row, float(hband_offset + curve_index));
        float curve_texel = loc_raw.x;

        vec4 p12 = SampleCurveTexel(curve_texel) - vec4(render_coord, render_coord);
        vec2 p3 = SampleCurveTexel(curve_texel + 1.0).xy - render_coord;

        if (max(max(p12.x, p12.z), p3.x) * filters_per_em.x < -0.5)
        {
            break;
        }

        int code = CalcRootCode(p12.y, p12.w, p3.y);
        if (code != 0)
        {
            vec2 r = SolveHorizPoly(p12, p3) * filters_per_em.x;

            if ((code & 1) != 0)
            {
                xcov += clamp(r.x + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }

            if (code > 1)
            {
                xcov -= clamp(r.y + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    vec4 vband_raw = SampleBandTexel(band_row, float(band_max.y + 1 + band_index.x));
    int vband_count = int(vband_raw.x + 0.5);
    int vband_offset = int(vband_raw.y + 0.5);

    float ycov = 0.0;
    float ywgt = 0.0;

    for (int curve_index = 0; curve_index < MAX_VECTOR_BAND_CURVES; ++curve_index)
    {
        if (curve_index >= vband_count)
        {
            break;
        }

        vec4 loc_raw = SampleBandTexel(band_row, float(vband_offset + curve_index));
        float curve_texel = loc_raw.x;

        vec4 p12 = SampleCurveTexel(curve_texel) - vec4(render_coord, render_coord);
        vec2 p3 = SampleCurveTexel(curve_texel + 1.0).xy - render_coord;

        if (max(max(p12.y, p12.w), p3.y) * filters_per_em.y < -0.5)
        {
            break;
        }

        int code = CalcRootCode(p12.x, p12.z, p3.x);
        if (code != 0)
        {
            vec2 r = SolveVertPoly(p12, p3) * filters_per_em.y;

            if ((code & 1) != 0)
            {
                ycov -= clamp(r.x + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }

            if (code > 1)
            {
                ycov += clamp(r.y + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    return CalcCoverage(xcov, ycov, xwgt, ywgt);
}

void main()
{
    float curve_count = var_glyph.x;
    if (curve_count <= 0.0)
    {
        discard;
    }

    mediump vec2 p = var_texcoord;
    float band_row = var_glyph.y;
    ivec2 band_max = ivec2(int(var_jacobian.x + 0.5), int(var_jacobian.y + 0.5));
    vec2 pixel_filter_width = fwidth(p);
    float coverage = SlugRenderFiltered(p, var_banding, band_row, band_max, pixel_filter_width);
    if (coverage <= 0.0)
    {
        discard;
    }

    float alpha = var_color.a * coverage;
    out_fragColor = vec4(var_color.rgb * alpha, alpha);
}
