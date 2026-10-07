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
using System.Diagnostics;
using System.Globalization;
using System.IO;
using Unity.Collections;
using Unity.Entities;
using Unity.Mathematics;

namespace Defold.Data.Benchmarks
{
    public static class Core
    {
        public static void Run(string output, int count, int samples)
        {
            using var fixture = new Fixture(count);
            using var writer = new StreamWriter(output);
            writer.WriteLine(
            "# backend=Unity; Burst + IL2CPP; one warmup; seven standalone cases; memory not measured");
            writer.WriteLine(
            "backend,operation,rows,sample,operations,hits,batches,total_ms,ns_per_operation,checksum");
            for (int sample = 0; sample <= samples; ++sample)
            {
                using var store = new Store(fixture.Total);
                using var lookupOutput = new NativeArray<Stats>(1, Allocator.Persistent);
                long start = Stopwatch.GetTimestamp();
                CreatePopulation.Run(store, fixture);
                Record(writer, fixture, sample, "create_population", start, Stopwatch.GetTimestamp(), count, default);
                ValidateDefaults(store, fixture, false);
                using var players = Boids.Create(store, false);
                using var enemies = Boids.Create(store, true);
                using var explosion = Explosion.Create(store);
                using var lights = NearbyLights.Create(store);

                start = Stopwatch.GetTimestamp();
                Stats actual = Boids.Run(store, players);
                actual.Add(Boids.Run(store, enemies));
                long end = Stopwatch.GetTimestamp();
                Values.Check(actual.Rows == fixture.Counts[2] + fixture.Counts[3], "boid count");
                Boids.Validate(store, fixture, 2, players);
                Boids.Validate(store, fixture, 3, enemies);
                Record(writer, fixture, sample, "boids", start, end, actual.Rows, actual);
                store.Restore(fixture);

                Stats expected = default;
                for (int t = 0; t < 6; ++t)
                    if (Values.HasHealth(t))
                        for (int r = 0; r < fixture.Counts[t]; ++r)
                        {
                            var row = fixture.Rows[fixture.Offsets[t] + r];
                            bool hit = Values.Hit(row.Position);
                            expected.Sum +=
                            (hit ? Math.Max(0, row.Health - 25) : row.Health) + Values.Sum(row.Position);
                            ++expected.Rows;
                            if (hit)
                                ++expected.Hits;
                        }
                start = Stopwatch.GetTimestamp();
                actual = Explosion.Run(store, explosion);
                end = Stopwatch.GetTimestamp();
                Values.CheckStats(actual, expected, "explosion");
                for (int t = 0; t < 6; ++t)
                    if (Values.HasHealth(t))
                        for (int r = 0; r < fixture.Counts[t]; ++r)
                        {
                            int index = fixture.Offsets[t] + r;
                            var row = fixture.Rows[index];
                            double health = Values.Hit(row.Position) ? Math.Max(0, row.Health - 25) : row.Health;
                            Values.Check(store.Manager.GetComponentData<Health>(store.Ids[index]).Value == health,
                                         "persisted damage");
                        }
                Record(writer, fixture, sample, "explosion_r50_first", start, end, actual.Rows, actual);
                store.Restore(fixture);

                expected = default;
                for (int t = 0; t < 2; ++t)
                    for (int r = 0; r < fixture.Counts[t]; ++r)
                    {
                        var row = fixture.Rows[fixture.Offsets[t] + r];
                        ++expected.Rows;
                        if (Values.Hit(row.Position))
                        {
                            ++expected.Hits;
                            expected.Sum += Values.Contribution(row);
                        }
                    }
                start = Stopwatch.GetTimestamp();
                actual = NearbyLights.Run(store, lights);
                end = Stopwatch.GetTimestamp();
                Values.CheckStats(actual, expected, "nearby lights");
                Record(writer, fixture, sample, "nearby_lights", start, end, actual.Rows, actual);

                expected = default;
                for (int i = 0; i < fixture.Count; ++i)
                {
                    expected.Sum += Values.Sum(fixture.Rows[fixture.Order[i]].Position);
                    ++expected.Rows;
                }
                start = Stopwatch.GetTimestamp();
                actual = PositionLookup.Run(store, fixture, lookupOutput);
                end = Stopwatch.GetTimestamp();
                Values.CheckStats(actual, expected, "position lookup");
                Record(writer, fixture, sample, "position_lookup", start, end, actual.Rows, actual);

                start = Stopwatch.GetTimestamp();
                SpawnWave.Run(store, fixture);
                end = Stopwatch.GetTimestamp();
                Record(writer, fixture, sample, "spawn_wave", start, end, fixture.Total - fixture.Count, default);
                ValidateDefaults(store, fixture, true);
                ValidateQueries(fixture, players.Query, enemies.Query, explosion.Query, lights.Query, 0);
                var manager = store.Manager;
                var ids = store.Ids;
                var order = fixture.Order;
                start = Stopwatch.GetTimestamp();
                DespawnWave.Run(ref manager, in ids, in order, count / 100);
                end = Stopwatch.GetTimestamp();
                Record(writer, fixture, sample, "despawn_wave", start, end, count / 100, default);
                for (int i = 0; i < count / 100; ++i)
                    Values.Check(!manager.Exists(ids[order[i]]), "stale entity after despawn");
                ValidateQueries(fixture, players.Query, enemies.Query, explosion.Query, lights.Query, count / 100);
                writer.Flush();
                UnityEngine.Debug.Log($"Unity core sample {sample}/{samples} passed");
            }
        }

        static void Record(StreamWriter writer,
                           Fixture fixture,
                           int sample,
                           string name,
                           long start,
                           long end,
                           int operations,
                           Stats stats)
        {
            if (sample == 0)
                return;
            double ms = (end - start) * 1000.0 / Stopwatch.Frequency;
            writer.WriteLine(string.Format(CultureInfo.InvariantCulture,
                                           "unity,{0},{1},{2},{3},{4},{5},{6:F6},{7:F3},{8:F9}",
                                           name,
                                           fixture.Count,
                                           sample,
                                           operations,
                                           stats.Hits,
                                           stats.Batches,
                                           ms,
                                           ms * 1e6 / operations,
                                           stats.Sum));
        }

        static void ValidateDefaults(Store store, Fixture fixture, bool wave)
        {
            for (int t = 0; t < 6; ++t)
                for (int r = 0; r < fixture.Counts[t] + (wave ? fixture.Extras[t] : 0); ++r)
                {
                    int index = fixture.Offsets[t] + r;
                    var row = fixture.Rows[index];
                    var entity = store.Ids[index];
                    Values.Check(store.Manager.Exists(entity), "created entity exists");
                    Values.Check(math.all(store.Manager.GetComponentData<Position>(entity).Value == row.Position),
                                 "created position");
                    Values.Check(store.Manager.GetComponentData<Owner>(entity).Value == row.Owner &&
                                 store.Manager.GetComponentData<ComponentIdentity>(entity).Value == row.Component,
                                 "created identity");
                    if (Values.HasHealth(t))
                        Values.Check(store.Manager.GetComponentData<Health>(entity).Value == row.Health,
                                     "created health");
                    if (t == 2 || t == 3)
                        Values.Check(math.all(store.Manager.GetComponentData<Velocity>(entity).Value == row.Velocity),
                                     "created velocity");
                    if (t < 2)
                    {
                        var light = store.Manager.GetComponentData<Light>(entity).Value;
                        Values.Check(math.all(light.Color == row.Color) && light.Intensity == row.Intensity,
                                     "created nested light");
                        Values.Check(store.Manager.GetComponentData<Range>(entity).Value == 10, "created range");
                    }
                    if (t == 0)
                        Values.Check(store.Manager.GetComponentData<InnerAngle>(entity).Value == 0 &&
                                     store.Manager.GetComponentData<OuterAngle>(entity).Value == 45,
                                     "created cone");
                    if (t == 4)
                        Values.Check(store.Manager.GetComponentData<Amount>(entity).Value == 10, "created amount");
                }
        }

        static void
        ValidateQueries(Fixture fixture, EntityQuery players, EntityQuery enemies, EntityQuery explosion, EntityQuery lights, int removed)
        {
            int moving = 0, damageable = 0, lit = 0;
            for (int t = 0; t < 6; ++t)
            {
                int count = fixture.Counts[t] + fixture.Extras[t];
                if (t == 2 || t == 3)
                    moving += count;
                if (Values.HasHealth(t))
                    damageable += count;
                if (t < 2)
                    lit += count;
            }
            for (int i = 0; i < removed; ++i)
            {
                int t = fixture.Rows[fixture.Order[i]].Type;
                if (t == 2 || t == 3)
                    --moving;
                if (Values.HasHealth(t))
                    --damageable;
                if (t < 2)
                    --lit;
            }
            Values.Check(players.CalculateEntityCount() + enemies.CalculateEntityCount() == moving && explosion.CalculateEntityCount() == damageable &&
                         lights.CalculateEntityCount() == lit,
                         "live queries after spawn/despawn");
        }
    }
}
