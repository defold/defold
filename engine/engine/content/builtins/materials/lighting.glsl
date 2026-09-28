// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

#ifndef DEFOLD_LIGHTING_GLSL
#define DEFOLD_LIGHTING_GLSL

#define LIGHT_DIRECTIONAL 0
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_AREA        4

#include "/builtins/materials/area_lighting.glsl"

#ifdef EDITOR
uniform Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
} lights[MAX_LIGHT_COUNT];

uniform LightBuffer
{
    vec4 light_info;
};
#else
struct Light
{
    vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
};

uniform LightBuffer
{
    // xyz: ambient light, w: number of active lights
    vec4  light_info;
    Light lights[MAX_LIGHT_COUNT];
};
#endif

vec3 world_to_view_point(vec3 p)
{
    return (var_view * vec4(p, 1.0)).xyz;
}

vec3 world_to_view_dir(vec3 d)
{
    return normalize((var_view * vec4(d, 0.0)).xyz);
}

vec3 ambient_light()
{
     return light_info.xyz;
}

// Area-only layout: position.w + direction_range.xyz store the quaternion;
// params.zw store the full width/height. Intensity is emitted surface brightness.
vec3 diffuse_area(int index, vec3 normal, vec3 view_position)
{
    float range = lights[index].direction_range.w;
    vec2 size = lights[index].params.zw;
    if (range <= 0.0 || size.x <= 0.0 || size.y <= 0.0)
    {
        return vec3(0.0);
    }

    vec4 orientation = vec4(lights[index].direction_range.xyz, lights[index].position.w);
    vec3 right = world_to_view_dir(area_light_rotate(orientation, vec3(1.0, 0.0, 0.0)));
    vec3 up = world_to_view_dir(area_light_rotate(orientation, vec3(0.0, 1.0, 0.0)));
    vec3 to_center = world_to_view_point(lights[index].position.xyz) - view_position;
    // +Z faces away from the emitting side. Coplanar points have zero response.
    if (dot(cross(right, up), to_center) <= 0.0)
    {
        return vec3(0.0);
    }

    vec2 half_size = 0.5 * size;
    vec2 closest = clamp(vec2(dot(-to_center, right), dot(-to_center, up)), -half_size, half_size);
    float distance_to_light = length(to_center + right * closest.x + up * closest.y);
    float attenuation = clamp(1.0 - distance_to_light / range, 0.0, 1.0);
    if (attenuation <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 corners[4];
    area_light_corners(to_center, right * half_size.x, up * half_size.y, corners);
    float diffuse = area_light_diffuse_integral(normal, corners);
    return lights[index].color.rgb * lights[index].params.y * diffuse * attenuation * attenuation;
}

vec3 diffuse_lambert(int index, vec3 normal, vec3 view_position)
{
    int type = int(lights[index].params.x);
    if (type == LIGHT_AREA)
    {
        return diffuse_area(index, normal, view_position);
    }
    else if (type == LIGHT_DIRECTIONAL)
    {
        vec3 L = -world_to_view_dir(lights[index].direction_range.xyz);
        return lights[index].color.rgb * lights[index].params.y * max(dot(normal, L), 0.0);
    }
    else if (type == LIGHT_POINT)
    {
        vec3  to_light = world_to_view_point(lights[index].position.xyz) - view_position;
        float dist     = length(to_light);
        float atten    = clamp(1.0 - (dist / lights[index].direction_range.w), 0.0, 1.0);
        atten *= atten;
        vec3  L        = normalize(to_light);
        return lights[index].color.rgb * lights[index].params.y * max(dot(normal, L), 0.0) * atten;
    }
    else if (type == LIGHT_SPOT)
    {
        vec3  to_light = world_to_view_point(lights[index].position.xyz) - view_position;
        float dist     = length(to_light);
        float atten    = clamp(1.0 - (dist / lights[index].direction_range.w), 0.0, 1.0);
        atten *= atten;
        vec3  L         = normalize(to_light);
        vec3  spot_dir  = world_to_view_dir(lights[index].direction_range.xyz);
        float inner_cos = cos(0.5 * lights[index].params.z - 0.00001);
        float outer_cos = cos(0.5 * lights[index].params.w);
        float spot_i    = smoothstep(outer_cos, inner_cos, dot(-L, spot_dir));
        return lights[index].color.rgb * lights[index].params.y * max(dot(normal, L), 0.0) * atten * spot_i;
    }

    return vec3(0.0);
}

vec3 diffuse_lambert(vec3 view_normal, vec3 view_position)
{
    vec3 total_light = vec3(0.0);
    // GLSL 1.20 (editor previews) only provides floating-point min overloads.
    int light_count = int(min(light_info.w, float(MAX_LIGHT_COUNT)));

    for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
    {
        if (i >= light_count)
        {
            break;
        }
        total_light += diffuse_lambert(i, view_normal, view_position);
    }

    return total_light;
}

#endif
