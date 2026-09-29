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

use super::common::*;
use bevy_ecs::{batching::BatchingStrategy, prelude::*};
use bevy_utils::Parallel;

// Query types beside the traversal declare the scheduler's read/write access.
pub fn movement_bevy(
    mut query: Query<(&mut Position, &Velocity), Without<Light>>,
    trace: Res<Trace>,
    mut locals: Local<Parallel<Stats>>,
) {
    trace.begin(0);
    for s in locals.iter_mut() {
        *s = Stats::default();
    }
    query
        .par_iter_mut()
        .batching_strategy(BatchingStrategy::fixed(4096))
        .for_each_init(
            || {
                #[cfg(data_tsan)]
                check_worker();
                locals.borrow_local_mut()
            },
            |s, (mut position, velocity)| {
                for axis in 0..3 {
                    position.0[axis] += velocity.0[axis] * 0.015625;
                }
                s.sum += position.0[0] as f64;
                s.rows += 1;
            },
        );
    let mut total = Stats::default();
    for s in locals.iter_mut() {
        total.add(s);
    }
    trace.finish(0, total);
}
pub fn reference(row: &mut Row, stats: &mut Stats) {
    if row.kind != 2 && row.kind != 3 {
        return;
    }
    for axis in 0..3 {
        row.position.0[axis] += row.velocity.0[axis] * 0.015625;
    }
    stats.sum += row.position.0[0] as f64;
    stats.rows += 1;
}
