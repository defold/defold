// Font adapter for the shared vector paint format.
#include "/builtins/materials/slug-paint.glsl"

vec4 FontVectorPaintGlyphColor(int glyph, vec2 p, vec2 dx, vec2 dy, vec4 foreground)
{
    return SlugPaintImageColor(glyph, p, dx, dy, foreground);
}
