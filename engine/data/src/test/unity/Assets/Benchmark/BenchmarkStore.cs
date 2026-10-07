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
using System.Collections.Generic;
using Unity.Burst;
using Unity.Collections;
using Unity.Entities;
using Unity.Jobs;

namespace Defold.Data.Benchmarks
{
    [DisableAutoCreation]
    public partial class LookupSystem : SystemBase
    {
        protected override void OnUpdate() {}
    }

    public sealed class Store : IDisposable
    {
        public readonly World World;
        public readonly LookupSystem Lookups;
        public EntityManager Manager => World.EntityManager;
        public readonly EntityArchetype[] Types = new EntityArchetype[6];
        public NativeArray<Entity> Ids;

        readonly bool Threaded;

        public Store(int capacity, bool threaded = false)
        {
            Threaded = threaded;
            World = new World("ECS Benchmark");
            Lookups = World.CreateSystemManaged<LookupSystem>();
            Ids = new NativeArray<Entity>(capacity, Allocator.Persistent);
            Types[0] = Archetype(typeof(Position),
                                 typeof(Light),
                                 typeof(Range),
                                 typeof(InnerAngle),
                                 typeof(OuterAngle),
                                 typeof(SpotLightTag),
                                 typeof(LightTag));
            Types[1] =
            Archetype(typeof(Position), typeof(Light), typeof(Range), typeof(PointLightTag), typeof(LightTag));
            Types[2] = Archetype(typeof(Position),
                                 typeof(Velocity),
                                 typeof(Health),
                                 typeof(PlayerTag),
                                 typeof(ActorTag),
                                 typeof(DamageableTag));
            Types[3] = Archetype(typeof(Position),
                                 typeof(Velocity),
                                 typeof(Health),
                                 typeof(EnemyTag),
                                 typeof(ActorTag),
                                 typeof(DamageableTag));
            Types[4] = Archetype(typeof(Position), typeof(Amount), typeof(PickupTag));
            Types[5] = Archetype(typeof(Position), typeof(Health), typeof(BreakableTag), typeof(DamageableTag));
        }

        EntityArchetype Archetype(params ComponentType[] components)
        {
            var fields = new List<ComponentType>(components);
            if (!Threaded)
            {
                fields.Add(ComponentType.ReadWrite<Owner>());
                fields.Add(ComponentType.ReadWrite<ComponentIdentity>());
            }
            return Manager.CreateArchetype(fields.ToArray());
        }

        // Public bulk entity creation plus a Burst job using public ComponentLookup
        // setters. No internal ECS pointers or layout assumptions are used.
        public void Add(int type, NativeArray<InputRow> source, int first, int count, int destination)
        {
            var ids = Ids.GetSubArray(destination, count);
            Manager.CreateEntity(Types[type], ids);
            var job = new InitializeRows { Entities = ids,
                                           Input = source.GetSubArray(first, count),
                                           Type = type,
                                           Threaded = Threaded,
                                           Positions = Lookups.GetComponentLookup<Position>(),
                                           Velocities = Lookups.GetComponentLookup<Velocity>(),
                                           Healths = Lookups.GetComponentLookup<Health>(),
                                           Lights = Lookups.GetComponentLookup<Light>(),
                                           Ranges = Lookups.GetComponentLookup<Range>(),
                                           Inner = Lookups.GetComponentLookup<InnerAngle>(),
                                           Outer = Lookups.GetComponentLookup<OuterAngle>(),
                                           Amounts = Lookups.GetComponentLookup<Amount>(),
                                           Owners = Lookups.GetComponentLookup<Owner>(),
                                           Components = Lookups.GetComponentLookup<ComponentIdentity>() };
            job.Run();
        }

        public void Restore(Fixture fixture)
        {
            for (int t = 0; t < 6; ++t)
                for (int r = 0; r < fixture.Counts[t]; ++r)
                {
                    int index = fixture.Offsets[t] + r;
                    var row = fixture.Rows[index];
                    var entity = Ids[index];
                    Manager.SetComponentData(entity, new Position { Value = row.Position });
                    if (Values.HasHealth(t))
                        Manager.SetComponentData(entity, new Health { Value = row.Health });
                }
        }

        public void Dispose()
        {
            World.Dispose();
            Ids.Dispose();
        }
    }

    [BurstCompile(CompileSynchronously = true, FloatMode = FloatMode.Strict)]
    struct InitializeRows : IJob
    {
        [ReadOnly]
        public NativeArray<Entity> Entities;
        [ReadOnly]
        public NativeArray<InputRow> Input;
        public int Type;
        public bool Threaded;
        public ComponentLookup<Position> Positions;
        public ComponentLookup<Velocity> Velocities;
        public ComponentLookup<Health> Healths;
        public ComponentLookup<Light> Lights;
        public ComponentLookup<Range> Ranges;
        public ComponentLookup<InnerAngle> Inner;
        public ComponentLookup<OuterAngle> Outer;
        public ComponentLookup<Amount> Amounts;
        public ComponentLookup<Owner> Owners;
        public ComponentLookup<ComponentIdentity> Components;

        public void Execute()
        {
            for (int i = 0; i < Entities.Length; ++i)
            {
                Entity entity = Entities[i];
                InputRow row = Input[i];
                Positions[entity] = new Position { Value = row.Position };
                if (!Threaded)
                {
                    Owners[entity] = new Owner { Value = row.Owner };
                    Components[entity] = new ComponentIdentity { Value = row.Component };
                }
                if (Type < 2)
                {
                    Lights[entity] =
                    new Light { Value = new LightParameters { Color = row.Color, Intensity = row.Intensity } };
                    Ranges[entity] = new Range { Value = 10 };
                    if (Type == 0)
                    {
                        Inner[entity] = new InnerAngle { Value = Threaded ? 10 : 0 };
                        Outer[entity] = new OuterAngle { Value = Threaded ? 10 : 45 };
                    }
                }
                if (Type == 2 || Type == 3)
                    Velocities[entity] = new Velocity { Value = row.Velocity };
                if (Values.HasHealth(Type))
                    Healths[entity] = new Health { Value = row.Health };
                if (Type == 4)
                    Amounts[entity] = new Amount { Value = 10 };
            }
        }
    }
}
