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
pub fn nearby_lights_bevy(
    query: Query<(&Position, &Light), (With<LightTag>, Without<Velocity>)>,
    trace: Res<Trace>,
    mut locals: Local<Parallel<Stats>>,
) {
    trace.begin(3);
    for s in locals.iter_mut() {
        *s = Stats::default();
    }
    query
        .par_iter()
        .batching_strategy(BatchingStrategy::fixed(4096))
        .for_each_init(
            || {
                #[cfg(data_tsan)]
                check_worker();
                locals.borrow_local_mut()
            },
            |s, (position, light)| {
                if in_radius(position) {
                    s.sum += contribution(position, light);
                    s.hits += 1;
                }
                s.rows += 1;
            },
        );
    let mut total = Stats::default();
    for s in locals.iter_mut() {
        total.add(s);
    }
    trace.finish(3, total);
}
pub fn reference(row: &mut Row, stats: &mut Stats) {
    if row.kind > 1 {
        return;
    }
    if in_radius(&row.position) {
        stats.sum += contribution(&row.position, &row.light);
        stats.hits += 1;
    }
    stats.rows += 1;
}
