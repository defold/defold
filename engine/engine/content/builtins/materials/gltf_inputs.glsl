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

#ifndef DEFOLD_PBR_INPUTS
#define DEFOLD_PBR_INPUTS

/*
 * Shared fragment inputs for the Defold PBR material.
 *
 * The built-in lighting include declares Defold's LightBuffer layout,
 * light_info, lights[], light type constants, and world/view conversion
 * helpers. The root fragment shader must declare var_view and define
 * MAX_LIGHT_COUNT before this file is included.
 */
#ifndef MAX_LIGHT_COUNT
#define MAX_LIGHT_COUNT 8
#endif

in highp vec4 var_position;
in mediump vec3 var_normal;
in mediump vec2 var_texcoord0;
in mediump vec4 var_color;
in mediump vec3 var_tangent;
in mediump vec3 var_bitangent;
in mediump float var_has_tangent;

out vec4 out_fragColor;

#include "/builtins/materials/lighting.glsl"

#endif
