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

#ifndef DEFOLD_AREA_LIGHTING_GLSL
#define DEFOLD_AREA_LIGHTING_GLSL

// Rotate a vector by the normalized orientation stored in an area light.
vec3 area_light_rotate(vec4 orientation, vec3 direction)
{
    return direction + 2.0 * cross(orientation.xyz, cross(orientation.xyz, direction) + orientation.w * direction);
}

// Corner winding points along local +Z; the emitting side faces local -Z.
// Coordinates can be world space or shading space, but must share one space.
void area_light_corners(vec3 center, vec3 half_width, vec3 half_height, out vec3 corners[4])
{
    corners[0] = center - half_width - half_height;
    corners[1] = center + half_width - half_height;
    corners[2] = center + half_width + half_height;
    corners[3] = center - half_width + half_height;
}

// Integral of clamped cosine / PI over a convex quad's solid angle. Corners
// are relative to the shaded point, with winding toward the receiving normal.
// Clipping before integration is necessary when the light crosses the horizon.
float area_light_diffuse_integral(vec3 normal, vec3 corners[4])
{
    vec3 clipped[5];
    int count = 0;
    vec3 previous = corners[3];
    float previous_height = dot(normal, previous);
    for (int i = 0; i < 4; ++i)
    {
        vec3 current = corners[i];
        float current_height = dot(normal, current);
        if ((previous_height > 0.0) != (current_height > 0.0))
        {
            float t = previous_height / (previous_height - current_height);
            clipped[count++] = mix(previous, current, t);
        }
        if (current_height > 0.0)
        {
            clipped[count++] = current;
        }
        previous = current;
        previous_height = current_height;
    }

    if (count < 3)
    {
        return 0.0;
    }

    vec3 integral = vec3(0.0);
    previous = normalize(clipped[count - 1]);
    for (int i = 0; i < 5; ++i)
    {
        if (i >= count)
        {
            break;
        }
        vec3 current = normalize(clipped[i]);
        vec3 edge = cross(previous, current);
        float sine = length(edge);
        float cosine = clamp(dot(previous, current), -1.0, 1.0);
        // atan retains precision for small lights and near-antipodal edges.
        // Normalize first so an almost half-circle edge keeps its finite limit.
        if (sine > 0.0)
        {
            integral += (edge / sine) * atan(sine, cosine);
        }
        previous = current;
    }
    return clamp(dot(normal, integral) * 0.15915494309189535, 0.0, 1.0);
}

#endif
