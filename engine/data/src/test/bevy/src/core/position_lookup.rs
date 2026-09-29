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

fn position_lookup_bevy(store: &Backend, input: &Fixture) -> Stats {
    let mut stats = Stats::default();
    for key in &input.order {
        let id = store.ids[input.offsets[key.kind] + key.row];
        stats.sum += sum_vector(store.world.get::<Position>(id).unwrap().0);
        stats.rows += 1;
    }
    stats
}
pub(super) fn measure(run: &Run, store: &Backend) {
    let expected: f64 = run
        .input
        .order
        .iter()
        .map(|key| sum_vector(run.input.values[key.kind][key.row].position))
        .sum();
    let start = Measurement::begin();
    let stats = position_lookup_bevy(store, run.input);
    let elapsed = start.end();
    assert_eq!(stats.sum, expected);
    assert_eq!(stats.rows, run.input.count as u64);
    run.record("position_lookup", elapsed, stats.rows, stats);
}
