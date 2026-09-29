// Copyright 2026 The Defold Foundation
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

using Unity.Burst;
using Unity.Collections;
using Unity.Entities;

namespace Defold.Data.Benchmarks
{
    [BurstCompile]
    public static class DespawnWave
    {
        // Structural changes stay on the main thread. The loop uses the public
        // EntityManager API; Burst direct call avoids managed per-entity overhead.
        [BurstCompile(CompileSynchronously = true)]
        public static void
        Run(ref EntityManager manager, in NativeArray<Entity> entities, in NativeArray<int> order, int count)
        {
            for (int i = 0; i < count; ++i)
                manager.DestroyEntity(entities[order[i]]);
        }
    }
}
