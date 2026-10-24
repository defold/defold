#ifdef SLUG_ANIMATION
// Includes are ordered before their users by the shader preprocessor.
vec4 paintData(int index);

// Immutable SVG tracks. svgTime is supplied by the sprite or GUI node.
float svgTime;
int   svgLayer;
int   svgPaint;

int   svgPointer(int start, int index)
{
    return int(paintData(start + index / 4)[index & 3] + 0.5);
}

mat3 svgMatrix(int start)
{
    vec3 x = paintData(start).xyz;
    vec3 y = paintData(start + 1).xyz;
    return mat3(vec3(x.x, y.x, 0.0), vec3(x.y, y.y, 0.0), vec3(x.z, y.z, 1.0));
}

float svgDeterminant(mat3 m)
{
    return m[0].x * m[1].y - m[1].x * m[0].y;
}

mat3 svgInverse(mat3 m)
{
    float d = svgDeterminant(m);
    mat3  result = mat3(vec3(m[1].y / d, -m[0].y / d, 0), vec3(-m[1].x / d, m[0].x / d, 0), vec3(0, 0, 1));
    result[2].xy = -mat2(result) * m[2].xy;
    return result;
}

float svgEase(float x, vec4 curve)
{
    if (x <= 0.0 || x >= 1.0)
        return x;
    float lo = 0.0, hi = 1.0;
    for (int i = 0; i < 16; ++i)
    {
        float t = (lo + hi) * 0.5, u = 1.0 - t;
        float value = 3.0 * u * u * t * curve.x + 3.0 * u * t * t * curve.z + t * t * t;
        if (value < x)
            lo = t;
        else
            hi = t;
    }
    float t = (lo + hi) * 0.5, u = 1.0 - t;
    return 3.0 * u * u * t * curve.y + 3.0 * u * t * t * curve.w + t * t * t;
}

bool svgTrack(int track, out vec4 value)
{
    vec4  header = paintData(track);
    vec4  timing = paintData(track + 1);
    float elapsed = svgTime - timing.x;
    if (elapsed < 0.0)
        return false;
    bool ended = timing.z >= 0.0 && elapsed >= timing.y * timing.z;
    if (ended && timing.w == 0.0)
        return false;
    float fraction = ended ? 1.0 : mod(elapsed, timing.y) / timing.y;
    int   keys = int(paintData(track + 2).x);
    int   count = int(header.x), lo = 0;
    for (int i = 1; i < count; ++i)
    {
        if (fraction < paintData(keys + i * 3 + 1).x)
            break;
        lo = i;
    }
    value = paintData(keys + lo * 3);
    if (header.y == 1.0 || lo == count - 1)
        return true;
    vec4  key = paintData(keys + lo * 3 + 1);
    float t = (fraction - key.x) / (paintData(keys + (lo + 1) * 3 + 1).x - key.x);
    if (header.y == 2.0)
        t = svgEase(t, vec4(key.yzw, paintData(keys + lo * 3 + 2).x));
    value = mix(value, paintData(keys + (lo + 1) * 3), t);
    return true;
}

vec4 svgValue(int binding)
{
    vec4 value = paintData(binding);
    vec4 header = paintData(binding + 1);
    for (int i = 0; i < int(header.x); ++i)
    {
        vec4 animated;
        if (svgTrack(svgPointer(int(header.y), i), animated))
            value = animated;
    }
    return value;
}

mat3 svgTransform(int record)
{
    mat3 result = svgMatrix(record);
    vec4 header = paintData(record + 2);
    for (int i = 0; i < int(header.x); ++i)
    {
        int  track = svgPointer(int(header.y), i);
        vec4 value;
        if (!svgTrack(track, value))
            continue;
        vec4 info = paintData(track);
        mat3 transform = mat3(1.0);
        if (info.z == 1.0)
            transform[2].xy = value.xy;
        else if (info.z == 2.0)
        {
            transform[0].x = value.x;
            transform[1].y = value.y;
        }
        else if (info.z == 3.0)
        {
            float a = radians(value.x), c = cos(a), s = sin(a);
            transform = mat3(vec3(c, s, 0), vec3(-s, c, 0), vec3(0, 0, 1));
            transform[2].xy = value.yz - mat2(transform) * value.yz;
        }
        else if (info.z == 4.0)
            transform[1].x = tan(radians(value.x));
        else if (info.z == 5.0)
            transform[0].y = tan(radians(value.x));
        result = info.w == 1.0 ? result * transform : transform;
    }
    return result;
}

bool svgLayerPosition(int extension, inout vec2 p, inout vec2 dx, inout vec2 dy)
{
    vec4 header = paintData(extension), properties = paintData(extension + 1);
    if (svgValue(int(properties.y)).x <= 0.0)
        return false;
    for (int i = 0; i < int(properties.w); ++i)
        if (svgValue(svgPointer(int(properties.z), i)).x <= 0.0)
            return false;
    mat3 viewport = svgMatrix(extension + 3);
    mat3 transform = viewport;
    for (int i = 0; i < int(header.y); ++i)
        transform = transform * svgTransform(svgPointer(int(header.x), i));
    if (svgDeterminant(transform) == 0.0)
        return false;
    mat3 mapping = svgMatrix(extension + 7) * svgInverse(transform);
    p = (mapping * vec3(p, 1.0)).xy;
    dx = mat2(mapping) * dx;
    dy = mat2(mapping) * dy;
    return true;
}

vec2 svgPaintPosition(vec2 p)
{
    int transform = int(paintData(svgLayer + 2).x);
    if (transform == 0)
        return p;
    mat3 current = svgTransform(transform);
    if (svgDeterminant(current) == 0.0)
        return vec2(1.0e20);
    return (svgInverse(current) * svgMatrix(svgLayer + 5) * vec3(p, 1.0)).xy;
}

vec4 svgStop(vec4 color, int index)
{
    vec4 header = paintData(svgLayer + 2);
    int  stop = (index - svgPaint - 3) / 2;
    if (stop >= int(header.z))
        return color;
    vec4 data = paintData(int(header.y) + stop);
    color = svgValue(int(data.x));
    color.a *= clamp(svgValue(int(data.y)).x, 0.0, 1.0);
    return color;
}

float svgOffset(int index)
{
    if (svgLayer == 0)
        return paintData(index + 1).x;
    vec4 header = paintData(svgLayer + 2);
    int  stop = (index - svgPaint - 3) / 2;
    if (stop >= int(header.z))
        return paintData(index + 1).x;
    return clamp(svgValue(int(paintData(int(header.y) + stop).z)).x, 0.0, 1.0);
}

void svgGradient(int type, inout vec4 p0, inout vec4 p1)
{
    int coordinates = int(paintData(svgLayer + 2).w);
    if (coordinates == 0)
        return;
    vec4 pointers = paintData(coordinates);
    if (type == 1)
    {
        vec2 start = vec2(svgValue(int(pointers.x)).x, svgValue(int(pointers.y)).x);
        vec2 end = vec2(svgValue(int(pointers.z)).x, svgValue(int(pointers.w)).x);
        vec2 direction = end - start;
        p0 = vec4(direction / max(dot(direction, direction), 1.0e-20), start);
    }
    else if (type == 2)
    {
        vec4  remaining = paintData(coordinates + 1);
        vec2  center = vec2(svgValue(int(pointers.w)).x, svgValue(int(remaining.x)).x);
        vec2  focus = vec2(svgValue(int(pointers.x)).x, svgValue(int(pointers.y)).x);
        float radius = max(0.0, svgValue(int(remaining.y)).x);
        float distance = length(focus - center);
        if (distance > radius)
            focus = center + (focus - center) * radius / distance;
        p0 = vec4(focus, 0, 0);
        p1 = vec4(center, radius, 0);
    }
}

#endif
