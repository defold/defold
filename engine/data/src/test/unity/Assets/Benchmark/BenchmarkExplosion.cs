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
using Unity.Burst.Intrinsics;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using Unity.Mathematics;

namespace Defold.Data.Benchmarks
{
    public static class Explosion
    {
        public static QueryOutput Create(Store store, int capacity = 0) => new QueryOutput(
        store.Manager.CreateEntityQuery(ComponentType.ReadOnly<Position>(), ComponentType.ReadWrite<Health>()),
        capacity);
        public static ExplosionJob Job(Store store, QueryOutput output) => new ExplosionJob {
            Positions = store.Manager.GetComponentTypeHandle<Position>(true),
            Healths = store.Manager.GetComponentTypeHandle<Health>(false),
            Output = output.Chunks
        };
        public static Stats Run(Store store, QueryOutput output)
        {
            Job(store, output).Run(output.Query);
            return output.Reduce();
        }
    }
    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    public struct ExplosionJob : IJobChunk
    {
        [ReadOnly]
        public ComponentTypeHandle<Position> Positions;
        public ComponentTypeHandle<Health> Healths;
        [WriteOnly, NativeDisableParallelForRestriction]
        public NativeArray<Stats> Output;
        public void
        Execute(in ArchetypeChunk chunk, int unfilteredChunkIndex, bool useEnabledMask, in v128 chunkEnabledMask)
        {
            var positions = chunk.GetNativeArray(ref Positions);
            var healths = chunk.GetNativeArray(ref Healths);
            Stats stats = new Stats { Batches = 1 };
            var rows = new ChunkEntityEnumerator(useEnabledMask, chunkEnabledMask, chunk.Count);
            while (rows.NextEntityIndex(out int row))
            {
                var position = positions[row].Value;
                double health = healths[row].Value;
                if (Values.Hit(position))
                {
                    health = math.max(0.0, health - 25);
                    healths[row] = new Health { Value = health };
                    ++stats.Hits;
                }
                stats.Sum += health + Values.Sum(position);
                ++stats.Rows;
            }
            Output[unfilteredChunkIndex] = stats;
        }
    }
}
