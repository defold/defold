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
using Unity.Entities;
using Unity.Mathematics;

namespace Defold.Data.Benchmarks
{
    public struct Position : IComponentData
    {
        public float3 Value;
    }
    public struct Velocity : IComponentData
    {
        public float3 Value;
    }
    public struct Health : IComponentData
    {
        public double Value;
    }
    public struct LightParameters
    {
        public float3 Color;
        public double Intensity;
    }
    public struct Light : IComponentData
    {
        public LightParameters Value;
    }
    public struct Range : IComponentData
    {
        public double Value;
    }
    public struct InnerAngle : IComponentData
    {
        public double Value;
    }
    public struct OuterAngle : IComponentData
    {
        public double Value;
    }
    public struct Amount : IComponentData
    {
        public double Value;
    }
    public struct Owner : IComponentData
    {
        public ulong Value;
    }
    public struct ComponentIdentity : IComponentData
    {
        public ulong Value;
    }
    public struct SpotLightTag : IComponentData
    {
    }
    public struct PointLightTag : IComponentData
    {
    }
    public struct PlayerTag : IComponentData
    {
    }
    public struct EnemyTag : IComponentData
    {
    }
    public struct PickupTag : IComponentData
    {
    }
    public struct BreakableTag : IComponentData
    {
    }
    public struct LightTag : IComponentData
    {
    }
    public struct ActorTag : IComponentData
    {
    }
    public struct DamageableTag : IComponentData
    {
    }

    public struct InputRow
    {
        public float3 Position, Velocity, Color;
        public double Health, Intensity;
        public ulong Owner, Component;
        public int Type;
    }

    public struct Stats
    {
        public double Sum;
        public int Rows, Hits, Batches;
        public void Add(Stats other)
        {
            Sum += other.Sum;
            Rows += other.Rows;
            Hits += other.Hits;
            Batches += other.Batches;
        }
    }

    public static class Values
    {
        public static uint Random(ref uint state) => state = unchecked(state * 1664525u + 1013904223u);
        public static double Sum(float3 value) => (double)value.x + value.y + value.z;
        public static float Distance(float3 value) => value.x * value.x + value.y * value.y + value.z * value.z;
        public static bool Hit(float3 value) => Distance(value) <= 2500.0f;
        public static double Contribution(InputRow row) => Sum(row.Color) * row.Intensity *
        (1.0 - Distance(row.Position) / 2500.0);
        public static bool HasHealth(int type) => type == 2 || type == 3 || type == 5;
        public static void Check(bool condition, string message)
        {
            if (!condition)
                throw new InvalidOperationException(message);
        }
        public static void CheckStats(Stats actual, Stats expected, string operation)
        {
            Check(actual.Rows == expected.Rows && actual.Hits == expected.Hits &&
                  Math.Abs(actual.Sum - expected.Sum) <= 1e-7 * (1 + Math.Abs(expected.Sum)),
                  operation + " count/hit/checksum mismatch");
        }
    }

    public sealed class Fixture : IDisposable
    {
        public static readonly int[] Percent = { 10, 15, 1, 24, 25, 25 };
        // Defold type hashes are computed from the fixture names during setup.
        public readonly int Count, Total;
        public readonly int[] Counts = new int[6], Extras = new int[6], Offsets = new int[6];
        public NativeArray<InputRow> Rows;
        public NativeArray<int> Order;

        public Fixture(int count, ulong[] typeHashes)
        {
            Count = count;
            for (int t = 0; t < 6; ++t)
            {
                Counts[t] = count / 100 * Percent[t];
                Extras[t] = Counts[t] / 10;
                Offsets[t] = Total;
                Total += Counts[t] + Extras[t];
            }
            Rows = new NativeArray<InputRow>(Total, Allocator.Persistent);
            Order = new NativeArray<int>(count, Allocator.Persistent);
            uint random = 0x12345678;
            int order = 0;
            for (int t = 0; t < 6; ++t)
                for (int r = 0; r < Counts[t] + Extras[t]; ++r)
                {
                    int index = Offsets[t] + r;
                    float3 position = new float3((int)(Values.Random(ref random) >> 25) - 64,
                                                 (int)(Values.Random(ref random) >> 25) - 64,
                                                 (int)(Values.Random(ref random) >> 25) - 64);
                    float3 color = new float3((Values.Random(ref random) >> 24) / 256.0f,
                                              (Values.Random(ref random) >> 24) / 256.0f,
                                              (Values.Random(ref random) >> 24) / 256.0f);
                    double health = 100 + (Values.Random(ref random) >> 16) % 101;
                    Rows[index] =
                    new InputRow { Position = position,       Color = color, Velocity = new float3(1, 0, -1),
                                   Health = health,           Intensity = 1, Owner = (ulong)index + 1,
                                   Component = typeHashes[t], Type = t };
                    if (r < Counts[t])
                        Order[order++] = index;
                }
            for (int i = count - 1; i > 0; --i)
            {
                int j = (int)(Values.Random(ref random) % (uint)(i + 1));
                int swap = Order[i];
                Order[i] = Order[j];
                Order[j] = swap;
            }
        }
        public void Dispose()
        {
            Rows.Dispose();
            Order.Dispose();
        }
    }
}
