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

using System;
using Unity.Burst;
using Unity.Burst.Intrinsics;
using Unity.Collections;
using Unity.Collections.LowLevel.Unsafe;
using Unity.Entities;
using Unity.Jobs;
using Unity.Mathematics;

namespace Defold.Data.Benchmarks
{
    public struct Boid
    {
        public float3 Position, Velocity;
    }
    public struct BoidCell
    {
        public int3 Coordinates;
        public int Count;
        public float3 PositionSum, VelocitySum;
        public int Target;
        public bool Avoid;
    }
    public sealed class BoidsFlock : IDisposable
    {
        public QueryOutput Output;
        public EntityQuery Query => Output.Query;
        public NativeArray<Boid> Snapshot;
        public NativeArray<BoidCell> Cells;
        public NativeArray<int> RowCells, Slots, Cursor;
        public BoidsFlock(Store store, ComponentType tag)
        {
            Output = new QueryOutput(store.Manager.CreateEntityQuery(ComponentType.ReadWrite<Position>(),
                                     ComponentType.ReadWrite<Velocity>(), tag));
            int count = Query.CalculateEntityCount(), slots = 1;
            while (slots < count * 2)
                slots *= 2;
            Snapshot = new NativeArray<Boid>(count, Allocator.Persistent);
            Cells = new NativeArray<BoidCell>(count, Allocator.Persistent);
            RowCells = new NativeArray<int>(count, Allocator.Persistent);
            Slots = new NativeArray<int>(slots, Allocator.Persistent);
            Cursor = new NativeArray<int>(1, Allocator.Persistent);
        }
        public void Dispose()
        {
            Output.Dispose();
            Snapshot.Dispose();
            Cells.Dispose();
            RowCells.Dispose();
            Slots.Dispose();
            Cursor.Dispose();
        }
    }
    public static class Boids
    {
        public static float3 Target(int index) => index == 0 ? new float3(38, 27, 35) : new float3(-42, 29, -32);
        public static float LengthSquared(float3 v) => v.x * v.x + v.y * v.y + v.z * v.z;
        static float3 Normalize(float3 v, float3 fallback)
        {
            float length = LengthSquared(v);
            return length > 1e-12f ? v * (1.0f / math.sqrt(length)) : fallback;
        }
        public static Boid Steer(Boid row, BoidCell cell)
        {
            float3 heading = Normalize(row.Velocity, new float3(0, 0, 1));
            float3 alignment = Normalize(cell.VelocitySum * (1.0f / cell.Count) - row.Velocity, default);
            float3 separation = Normalize(row.Position * cell.Count - cell.PositionSum, default);
            float3 attraction = Normalize(Target(cell.Target) - row.Position, default) * 2.0f;
            float3 desired = Normalize(alignment + separation + attraction, heading);
            if (cell.Avoid)
            {
                float3 away = Normalize(row.Position, heading);
                desired = Normalize(away * 30.0f - row.Position, away);
            }
            heading = Normalize(heading + (desired - heading) * 0.015625f, heading);
            float3 velocity = heading * 25.0f;
            return new Boid { Position = row.Position + velocity * 0.015625f, Velocity = velocity };
        }
        public static double Checksum(Boid row)
        {
            double sum = 0;
            for (int axis = 0; axis < 3; ++axis)
                sum += (double)row.Position[axis] * row.Position[axis] + (double)row.Velocity[axis] * row.Velocity[axis];
            return sum;
        }
        public static BoidsFlock Create(Store store, bool enemies) => new BoidsFlock(store,
            enemies ? ComponentType.ReadOnly<EnemyTag>() : ComponentType.ReadOnly<PlayerTag>());
        public static void BuildCells(BoidsFlock flock)
        {
            new BoidsCellsJob { Snapshot = flock.Snapshot, Cells = flock.Cells, RowCells = flock.RowCells, Slots = flock.Slots }.Run();
        }
        public static Stats Run(Store store, BoidsFlock flock)
        {
            flock.Cursor[0] = 0;
            new BoidsSnapshotJob { Positions = store.Manager.GetComponentTypeHandle<Position>(true),
                Velocities = store.Manager.GetComponentTypeHandle<Velocity>(true), Snapshot = flock.Snapshot, Cursor = flock.Cursor }.Run(flock.Query);
            BuildCells(flock);
            flock.Cursor[0] = 0;
            new BoidsSteeringJob { Positions = store.Manager.GetComponentTypeHandle<Position>(false),
                Velocities = store.Manager.GetComponentTypeHandle<Velocity>(false), Snapshot = flock.Snapshot,
                Cells = flock.Cells, RowCells = flock.RowCells, Cursor = flock.Cursor, Output = flock.Output.Chunks }.Run(flock.Query);
            return flock.Output.Reduce();
        }
        public static void Validate(Store store, Fixture fixture, int type, BoidsFlock flock)
        {
            for (int r = 0; r < fixture.Counts[type]; ++r)
            {
                InputRow row = fixture.Rows[fixture.Offsets[type] + r];
                flock.Snapshot[r] = new Boid { Position = row.Position, Velocity = row.Velocity };
            }
            BuildCells(flock);
            for (int r = 0; r < fixture.Counts[type]; ++r)
            {
                Boid expected = Steer(flock.Snapshot[r], flock.Cells[flock.RowCells[r]]);
                Entity id = store.Ids[fixture.Offsets[type] + r];
                float3 position = store.Manager.GetComponentData<Position>(id).Value;
                float3 velocity = store.Manager.GetComponentData<Velocity>(id).Value;
                Values.Check(math.all(math.isfinite(position)) && math.all(math.isfinite(velocity)), "finite boid");
                Values.Check(math.all(math.abs(position - expected.Position) < 1e-4f) &&
                             math.all(math.abs(velocity - expected.Velocity) < 1e-4f), "persisted boid position/velocity");
                Values.Check(math.abs(LengthSquared(velocity) - 625.0f) < 0.001f, "boid speed");
            }
        }
    }

    // These jobs run serially for the standalone comparison. Snapshot and steering
    // use the same query order, with no structural change between the passes.
    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    public struct BoidsSnapshotJob : IJobChunk
    {
        [ReadOnly] public ComponentTypeHandle<Position> Positions;
        [ReadOnly] public ComponentTypeHandle<Velocity> Velocities;
        [WriteOnly, NativeDisableParallelForRestriction] public NativeArray<Boid> Snapshot;
        public NativeArray<int> Cursor;
        public void Execute(in ArchetypeChunk chunk, int unfilteredChunkIndex, bool useEnabledMask, in v128 chunkEnabledMask)
        {
            var positions = chunk.GetNativeArray(ref Positions);
            var velocities = chunk.GetNativeArray(ref Velocities);
            int index = Cursor[0];
            var rows = new ChunkEntityEnumerator(useEnabledMask, chunkEnabledMask, chunk.Count);
            while (rows.NextEntityIndex(out int row))
                Snapshot[index++] = new Boid { Position = positions[row].Value, Velocity = velocities[row].Value };
            Cursor[0] = index;
        }
    }
    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    public struct BoidsCellsJob : IJob
    {
        [ReadOnly] public NativeArray<Boid> Snapshot;
        public NativeArray<BoidCell> Cells;
        public NativeArray<int> RowCells, Slots;
        public void Execute()
        {
            for (int i = 0; i < Slots.Length; ++i)
                Slots[i] = 0;
            int cellCount = 0, mask = Slots.Length - 1;
            for (int i = 0; i < Snapshot.Length; ++i)
            {
                Boid row = Snapshot[i];
                int3 coordinates = (int3)math.floor(row.Position * 0.125f);
                int slot = (int)(unchecked((uint)coordinates.x * 73856093u) ^
                                 unchecked((uint)coordinates.y * 19349663u) ^
                                 unchecked((uint)coordinates.z * 83492791u)) & mask;
                while (Slots[slot] != 0)
                {
                    if (math.all(Cells[Slots[slot] - 1].Coordinates == coordinates))
                        break;
                    slot = (slot + 1) & mask;
                }
                if (Slots[slot] == 0)
                {
                    Cells[cellCount++] = new BoidCell { Coordinates = coordinates };
                    Slots[slot] = cellCount;
                }
                int index = Slots[slot] - 1;
                BoidCell cell = Cells[index];
                ++cell.Count;
                cell.PositionSum += row.Position;
                cell.VelocitySum += row.Velocity;
                Cells[index] = cell;
                RowCells[i] = index;
            }
            for (int i = 0; i < cellCount; ++i)
            {
                BoidCell cell = Cells[i];
                float3 center = cell.PositionSum * (1.0f / cell.Count);
                cell.Target = Boids.LengthSquared(Boids.Target(0) - center) <= Boids.LengthSquared(Boids.Target(1) - center) ? 0 : 1;
                cell.Avoid = Boids.LengthSquared(center) < 900.0f;
                Cells[i] = cell;
            }
        }
    }
    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    public struct BoidsSteeringJob : IJobChunk
    {
        public ComponentTypeHandle<Position> Positions;
        public ComponentTypeHandle<Velocity> Velocities;
        [ReadOnly] public NativeArray<Boid> Snapshot;
        [ReadOnly] public NativeArray<BoidCell> Cells;
        [ReadOnly] public NativeArray<int> RowCells;
        public NativeArray<int> Cursor;
        [WriteOnly, NativeDisableParallelForRestriction] public NativeArray<Stats> Output;
        public void Execute(in ArchetypeChunk chunk, int unfilteredChunkIndex, bool useEnabledMask, in v128 chunkEnabledMask)
        {
            var positions = chunk.GetNativeArray(ref Positions);
            var velocities = chunk.GetNativeArray(ref Velocities);
            Stats stats = new Stats { Batches = 1 };
            int index = Cursor[0];
            var rows = new ChunkEntityEnumerator(useEnabledMask, chunkEnabledMask, chunk.Count);
            while (rows.NextEntityIndex(out int row))
            {
                BoidCell cell = Cells[RowCells[index]];
                Boid result = Boids.Steer(Snapshot[index++], cell);
                positions[row] = new Position { Value = result.Position };
                velocities[row] = new Velocity { Value = result.Velocity };
                stats.Sum += Boids.Checksum(result);
                if (cell.Avoid)
                    ++stats.Hits;
                ++stats.Rows;
            }
            Cursor[0] = index;
            Output[unfilteredChunkIndex] = stats;
        }
    }
}
