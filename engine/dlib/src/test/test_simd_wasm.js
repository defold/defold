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

// Probe before Emscripten instantiates the test module. Its Node/Bun runners
// must skip unsupported SIMD modules before entering any C code.
if (!WebAssembly.validate(new Uint8Array([
    0, 97, 115, 109, 1, 0, 0, 0,
    1, 5, 1, 96, 0, 1, 123,
    3, 2, 1, 0,
    10, 8, 1, 6, 0, 65, 0, 253, 15, 11
]))) {
    console.log('SKIP: Wasm SIMD128 is unsupported by this host');
    process.exit(0);
}
