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
using Unity.Jobs;

namespace Defold.Data.Benchmarks
{
    public static class PositionLookup
    {
        public static Stats Run(Store store, Fixture fixture, NativeArray<Stats> output)
        {
            var job = new PositionLookupJob { Positions = store.Lookups.GetComponentLookup<Position>(true),
                                              Entities = store.Ids,
                                              Order = fixture.Order,
                                              Output = output };
            job.Run();
            return output[0];
        }
    }
    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    public struct PositionLookupJob : IJob
    {
        [ReadOnly]
        public ComponentLookup<Position> Positions;
        [ReadOnly]
        public NativeArray<Entity> Entities;
        [ReadOnly]
        public NativeArray<int> Order;
        [WriteOnly]
        public NativeArray<Stats> Output;
        public void Execute()
        {
            Stats stats = default;
            for (int i = 0; i < Order.Length; ++i)
            {
                stats.Sum += Values.Sum(Positions[Entities[Order[i]]].Value);
                ++stats.Rows;
            }
            Output[0] = stats;
        }
    }
}
