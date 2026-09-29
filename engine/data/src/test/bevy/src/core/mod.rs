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

mod movement;
mod nearby_lights;
mod position_lookup;

use crate::{Measurement, Run, backend::*, fixture::*, memory};

impl Run<'_> {
    pub(super) fn core(&self) {
        assert!(matches!(self.kind, Kind::Columns));
        let input = self.input;
        let setup = Run {
            input,
            kind: self.kind,
            sample: 0,
        };
        let mut store = setup.create("setup");
        self.populate(&mut store, true, "create_population");
        memory::domain(memory::BACKEND);
        let mut movement = movement::query(&mut store.world);
        let mut explosion = Query::new(&mut store, Selection::Health);
        let mut lights = nearby_lights::query(&mut store.world);
        memory::domain(memory::NONE);
        movement::measure(self, &mut store, &mut movement);
        store.reset(input);
        self.query(
            &mut store,
            &mut explosion,
            Selection::Health,
            "explosion_r50_first",
            50,
            0,
            true,
            false,
            0.0,
        );
        self.validate_health(&mut store, 50, 1);
        store.reset(input);
        nearby_lights::measure(self, &mut store, &mut lights);
        position_lookup::measure(self, &store);
        let start = Measurement::begin();
        for kind in 0..6 {
            let extra = input.counts[kind] / 10;
            for r in (0..extra).step_by(100) {
                store.insert(
                    input,
                    kind,
                    input.counts[kind] + r,
                    (extra - r).min(100),
                    true,
                );
            }
        }
        self.record(
            "spawn_wave",
            start.end(),
            (input.total - input.count) as u64,
            Stats::default(),
        );
        store.validate_identity(input, true);
        for kind in 0..6 {
            for r in 0..input.counts[kind] + input.counts[kind] / 10 {
                assert_eq!(
                    store
                        .world
                        .get::<Position>(store.ids[input.offsets[kind] + r])
                        .unwrap()
                        .0,
                    input.values[kind][r].position
                );
            }
        }
        memory::domain(memory::BACKEND);
        let expected = |types: &[usize], removed: usize| -> u64 {
            let total: usize = types
                .iter()
                .map(|&kind| input.counts[kind] + input.counts[kind] / 10)
                .sum();
            (total
                - input.order[..removed]
                    .iter()
                    .filter(|key| types.contains(&key.kind))
                    .count()) as u64
        };
        assert_eq!(
            movement.iter_mut(&mut store.world).count() as u64,
            expected(&[2, 3], 0)
        );
        assert_eq!(
            explosion.read(&mut store.world, -1, false).rows,
            expected(&[2, 3, 5], 0)
        );
        assert_eq!(
            lights.iter(&store.world).count() as u64,
            expected(&[0, 1], 0)
        );
        memory::domain(memory::NONE);
        let removed = input.count / 100;
        let start = Measurement::begin();
        for key in &input.order[..removed] {
            assert!(
                store
                    .world
                    .despawn(store.ids[input.offsets[key.kind] + key.row])
            );
        }
        self.record(
            "despawn_wave",
            start.end(),
            removed as u64,
            Stats::default(),
        );
        for key in &input.order[..removed] {
            assert!(
                !store
                    .world
                    .entities()
                    .contains(store.ids[input.offsets[key.kind] + key.row])
            );
        }
        memory::domain(memory::BACKEND);
        assert_eq!(
            movement.iter_mut(&mut store.world).count() as u64,
            expected(&[2, 3], removed)
        );
        assert_eq!(
            explosion.read(&mut store.world, -1, false).rows,
            expected(&[2, 3, 5], removed)
        );
        assert_eq!(
            lights.iter(&store.world).count() as u64,
            expected(&[0, 1], removed)
        );
        memory::domain(memory::NONE);
        drop((movement, explosion, lights));
        setup.destroy(store, "destroy");
    }
}
