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

use crate::fixture::*;
use bevy_ecs::{bundle::NoBundleEffect, prelude::*, query::QueryFilter};

#[derive(Clone, Copy)]
pub enum Kind {
    Rows,
    Columns,
}

impl Kind {
    pub fn name(self) -> &'static str {
        match self {
            Self::Rows => "bevy_rows",
            Self::Columns => "bevy_columns",
        }
    }
}

pub struct Backend {
    pub kind: Kind,
    pub world: World,
    // The fixture owns this input-index-to-runtime-ID mapping in every backend.
    pub ids: Vec<Entity>,
}

fn spawn<B: Bundle<Effect: NoBundleEffect> + Copy>(
    world: &mut World,
    values: &[B],
    ids: &mut [Entity],
    bulk: bool,
) {
    if bulk {
        for (output, entity) in ids
            .iter_mut()
            .zip(world.spawn_batch(values.iter().copied()))
        {
            *output = entity;
        }
    } else {
        for (output, value) in ids.iter_mut().zip(values) {
            *output = world.spawn(*value).id();
        }
    }
}

impl Backend {
    pub fn new(kind: Kind, ids: Vec<Entity>) -> Self {
        let mut world = World::new();
        // Register component types before population, as the C harness does.
        macro_rules! register { ($($t:ty),*) => { $(world.register_component::<$t>();)* }; }
        register!(
            Owner,
            ComponentId,
            LightTag,
            SpotTag,
            PointTag,
            ActorTag,
            PlayerTag,
            EnemyTag,
            PickupTag,
            BreakableTag,
            DamageableTag
        );
        match kind {
            Kind::Rows => {
                register!(SpotLight, PointLight, Player, Enemy, Pickup, Breakable);
            }
            Kind::Columns => {
                register!(
                    Position, Light, Range, InnerAngle, OuterAngle, Health, Velocity, Amount
                );
            }
        }
        Self { kind, world, ids }
    }

    pub fn insert(&mut self, input: &Fixture, kind: usize, start: usize, count: usize, bulk: bool) {
        let end = start + count;
        let offset = input.offsets[kind] + start;
        let output = &mut self.ids[offset..offset + count];
        macro_rules! insert {
            ($values:expr) => {
                match kind {
                    0 => spawn(&mut self.world, &$values.spot[start..end], output, bulk),
                    1 => spawn(&mut self.world, &$values.point[start..end], output, bulk),
                    2 => spawn(&mut self.world, &$values.player[start..end], output, bulk),
                    3 => spawn(&mut self.world, &$values.enemy[start..end], output, bulk),
                    4 => spawn(&mut self.world, &$values.pickup[start..end], output, bulk),
                    5 => spawn(
                        &mut self.world,
                        &$values.breakable[start..end],
                        output,
                        bulk,
                    ),
                    _ => unreachable!(),
                }
            };
        }
        match self.kind {
            Kind::Rows => insert!(input.rows),
            Kind::Columns => insert!(input.columns),
        }
    }

    pub fn position(&mut self, kind: usize, id: Entity, write: bool) -> Vector3 {
        fn access<T: Row>(world: &mut World, id: Entity, write: bool) -> Vector3 {
            let mut value = world.get_mut::<T>(id).unwrap();
            if write {
                value.position_mut()[0] += 1.0;
            }
            value.position()
        }
        match self.kind {
            Kind::Rows => match kind {
                0 => access::<SpotLight>(&mut self.world, id, write),
                1 => access::<PointLight>(&mut self.world, id, write),
                2 => access::<Player>(&mut self.world, id, write),
                3 => access::<Enemy>(&mut self.world, id, write),
                4 => access::<Pickup>(&mut self.world, id, write),
                5 => access::<Breakable>(&mut self.world, id, write),
                _ => unreachable!(),
            },
            Kind::Columns => {
                let mut value = self.world.get_mut::<Position>(id).unwrap();
                if write {
                    value.0[0] += 1.0;
                }
                value.0
            }
        }
    }

    pub fn scalar(&mut self, kind: usize, id: Entity, write: bool) -> f64 {
        fn access<T: Row>(world: &mut World, id: Entity, write: bool) -> f64 {
            let mut value = world.get_mut::<T>(id).unwrap();
            if write {
                *value.scalar_mut() += 1.0;
            }
            value.scalar()
        }
        match self.kind {
            Kind::Rows => match kind {
                0 => access::<SpotLight>(&mut self.world, id, write),
                1 => access::<PointLight>(&mut self.world, id, write),
                2 => access::<Player>(&mut self.world, id, write),
                3 => access::<Enemy>(&mut self.world, id, write),
                4 => access::<Pickup>(&mut self.world, id, write),
                5 => access::<Breakable>(&mut self.world, id, write),
                _ => unreachable!(),
            },
            Kind::Columns => match kind {
                0 | 1 => {
                    let mut value = self.world.get_mut::<Range>(id).unwrap();
                    if write {
                        value.0 += 1.0;
                    }
                    value.0
                }
                4 => {
                    let mut value = self.world.get_mut::<Amount>(id).unwrap();
                    if write {
                        value.0 += 1.0;
                    }
                    value.0
                }
                _ => {
                    let mut value = self.world.get_mut::<Health>(id).unwrap();
                    if write {
                        value.0 += 1.0;
                    }
                    value.0
                }
            },
        }
    }

    pub fn reset(&mut self, input: &Fixture) {
        fn reset_rows<T: Row>(world: &mut World, ids: &[Entity], values: &[Input]) {
            for (&id, v) in ids.iter().zip(values) {
                *world.get_mut::<T>(id).unwrap() = T::from_input(v);
            }
        }
        for kind in 0..6 {
            let ids = &self.ids[input.offsets[kind]..input.offsets[kind] + input.counts[kind]];
            let values = &input.values[kind];
            match self.kind {
                Kind::Rows => match kind {
                    0 => reset_rows::<SpotLight>(&mut self.world, ids, values),
                    1 => reset_rows::<PointLight>(&mut self.world, ids, values),
                    2 => reset_rows::<Player>(&mut self.world, ids, values),
                    3 => reset_rows::<Enemy>(&mut self.world, ids, values),
                    4 => reset_rows::<Pickup>(&mut self.world, ids, values),
                    5 => reset_rows::<Breakable>(&mut self.world, ids, values),
                    _ => unreachable!(),
                },
                Kind::Columns => {
                    for (&id, v) in ids.iter().zip(values) {
                        self.world.get_mut::<Position>(id).unwrap().0 = v.position;
                        match kind {
                            0 | 1 => self.world.get_mut::<Range>(id).unwrap().0 = 10.0,
                            4 => self.world.get_mut::<Amount>(id).unwrap().0 = 10.0,
                            _ => self.world.get_mut::<Health>(id).unwrap().0 = v.health,
                        }
                    }
                }
            }
        }
    }

    pub fn validate_identity(&self, input: &Fixture, extra: bool) {
        for kind in 0..6 {
            let count = input.counts[kind] + if extra { input.counts[kind] / 10 } else { 0 };
            for r in 0..count {
                let id = self.ids[input.offsets[kind] + r];
                assert_eq!(
                    self.world.get::<Owner>(id).unwrap().0,
                    input.values[kind][r].owner
                );
                assert_eq!(
                    self.world.get::<ComponentId>(id).unwrap().0,
                    input.values[kind][r].component
                );
            }
        }
    }
}

#[derive(Clone, Copy, Default, Debug, PartialEq)]
pub struct Stats {
    pub sum: f64,
    pub rows: u64,
    pub hits: u64,
    pub batches: u64,
}

impl Stats {
    pub fn health(
        &mut self,
        position: Vector3,
        health: f64,
        radius: i32,
        write: bool,
    ) -> (f64, bool) {
        let hit = hit(position, radius);
        let result = if write && hit {
            damaged(health, 1)
        } else {
            health
        };
        self.sum += result + sum_vector(position);
        self.hits += u64::from(hit);
        self.rows += 1;
        (result, hit)
    }
    pub fn color(&mut self, color: Vector3) {
        self.sum += sum_vector(color);
        self.rows += 1;
    }
    pub fn add(&mut self, other: Self) {
        self.sum += other.sum;
        self.rows += other.rows;
        self.hits += other.hits;
        self.batches += other.batches;
    }
}

#[derive(Clone, Copy)]
pub enum Selection {
    Spot,
    Light,
    Health,
    Enemy,
}

impl Selection {
    pub fn matches(self, kind: usize) -> bool {
        match self {
            Self::Spot => kind == 0,
            Self::Light => kind < 2,
            Self::Health => has_health(kind),
            Self::Enemy => kind == 3,
        }
    }
    pub fn color(self) -> bool {
        matches!(self, Self::Spot | Self::Light)
    }
}

type HealthColumns<F = ()> = QueryState<(&'static mut Health, &'static Position), F>;

pub enum Query {
    ColorRows {
        spot: QueryState<&'static SpotLight, With<LightTag>>,
        point: Option<QueryState<&'static PointLight, With<LightTag>>>,
    },
    HealthRows {
        player: Option<QueryState<&'static mut Player>>,
        enemy: QueryState<&'static mut Enemy>,
        breakable: Option<QueryState<&'static mut Breakable>>,
    },
    Light(QueryState<&'static Light, With<LightTag>>),
    Spot(QueryState<&'static Light, With<SpotTag>>),
    Health(HealthColumns),
    Enemy(HealthColumns<With<EnemyTag>>),
}

fn read_rows<T: Row>(
    query: &mut QueryState<&'static mut T>,
    world: &mut World,
    radius: i32,
    write: bool,
    stats: &mut Stats,
) {
    if write {
        for mut value in query.iter_mut(world) {
            let (result, hit) = stats.health(value.position(), value.scalar(), radius, true);
            if hit {
                *value.scalar_mut() = result;
            }
        }
    } else {
        for value in query.iter(world) {
            stats.health(value.position(), value.scalar(), radius, false);
        }
    }
    stats.batches += query.matched_tables().count() as u64;
}

fn read_columns<F: QueryFilter>(
    query: &mut HealthColumns<F>,
    world: &mut World,
    radius: i32,
    write: bool,
    stats: &mut Stats,
) {
    if write {
        for (mut health, position) in query.iter_mut(world) {
            let (result, hit) = stats.health(position.0, health.0, radius, true);
            if hit {
                health.0 = result;
            }
        }
    } else {
        for (health, position) in query.iter(world) {
            stats.health(position.0, health.0, radius, false);
        }
    }
    stats.batches += query.matched_tables().count() as u64;
}

impl Query {
    pub fn new(store: &mut Backend, selection: Selection) -> Self {
        let w = &mut store.world;
        match store.kind {
            Kind::Rows if selection.color() => Self::ColorRows {
                spot: w.query_filtered(),
                point: if matches!(selection, Selection::Light) {
                    Some(w.query_filtered())
                } else {
                    None
                },
            },
            Kind::Rows => Self::HealthRows {
                player: if matches!(selection, Selection::Health) {
                    Some(w.query())
                } else {
                    None
                },
                enemy: w.query(),
                breakable: if matches!(selection, Selection::Health) {
                    Some(w.query())
                } else {
                    None
                },
            },
            Kind::Columns => match selection {
                Selection::Spot => Self::Spot(w.query_filtered()),
                Selection::Light => Self::Light(w.query_filtered()),
                Selection::Health => Self::Health(w.query()),
                Selection::Enemy => Self::Enemy(w.query_filtered()),
            },
        }
    }

    pub fn read(&mut self, world: &mut World, radius: i32, write: bool) -> Stats {
        let mut stats = Stats::default();
        match self {
            Self::ColorRows { spot, point } => {
                for value in spot.iter(world) {
                    stats.color(value.light.color);
                }
                stats.batches += spot.matched_tables().count() as u64;
                if let Some(point) = point {
                    for value in point.iter(world) {
                        stats.color(value.light.color);
                    }
                    stats.batches += point.matched_tables().count() as u64;
                }
            }
            Self::HealthRows {
                player,
                enemy,
                breakable,
            } => {
                if let Some(q) = player {
                    read_rows(q, world, radius, write, &mut stats);
                }
                read_rows(enemy, world, radius, write, &mut stats);
                if let Some(q) = breakable {
                    read_rows(q, world, radius, write, &mut stats);
                }
            }
            Self::Light(q) => {
                for light in q.iter(world) {
                    stats.color(light.color);
                }
                stats.batches += q.matched_tables().count() as u64;
            }
            Self::Spot(q) => {
                for light in q.iter(world) {
                    stats.color(light.color);
                }
                stats.batches += q.matched_tables().count() as u64;
            }
            Self::Health(q) => read_columns(q, world, radius, write, &mut stats),
            Self::Enemy(q) => read_columns(q, world, radius, write, &mut stats),
        }
        stats
    }
}
