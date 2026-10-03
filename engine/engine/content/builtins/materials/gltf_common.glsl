/*
MIT License

Copyright (c) 2024 Defold

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef DEFOLD_PBR_COMMON
#define DEFOLD_PBR_COMMON

/*
 * Common scalar, color-space, and clamping helpers shared by the PBR shader
 * modules. The material assumes texture color data is sampled as sRGB-like
 * input and converted to linear space before lighting.
 */
const float PBR_PI = 3.1415926535897932384626433832795;
const float PBR_EPSILON = 0.00001;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 saturate(vec3 value)
{
    return clamp(value, vec3(0.0), vec3(1.0));
}

vec4 to_linear(vec4 color)
{
    return vec4(pow(color.rgb, vec3(2.2)), color.a);
}

vec3 to_output(vec3 color)
{
    return pow(saturate(color), vec3(1.0 / 2.2));
}

float clamped_dot(vec3 a, vec3 b)
{
    return saturate(dot(a, b));
}

#endif
