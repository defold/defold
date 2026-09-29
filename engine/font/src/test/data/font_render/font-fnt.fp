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

#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in highp vec2 var_decoration;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

float decoration_mask()
{
    return var_decoration.y > 0.0 ? 1.0 - step(var_decoration.y, fract(var_decoration.x)) : 1.0;
}

void main()
{
    out_fragColor = texture(texture_sampler, var_texcoord0.xy) * var_face_color * var_face_color.a * decoration_mask();
}
