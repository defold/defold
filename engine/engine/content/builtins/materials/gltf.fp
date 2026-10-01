#version 140

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

in mediump mat4 var_view;

#define MAX_LIGHT_COUNT 8
#include "/builtins/materials/gltf_lighting.glsl"

void main()
{
    PBRParams params = get_pbr_params();
    MaterialInfo material = get_material_info(params);

    /*
     * Extension point:
     *   PBRLightData pbr_data = calculate_pbr_light_data(...);
     *   pbr_data.specular += calculate_custom_specular(...);
     *   vec3 color = composite_pbr_light_data(pbr_data);
     */
    PBRLightData pbr_data = calculate_pbr_light_data(params, material, var_position.xyz);
    vec3 color = composite_pbr_light_data(pbr_data);
    out_fragColor = vec4(to_output(color), pbr_data.alpha);
    out_fragColor.a = 1.0;
}
