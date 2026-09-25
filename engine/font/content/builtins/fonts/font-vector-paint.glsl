// Copyright 2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef SLUG_LEGACY_GL
uniform highp sampler2D paint_texture;
vec4 paintData(int index)
{
    return texelFetch(paint_texture, ivec2(index & 4095, index >> 12), 0);
}

vec4 stopColor(int index, vec4 foreground)
{
    vec4 color = paintData(index);
    if (paintData(index + 1).y > 0.5)
        color.rgb = foreground.rgb;
    return vec4(color.rgb * color.a, color.a);
}

vec4 colorLine(float t, int count, int extend, int paint, vec4 foreground)
{
    int first = paint + 3;
    int last = first + (count - 1) * 2;
    float lo = paintData(first + 1).x;
    float hi = paintData(last + 1).x;
    if (hi > lo && extend != 0)
    {
        float u = (t - lo) / (hi - lo);
        u = extend == 1 ? fract(u) : 1.0 - abs(mod(u, 2.0) - 1.0);
        t = mix(lo, hi, u);
    }
    vec4 previous = stopColor(first, foreground);
    float previousOffset = lo;
    if (t < lo) return previous;
    for (int i = 1; i < count; ++i)
    {
        int index = first + i * 2;
        vec4 color = stopColor(index, foreground);
        float offset = paintData(index + 1).x;
        if (t < offset)
            return mix(previous, color, clamp((t - previousOffset) / (offset - previousOffset), 0.0, 1.0));
        previous = color;
        previousOffset = offset;
    }
    return previous;
}

vec4 FontVectorPaintColor(int paint, vec2 p, vec4 foreground)
{
    vec4 header = paintData(paint);
    vec4 p0 = paintData(paint + 1);
    vec4 p1 = paintData(paint + 2);
    float t = 0.0;
    if (header.x == 1.0)
    {
        // p0 holds the projected linear-gradient direction and origin.
        t = dot(p - p0.zw, p0.xy);
    }
    else if (header.x == 2.0)
    {
        // Solve |p - (c0 + t * dc)| = r0 + t * dr. COLR chooses
        // the greatest real root whose interpolated radius is nonnegative.
        vec2 q = p - p0.xy;
        vec2 dc = p1.xy - p0.xy;
        float dr = p1.z - p0.z;
        float a = dot(dc, dc) - dr * dr;
        float b = -2.0 * (dot(q, dc) + p0.z * dr);
        float c = dot(q, q) - p0.z * p0.z;
        if (abs(a) < 0.00001)
        {
            if (abs(b) < 0.00001) discard;
            t = -c / b;
            if (p0.z + t * dr < 0.0) discard;
        }
        else
        {
            float d = b * b - 4.0 * a * c;
            if (d < 0.0) discard;
            float t0 = (-b - sqrt(d)) / (2.0 * a);
            float t1 = (-b + sqrt(d)) / (2.0 * a);
            float hi = max(t0, t1);
            float lo = min(t0, t1);
            t = p0.z + hi * dr >= 0.0 ? hi : lo;
            if (p0.z + t * dr < 0.0) discard;
        }
    }
    return foreground.a * colorLine(t, int(header.y), int(header.z), paint, foreground);
}
#endif
