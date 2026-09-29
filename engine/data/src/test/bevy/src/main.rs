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

mod backend;
mod core;
mod fixture;
mod memory;

use backend::*;
use bevy_ecs::entity::Entity;
use fixture::*;
use std::{hint::black_box, time::Instant};

const READ_PASSES: u32 = 5;

struct Measurement {
    start: Instant,
    before: memory::Counts,
}
struct Finished {
    ns: f64,
    before: memory::Counts,
    after: memory::Counts,
}

impl Measurement {
    fn begin() -> Self {
        Self {
            before: memory::begin(),
            start: Instant::now(),
        }
    }
    fn end(self) -> Finished {
        let ns = self.start.elapsed().as_nanos() as f64;
        let after = memory::counts(memory::BACKEND);
        memory::domain(memory::NONE);
        Finished {
            ns,
            before: self.before,
            after,
        }
    }
}

struct Run<'a> {
    input: &'a Fixture,
    kind: Kind,
    sample: u32,
}

impl Run<'_> {
    fn record(&self, name: &str, measurement: Finished, operations: u64, stats: Stats) {
        black_box(stats.sum);
        if self.sample == 0 {
            return;
        }
        if cfg!(feature = "memory") {
            let before = measurement.before;
            let after = measurement.after;
            let fixture = memory::counts(memory::FIXTURE);
            println!(
                "{},{},{},{},{},{},{},{:.9},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},0,0",
                self.kind.name(),
                name,
                self.input.count,
                self.sample,
                operations,
                stats.hits,
                stats.batches,
                stats.sum,
                before.bytes,
                after.bytes,
                after.peak_bytes,
                after.blocks,
                after.peak_blocks,
                after.allocations - before.allocations,
                after.reallocations - before.reallocations,
                after.frees - before.frees,
                after.allocated_bytes - before.allocated_bytes,
                after.allocations,
                after.reallocations,
                after.frees,
                after.allocated_bytes,
                fixture.bytes,
                fixture.blocks
            );
        } else {
            println!(
                "{},{},{},{},{},{},{},{:.6},{:.3},{:.9}",
                self.kind.name(),
                name,
                self.input.count,
                self.sample,
                operations,
                stats.hits,
                stats.batches,
                measurement.ns / 1e6,
                measurement.ns / operations.max(1) as f64,
                stats.sum
            );
        }
    }

    fn record_memory(&self, name: &str, measurement: Finished, operations: u64) {
        if cfg!(feature = "memory") {
            self.record(name, measurement, operations, Stats::default());
        }
    }

    fn create(&self, name: &str) -> Backend {
        memory::domain(memory::FIXTURE);
        let ids = vec![Entity::PLACEHOLDER; self.input.total];
        memory::begin_backend();
        let start = Measurement::begin();
        let store = Backend::new(self.kind, ids);
        self.record_memory(name, start.end(), 1);
        store
    }

    fn destroy(&self, store: Backend, name: &str) {
        let start = Measurement::begin();
        drop(store);
        self.record_memory(name, start.end(), 1);
        assert_eq!(
            memory::counts(memory::BACKEND).bytes,
            0,
            "backend leaked bytes"
        );
        assert_eq!(
            memory::counts(memory::BACKEND).blocks,
            0,
            "backend leaked blocks"
        );
    }

    fn populate(&self, store: &mut Backend, bulk: bool, name: &str) {
        let start = Measurement::begin();
        for kind in 0..6 {
            store.insert(self.input, kind, 0, self.input.counts[kind], bulk);
        }
        self.record(name, start.end(), self.input.count as u64, Stats::default());
        store.validate_identity(self.input, false);
    }

    fn query(
        &self,
        store: &mut Backend,
        query: &mut Query,
        selection: Selection,
        name: &str,
        radius: i32,
        prior_writes: u32,
        write: bool,
        extra: bool,
        extra_sum: f64,
    ) {
        let mut expected = Stats::default();
        for kind in 0..6 {
            if !selection.matches(kind) {
                continue;
            }
            let count = self.input.counts[kind]
                + if extra {
                    self.input.counts[kind] / 10
                } else {
                    0
                };
            for v in &self.input.values[kind][..count] {
                if selection.color() {
                    expected.color(v.light.color);
                } else {
                    let health = damaged(
                        v.health,
                        if hit(v.position, radius) {
                            prior_writes
                        } else {
                            0
                        },
                    );
                    expected.health(v.position, health, radius, write);
                }
            }
        }
        expected.sum += extra_sum;
        let passes = if write { 1 } else { READ_PASSES };
        let mut stats = Stats::default();
        let start = Measurement::begin();
        for _ in 0..passes {
            stats.add(query.read(&mut store.world, radius, write));
        }
        let measurement = start.end();
        assert_eq!(stats.sum, expected.sum * passes as f64, "{name}: checksum");
        assert_eq!(stats.hits, expected.hits * passes as u64, "{name}: hits");
        assert_eq!(
            stats.rows,
            expected.rows * passes as u64,
            "{name}: visited rows"
        );
        self.record(name, measurement, stats.rows, stats);
    }

    fn read(
        &self,
        store: &mut Backend,
        query: &mut Query,
        selection: Selection,
        name: &str,
        extra: bool,
    ) {
        self.query(store, query, selection, name, -1, 0, false, extra, 0.0);
    }

    fn validate_health(&self, store: &mut Backend, radius: i32, writes: u32) {
        for kind in 0..6 {
            if !has_health(kind) {
                continue;
            }
            for r in 0..self.input.counts[kind] {
                let v = self.input.values[kind][r];
                let expected = damaged(v.health, if hit(v.position, radius) { writes } else { 0 });
                assert_eq!(
                    store.scalar(kind, store.ids[self.input.offsets[kind] + r], false),
                    expected
                );
            }
        }
    }

    fn shuffled_position(&self, store: &mut Backend) {
        let expected: f64 = self
            .input
            .order
            .iter()
            .map(|key| sum_vector(self.input.values[key.kind][key.row].position) + 1.0)
            .sum();
        let mut stats = Stats::default();
        let start = Measurement::begin();
        for key in &self.input.order {
            let id = store.ids[self.input.offsets[key.kind] + key.row];
            stats.sum += sum_vector(store.position(key.kind, id, true));
        }
        let measurement = start.end();
        assert_eq!(stats.sum, expected);
        // Verify persisted writes outside the measured loop.
        for kind in 0..6 {
            for row in 0..self.input.counts[kind] {
                let mut expected = self.input.values[kind][row].position;
                expected[0] += 1.0;
                assert_eq!(
                    store.position(kind, store.ids[self.input.offsets[kind] + row], false),
                    expected
                );
            }
        }
        self.record(
            "shuffled_position",
            measurement,
            self.input.count as u64,
            stats,
        );
    }

    fn dense(&self) {
        let input = self.input;
        let mut store = self.create("setup_individual");
        self.populate(&mut store, false, "create_individual");
        self.destroy(store, "destroy_individual");
        let mut store = self.create("setup_bulk");
        self.populate(&mut store, true, "create_bulk");
        let start = Measurement::begin();
        let mut spot = Query::new(&mut store, Selection::Spot);
        let mut light = Query::new(&mut store, Selection::Light);
        let mut health = Query::new(&mut store, Selection::Health);
        let mut enemy = Query::new(&mut store, Selection::Enemy);
        let mut explosion = Query::new(&mut store, Selection::Health);
        self.record("create_queries", start.end(), 5, Stats::default());
        self.read(&mut store, &mut spot, Selection::Spot, "spot_color", false);
        self.read(
            &mut store,
            &mut light,
            Selection::Light,
            "all_light_color",
            false,
        );
        self.read(
            &mut store,
            &mut health,
            Selection::Health,
            "health_position",
            false,
        );
        self.read(
            &mut store,
            &mut enemy,
            Selection::Enemy,
            "enemy_health_position",
            false,
        );
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
        self.shuffled_position(&mut store);
        store.reset(input);
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
            "add_10pct_live_queries_batch100",
            start.end(),
            (input.total - input.count) as u64,
            Stats::default(),
        );
        store.validate_identity(input, true);
        self.read(
            &mut store,
            &mut light,
            Selection::Light,
            "all_light_after_add",
            true,
        );
        self.read(
            &mut store,
            &mut health,
            Selection::Health,
            "health_after_add",
            true,
        );
        let removed = input.count / 100;
        let start = Measurement::begin();
        for key in &input.order[..removed] {
            assert!(
                store
                    .world
                    .despawn(store.ids[input.offsets[key.kind] + key.row])
            );
        }
        self.record("remove_1pct", start.end(), removed as u64, Stats::default());
        for key in &input.order[..removed] {
            assert!(
                !store
                    .world
                    .entities()
                    .contains(store.ids[input.offsets[key.kind] + key.row])
            );
        }
        let start = Measurement::begin();
        for key in &input.order[..removed] {
            store.insert(input, key.kind, key.row, 1, false);
        }
        self.record(
            "replace_1pct",
            start.end(),
            removed as u64,
            Stats::default(),
        );
        self.read(
            &mut store,
            &mut health,
            Selection::Health,
            "health_after_churn",
            true,
        );
        store.validate_identity(input, true);
        let start = Measurement::begin();
        drop((spot, light, health, enemy, explosion));
        self.record_memory("destroy_queries", start.end(), 5);
        self.destroy(store, "destroy_bulk");
    }

    fn packed_access(&self, store: &mut Backend, percent: usize, write: u32) {
        let input = self.input;
        let changed = input.count / 100 * percent;
        let count = if write == 0 { input.count } else { changed };
        let expected: f64 = input.order[..count]
            .iter()
            .enumerate()
            .map(|(i, key)| {
                scalar(&input.values[key.kind][key.row], key.kind)
                    + if i < changed {
                        if write == 0 { 2.0 } else { write as f64 }
                    } else {
                        0.0
                    }
            })
            .sum();
        let mut stats = Stats::default();
        let start = Measurement::begin();
        for key in &input.order[..count] {
            stats.sum += store.scalar(
                key.kind,
                store.ids[input.offsets[key.kind] + key.row],
                write != 0,
            );
        }
        let measurement = start.end();
        assert_eq!(stats.sum, expected);
        let suffix = ["random_get", "update_first", "update_repeat"][write as usize];
        self.record(
            &format!("packed_{percent}pct_{suffix}"),
            measurement,
            count as u64,
            stats,
        );
    }

    fn packed(&self) {
        let mut store = self.create("setup_packed");
        self.populate(&mut store, true, "packed_populate_and_ids");
        let start = Measurement::begin();
        let mut health = Query::new(&mut store, Selection::Health);
        let mut light = Query::new(&mut store, Selection::Light);
        self.record("packed_create_queries", start.end(), 2, Stats::default());
        self.read(
            &mut store,
            &mut light,
            Selection::Light,
            "packed_light_color",
            false,
        );
        for percent in [0, 1, 10] {
            if percent != 0 {
                self.packed_access(&mut store, percent, 1);
                self.packed_access(&mut store, percent, 2);
            }
            self.packed_access(&mut store, percent, 0);
            let changed = self.input.count / 100 * percent;
            let health_changes = self.input.order[..changed]
                .iter()
                .filter(|key| has_health(key.kind))
                .count();
            self.query(
                &mut store,
                &mut health,
                Selection::Health,
                &format!("packed_{percent}pct_health_query"),
                -1,
                0,
                false,
                false,
                (health_changes * 2) as f64,
            );
            // Restore fixture values outside measurement. This is not a native
            // packed default/override/reset contract and is not timed as one.
            store.reset(self.input);
            self.validate_health(&mut store, -1, 0);
        }
        let start = Measurement::begin();
        drop((health, light));
        self.record_memory("destroy_queries", start.end(), 2);
        self.destroy(store, "destroy_packed");
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    assert!(
        args.len() <= 5,
        "Usage: benchmark-data-bevy [rows] [samples] [all|bevy_rows|bevy_columns] [0|1|4|16|core]"
    );
    let count: usize = args
        .get(1)
        .map_or(1_000_000, |s| s.parse().expect("row count"));
    let samples: u32 = args.get(2).map_or(7, |s| s.parse().expect("sample count"));
    let backend = args.get(3).map_or("all", String::as_str);
    let core = args.get(4).is_some_and(|s| s == "core");
    assert!(!core || backend == "bevy_columns");
    let group: usize = if core {
        0
    } else {
        args.get(4).map_or(0, |s| s.parse().expect("group size"))
    };
    assert!((1_000..=10_000_000).contains(&count) && count % 1000 == 0);
    assert!((1..=31).contains(&samples));
    assert!(matches!(group, 0 | 1 | 4 | 16));
    assert!(matches!(backend, "all" | "bevy_rows" | "bevy_columns"));
    memory::domain(memory::FIXTURE);
    let input = Fixture::new(count, group);
    memory::domain(memory::NONE);
    if core {
        println!(
            "# suite=core; seven standalone cases; one pass; three live queries; no alternate layouts"
        );
    }
    let read_passes = if core { 1 } else { READ_PASSES };
    println!(
        "# packed_rows={group}; repeated prototype values; Bevy always owns mutable component values"
    );
    println!(
        "# Bevy ECS 0.19.1; seed={SEED}; samples={samples}; one warmup; read_passes={read_passes}; damage=25; explosion_radius=50"
    );
    println!(
        "# single-threaded World/QueryState; default table storage; normal change tracking; no rendering, schedule, serialization, reset/unload timing or disk I/O"
    );
    println!(
        "# batches counts matched tables per query pass; fixture and caller ID arrays excluded from backend heap; no RSS or allocator overhead"
    );
    if cfg!(feature = "memory") {
        println!(
            "backend,operation,rows,sample,operations,hits,batches,checksum,before_bytes,live_bytes,peak_bytes,live_blocks,peak_blocks,allocations,reallocations,frees,allocated_bytes,total_allocations,total_reallocations,total_frees,total_allocated_bytes,fixture_bytes,fixture_blocks,shared_bytes,shared_blocks"
        );
    } else {
        println!(
            "backend,operation,rows,sample,operations,hits,batches,total_ms,ns_per_operation,checksum"
        );
    }
    for sample in 0..=samples {
        for index in 0..2 {
            let kind = if (sample as usize + index) % 2 == 0 {
                Kind::Rows
            } else {
                Kind::Columns
            };
            if backend != "all" && backend != kind.name() {
                continue;
            }
            eprintln!("sample {sample}/{samples}: {}, {count} rows", kind.name());
            let run = Run {
                input: &input,
                kind,
                sample,
            };
            if core {
                run.core();
            } else if input.group == 0 {
                run.dense();
            } else {
                run.packed();
            }
        }
    }
    drop(input);
    assert_eq!(
        memory::counts(memory::FIXTURE).blocks,
        0,
        "fixture leaked blocks"
    );
}
