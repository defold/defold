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

use crate::{
    Measurement, Run,
    backend::{Backend, Stats},
    fixture::*,
};
use bevy_ecs::prelude::*;

type LightsQuery = QueryState<(&'static Position, &'static Light)>;
pub(super) fn query(world: &mut World) -> LightsQuery {
    world.query()
}
fn contribution(position: Vector3, light: &Light) -> f64 {
    let distance =
        position[0] * position[0] + position[1] * position[1] + position[2] * position[2];
    sum_vector(light.color) * light.intensity * (1.0 - distance as f64 / 2500.0)
}
fn nearby_lights_bevy(world: &World, query: &mut LightsQuery) -> Stats {
    let mut stats = Stats::default();
    for (position, light) in query.iter(world) {
        if hit(position.0, 50) {
            stats.sum += contribution(position.0, light);
            stats.hits += 1;
        }
        stats.rows += 1;
    }
    stats
}
pub(super) fn measure(run: &Run, store: &mut Backend, query: &mut LightsQuery) {
    let start = Measurement::begin();
    let stats = nearby_lights_bevy(&store.world, query);
    let elapsed = start.end();
    let mut expected = Stats::default();
    for kind in 0..2 {
        for value in &run.input.values[kind][..run.input.counts[kind]] {
            if hit(value.position, 50) {
                expected.sum += contribution(value.position, &value.light);
                expected.hits += 1;
            }
            expected.rows += 1;
        }
    }
    assert_eq!((stats.rows, stats.hits), (expected.rows, expected.hits));
    assert!((stats.sum - expected.sum).abs() <= 1e-7 * (1.0 + expected.sum.abs()));
    run.record("nearby_lights", elapsed, stats.rows, stats);
}
