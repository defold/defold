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

#ifndef DM_RENDER_FRAME_BENCHMARK_H
#define DM_RENDER_FRAME_BENCHMARK_H

// Explicit opt-in microbenchmark, never part of the default test run. Reports fixed
// allocation costs and capture/copy/consume/retirement phases; it is not qualification
// against the engine's inline regression budget. Returns nonzero on capture/checksum errors.
int RunRenderFrameBenchmark();

#endif // DM_RENDER_FRAME_BENCHMARK_H
