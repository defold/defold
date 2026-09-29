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

using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using Unity.Burst;
using Unity.Collections;
using Unity.Entities;
using Unity.Jobs;
using Unity.Jobs.LowLevel.Unsafe;

namespace Defold.Data.Benchmarks
{
    [BurstCompile]
    public static class Threaded
    {
        // Unity's component safety handles operate per component type. Conservatively
        // order all Position writers/readers, even when their queries are disjoint.
        // bit 0 = Position, bit 1 = Velocity, bit 2 = Health, bit 3 = Light.
        static readonly int[] Reads = { 3, 5, 4, 9 };
        static readonly int[] Writes = { 1, 4, 4, 0 };
        static bool Conflicts(int a, int b) => (Writes[a] & Reads[b]) != 0 || (Writes[b] & Reads[a]) != 0;
        static double Micros(long ticks) => ticks * 1e6 / Stopwatch.Frequency;

        public static void Run(string output, int population, int frames, int workers)
        {
            Values.Check(JobsUtility.JobWorkerCount == workers,
                         "Unity job-worker-count differs from requested workers");
            using var fixture = new ThreadedFixture(population, frames);
            using var store = new Store(fixture.Capacity * ThreadedFixture.GroupRows, true);
            for (int g = 0; g < fixture.Initial; ++g)
                fixture.Add(store, fixture.Mixed, false);
            fixture.ReferenceAdd(0, fixture.Issued);
            // More than enough chunk outputs for the maximum possible live population:
            // these archetypes hold at least 100 rows/chunk. No per-frame resizing.
            int capacity = fixture.Capacity + 6;
            using var movement = ThreadedMovement.Create(store, capacity);
            using var explosion = ThreadedExplosion.Create(store, capacity);
            using var regenerate = Regenerate.Create(store, capacity);
            using var lights = NearbyLights.Create(store, capacity);
            QueryOutput[] queries = { movement, explosion, regenerate, lights };
            var handles = new JobHandle[4];
            var order = new int[4];
            var stats = new Stats[4];
            var removed = new int[fixture.Capacity];
            using var writer = new StreamWriter(output);
            writer.WriteLine(
            "# sanitizer=none; Unity Jobs/Burst; safety validation uses a separate editor run; memory not " +
            "measured");
            writer.WriteLine(
            "backend,workers,frame,live_rows,frame_us,mutation_us,admission_order,light_sum,content_spawned,content_" +
            "despawned,enemies_spawned,enemies_despawned,live_enemies");
            uint random = 0x5678;
            for (int frame = 0; frame < frames; ++frame)
            {
                Wave wave = ThreadedFixture.Plan(fixture.Initial, frame, fixture.LiveCount[1]);
                for (int u = 0; u < 4; ++u)
                    order[u] = u;
                for (int u = 3; u > 0; --u)
                {
                    int j = (int)((Values.Random(ref random) >> 16) % (uint)(u + 1));
                    int swap = order[u];
                    order[u] = order[j];
                    order[j] = swap;
                }
                long start = Stopwatch.GetTimestamp();
                // Obtain handles before scheduling; each frame follows structural changes.
                var move = ThreadedMovement.Job(store, movement);
                var explode = ThreadedExplosion.Job(store, explosion);
                var regen = Regenerate.Job(store, regenerate);
                var light = NearbyLights.Job(store, lights);
                JobHandle all = default;
                for (int i = 0; i < 4; ++i)
                {
                    int task = order[i];
                    JobHandle dependency = default;
                    for (int j = 0; j < i; ++j)
                        if (Conflicts(task, order[j]))
                            dependency = JobHandle.CombineDependencies(dependency, handles[order[j]]);
                    switch (task)
                    {
                        case 0:
                            handles[task] = move.ScheduleParallel(movement.Query, dependency);
                            break;
                        case 1:
                            handles[task] = explode.ScheduleParallel(explosion.Query, dependency);
                            break;
                        case 2:
                            handles[task] = regen.ScheduleParallel(regenerate.Query, dependency);
                            break;
                        case 3:
                            handles[task] = light.ScheduleParallel(lights.Query, dependency);
                            break;
                    }
                    all = JobHandle.CombineDependencies(all, handles[task]);
                }
                JobHandle.ScheduleBatchedJobs();
                // Main thread prepares incoming content while worker jobs run.
                var pendingMixed = new NativeArray<InputRow>(fixture.Mixed, Allocator.Persistent);
                NativeArray<InputRow> pendingEnemies = default;
                if (wave.EnemyAdd != 0)
                    pendingEnemies = new NativeArray<InputRow>(fixture.Enemies, Allocator.Persistent);
                // Complete can execute jobs on the caller. Wait until finished so the
                // labelled worker count counts all threads doing component row work.
                while (!all.IsCompleted)
                    Thread.Sleep(0);
                all.Complete();
                for (int u = 0; u < 4; ++u)
                    stats[u] = queries[u].Reduce();
                long mutation = Stopwatch.GetTimestamp();
                int removedCount = 0, firstAdded = fixture.Issued;
                var manager = store.Manager;
                for (int kind = 0; kind < 2; ++kind)
                {
                    int count = kind == 0 ? wave.Content : wave.EnemyRemove;
                    for (int i = 0; i < count; ++i)
                    {
                        int slot = (int)((Values.Random(ref random) >> 8) % (uint)fixture.LiveCount[kind]);
                        int group = fixture.Live[kind][slot];
                        removed[removedCount++] = group;
                        var entities =
                        store.Ids.GetSubArray(group * ThreadedFixture.GroupRows, ThreadedFixture.GroupRows);
                        RemoveGroup(ref manager, in entities);
                        fixture.Live[kind][slot] = fixture.Live[kind][--fixture.LiveCount[kind]];
                    }
                }
                for (int g = 0; g < wave.Content; ++g)
                    fixture.Add(store, pendingMixed, false);
                int firstEnemy = fixture.Issued;
                for (int g = 0; g < wave.EnemyAdd; ++g)
                    fixture.Add(store, pendingEnemies, true);
                pendingMixed.Dispose();
                if (pendingEnemies.IsCreated)
                    pendingEnemies.Dispose();
                long end = Stopwatch.GetTimestamp();
                for (int i = 0; i < 4; ++i)
                    Values.CheckStats(
                    stats[order[i]], fixture.Replay(order[i], firstAdded), "threaded task " + order[i]);
                for (int i = 0; i < removedCount; ++i)
                    fixture.Active[removed[i]] = false;
                fixture.ReferenceAdd(firstAdded, firstEnemy);
                fixture.Validate(store);
                string admitted = string.Concat(order[0], order[1], order[2], order[3]);
                writer.WriteLine(
                $"Unity,{workers},{frame},{(fixture.LiveCount[0] + fixture.LiveCount[1]) * 100},{Micros(end - start):F3},{Micros(end - mutation):F3},{admitted},{stats[3].Sum:F9},{wave.Content * 100},{wave.Content * 100},{wave.EnemyAdd * 100},{wave.EnemyRemove * 100},{fixture.LiveCount[0] * 24 + fixture.LiveCount[1] * 100}");
                writer.Flush();
                UnityEngine.Debug.Log($"Unity threaded {workers} workers frame {frame + 1}/{frames} passed");
            }
            for (int kind = 0; kind < 2; ++kind)
                for (int i = 0; i < fixture.LiveCount[kind]; ++i)
                {
                    var manager = store.Manager;
                    var entities = store.Ids.GetSubArray(fixture.Live[kind][i] * 100, 100);
                    RemoveGroup(ref manager, in entities);
                }
            foreach (var query in queries)
                Values.Check(query.Query.CalculateEntityCount() == 0, "threaded teardown");
        }

        [BurstCompile(CompileSynchronously = true)]
        public static void RemoveGroup(ref EntityManager manager, in NativeArray<Entity> entities)
        {
            for (int r = 0; r < entities.Length; ++r)
                manager.DestroyEntity(entities[r]);
        }
    }
}
