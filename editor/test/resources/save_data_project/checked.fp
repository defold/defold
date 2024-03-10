// Copyright 2020-2023 The Defold Foundation
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

uniform lowp sampler2DArray texture0;

varying mediump vec4 position;
varying mediump vec3 var_texcoord0;
varying mediump vec4 var_color0;

void main() {
	vec4 texture0_color = texture2DArray(texture0, var_texcoord0);
	gl_FragColor = texture0_color * var_color0;
}
