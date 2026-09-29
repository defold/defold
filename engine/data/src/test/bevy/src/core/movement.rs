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

type MovementQuery = QueryState<(&'static mut Position, &'static Velocity)>;
pub(super) fn query(world: &mut World) -> MovementQuery {
    world.query()
}

fn movement_bevy(world: &mut World, query: &mut MovementQuery) -> Stats {
    let mut stats = Stats::default();
    for (mut position, velocity) in query.iter_mut(world) {
        for axis in 0..3 {
            position.0[axis] += velocity.0[axis] * 0.015625;
        }
        stats.sum += sum_vector(position.0);
        stats.rows += 1;
    }
    stats
}

pub(super) fn measure(run: &Run, store: &mut Backend, query: &mut MovementQuery) {
    let start = Measurement::begin();
    let stats = movement_bevy(&mut store.world, query);
    let elapsed = start.end();
    let mut expected = Stats::default();
    for kind in 2..=3 {
        for r in 0..run.input.counts[kind] {
            let value = run.input.values[kind][r];
            let mut position = value.position;
            for axis in 0..3 {
                position[axis] += [1.0, 0.0, -1.0][axis] * 0.015625;
            }
            expected.sum += sum_vector(position);
            expected.rows += 1;
            assert_eq!(
                store
                    .world
                    .get::<Position>(store.ids[run.input.offsets[kind] + r])
                    .unwrap()
                    .0,
                position
            );
        }
    }
    assert_eq!((stats.rows, stats.sum), (expected.rows, expected.sum));
    run.record("movement", elapsed, stats.rows, stats);
}
