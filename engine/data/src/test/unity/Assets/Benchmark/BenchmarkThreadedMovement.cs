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

namespace Defold.Data.Benchmarks
{
    public static class ThreadedMovement
    {
        public static QueryOutput Create(Store store, int capacity = 0) => new QueryOutput(
        store.Manager.CreateEntityQuery(ComponentType.ReadWrite<Position>(), ComponentType.ReadOnly<Velocity>()),
        capacity);
        public static ThreadedMovementJob Job(Store store, QueryOutput output) => new ThreadedMovementJob {
            Positions = store.Manager.GetComponentTypeHandle<Position>(false),
            Velocities = store.Manager.GetComponentTypeHandle<Velocity>(true),
            Output = output.Chunks
        };
    }
    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    public struct ThreadedMovementJob : IJobChunk
    {
        public ComponentTypeHandle<Position> Positions;
        [ReadOnly]
        public ComponentTypeHandle<Velocity> Velocities;
        [WriteOnly, NativeDisableParallelForRestriction]
        public NativeArray<Stats> Output;
        public void
        Execute(in ArchetypeChunk chunk, int unfilteredChunkIndex, bool useEnabledMask, in v128 chunkEnabledMask)
        {
            var positions = chunk.GetNativeArray(ref Positions);
            var velocities = chunk.GetNativeArray(ref Velocities);
            Stats stats = new Stats { Batches = 1 };
            var rows = new ChunkEntityEnumerator(useEnabledMask, chunkEnabledMask, chunk.Count);
            while (rows.NextEntityIndex(out int row))
            {
                var position = positions[row];
                position.Value += velocities[row].Value * 0.015625f;
                positions[row] = position;
                stats.Sum += position.Value.x;
                ++stats.Rows;
            }
            Output[unfilteredChunkIndex] = stats;
        }
    }
}
