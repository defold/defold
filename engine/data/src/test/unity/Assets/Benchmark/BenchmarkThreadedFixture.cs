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
using Unity.Collections;
using Unity.Mathematics;

namespace Defold.Data.Benchmarks
{
    public struct Wave
    {
        public int Content, EnemyAdd, EnemyRemove;
    }

    public sealed class ThreadedFixture : IDisposable
    {
        public const int GroupRows = 100;
        public readonly int Initial, Capacity;
        public readonly int[][] Live;
        public readonly int[] LiveCount = new int[2];
        public readonly InputRow[] Reference;
        public readonly bool[] Active;
        public NativeArray<InputRow> Mixed, Enemies;
        public int Issued;

        public ThreadedFixture(int population, int frames)
        {
            Initial = population / GroupRows;
            int capacity = Initial, enemies = 0;
            for (int f = 0; f < frames; ++f)
            {
                Wave wave = Plan(Initial, f, enemies);
                capacity += wave.Content + wave.EnemyAdd;
                enemies += wave.EnemyAdd - wave.EnemyRemove;
            }
            Capacity = capacity;
            Live = new[] { new int[capacity], new int[capacity] };
            Reference = new InputRow[capacity * GroupRows];
            Active = new bool[capacity];
            Mixed = Defaults(false);
            Enemies = Defaults(true);
        }

        public static Wave Plan(int initial, int frame, int enemies)
        {
            uint random = 0x91e10da5u ^ (uint)frame;
            int limit = initial / 10;
            Wave wave = new Wave { Content = 1 + (int)((Values.Random(ref random) >> 16) % (uint)(initial / 100 + 1)) };
            switch (frame % 8)
            {
                case 0:
                    wave.EnemyAdd = limit + (int)((Values.Random(ref random) >> 16) % (uint)limit);
                    break;
                case 1:
                case 3:
                    wave.EnemyAdd = (int)((Values.Random(ref random) >> 16) % (uint)(limit / 2 + 1));
                    wave.EnemyRemove = (int)((Values.Random(ref random) >> 16) % (uint)(limit / 2 + 1));
                    break;
                case 2:
                    wave.EnemyRemove = limit / 2 + (int)((Values.Random(ref random) >> 16) % (uint)limit);
                    break;
                case 4:
                case 7:
                    wave.EnemyRemove = enemies;
                    break;
                case 6:
                    wave.EnemyAdd = 1 + (int)((Values.Random(ref random) >> 16) % (uint)limit);
                    break;
            }
            wave.EnemyRemove = Math.Min(wave.EnemyRemove, enemies);
            return wave;
        }

        static NativeArray<InputRow> Defaults(bool enemies)
        {
            var rows = new NativeArray<InputRow>(GroupRows, Allocator.Persistent);
            uint random = enemies ? 0x9abcU : 0x1234U;
            int index = 0;
            for (int t = 0; t < 6; ++t)
                for (int r = 0; r < TypeRows(t, enemies); ++r)
                {
                    InputRow row = new InputRow { Type = t, Velocity = new float3(1, 0, -1), Intensity = 1 };
                    for (int a = 0; a < 3; ++a)
                    {
                        row.Position[a] = (int)(Values.Random(ref random) >> 25) - 64;
                        row.Color[a] = (Values.Random(ref random) >> 24) / 256.0f;
                    }
                    row.Health = r % 4 == 0 ? 100 : r % 4 == 1 ? 99.9 : (Values.Random(ref random) >> 16) % 101;
                    rows[index++] = row;
                }
            return rows;
        }

        static int TypeRows(int type, bool enemies) => enemies ? (type == 3 ? GroupRows : 0) : Fixture.Percent[type];

        public void Add(Store store, NativeArray<InputRow> source, bool enemies)
        {
            int group = Issued++, first = 0;
            for (int t = 0; t < 6; ++t)
            {
                int count = TypeRows(t, enemies);
                if (count == 0)
                    continue;
                store.Add(t, source, first, count, group * GroupRows + first);
                first += count;
            }
            int kind = enemies ? 1 : 0;
            Live[kind][LiveCount[kind]++] = group;
        }

        // Reference bookkeeping is performed after frame timing ends.
        public void ReferenceAdd(int first, int firstEnemy)
        {
            for (int group = first; group < Issued; ++group)
            {
                Active[group] = true;
                var source = group < firstEnemy ? Mixed : Enemies;
                for (int r = 0; r < GroupRows; ++r)
                    Reference[group * GroupRows + r] = source[r];
            }
        }

        public Stats Replay(int task, int groups)
        {
            Stats stats = default;
            for (int g = 0; g < groups; ++g)
                if (Active[g])
                    for (int r = 0; r < GroupRows; ++r)
                    {
                        ref InputRow row = ref Reference[g * GroupRows + r];
                        if (task == 0 && (row.Type == 2 || row.Type == 3))
                        {
                            row.Position += row.Velocity * 0.015625f;
                            stats.Sum += row.Position.x;
                            ++stats.Rows;
                        }
                        if (task == 1 && Values.HasHealth(row.Type))
                        {
                            if (Values.Hit(row.Position))
                            {
                                row.Health = Math.Max(0, row.Health - 25);
                                ++stats.Hits;
                            }
                            stats.Sum += row.Health;
                            ++stats.Rows;
                        }
                        if (task == 2 && Values.HasHealth(row.Type))
                        {
                            row.Health = row.Health < 99.75 ? row.Health + 0.25 : 100;
                            stats.Sum += row.Health;
                            ++stats.Rows;
                        }
                        if (task == 3 && row.Type < 2)
                        {
                            if (Values.Hit(row.Position))
                            {
                                stats.Sum += Values.Contribution(row);
                                ++stats.Hits;
                            }
                            ++stats.Rows;
                        }
                    }
            return stats;
        }

        public void Validate(Store store)
        {
            int live = 0;
            for (int g = 0; g < Issued; ++g)
                for (int r = 0; r < GroupRows; ++r)
                {
                    int index = g * GroupRows + r;
                    var entity = store.Ids[index];
                    Values.Check(store.Manager.Exists(entity) == Active[g], "threaded live/stale identity after reuse");
                    if (!Active[g])
                        continue;
                    ++live;
                    var row = Reference[index];
                    Values.Check(math.all(store.Manager.GetComponentData<Position>(entity).Value == row.Position),
                                 "threaded position");
                    if (Values.HasHealth(row.Type))
                        Values.Check(Math.Abs(store.Manager.GetComponentData<Health>(entity).Value - row.Health) < 1e-9,
                                     "threaded health");
                    if (row.Type < 2)
                    {
                        var light = store.Manager.GetComponentData<Light>(entity).Value;
                        Values.Check(math.all(light.Color == row.Color) && light.Intensity == row.Intensity,
                                     "threaded nested light");
                    }
                }
            Values.Check(live == (LiveCount[0] + LiveCount[1]) * GroupRows, "threaded live count");
        }
        public void Dispose()
        {
            Mixed.Dispose();
            Enemies.Dispose();
        }
    }
}
