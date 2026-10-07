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

use crate::{
    Measurement, Run,
    backend::{Backend, Stats},
    fixture::*,
    memory,
};
use bevy_ecs::{prelude::*, query::QueryFilter};

type FlockQuery<F> = QueryState<(&'static mut Position, &'static mut Velocity), F>;
pub(super) struct Queries {
    pub players: FlockQuery<With<PlayerTag>>,
    pub enemies: FlockQuery<With<EnemyTag>>,
}
pub(super) fn query(world: &mut World) -> Queries {
    Queries {
        players: world.query_filtered(),
        enemies: world.query_filtered(),
    }
}

#[derive(Clone, Copy, Default)]
#[repr(C)]
struct Boid {
    position: Vector3,
    velocity: Vector3,
}
#[derive(Clone, Copy, Default)]
#[repr(C)]
struct Cell {
    coordinates: [i32; 3],
    count: u32,
    position_sum: Vector3,
    velocity_sum: Vector3,
    target: u32,
    avoid: bool,
}
struct Scratch {
    snapshot: Vec<Boid>,
    cells: Vec<Cell>,
    row_cells: Vec<u32>,
    slots: Vec<u32>,
    cell_count: usize,
}
const TARGETS: [Vector3; 2] = [[38.0, 27.0, 35.0], [-42.0, 29.0, -32.0]];
fn add(a: Vector3, b: Vector3) -> Vector3 {
    [a[0] + b[0], a[1] + b[1], a[2] + b[2]]
}
fn sub(a: Vector3, b: Vector3) -> Vector3 {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}
fn mul(a: Vector3, scale: f32) -> Vector3 {
    [a[0] * scale, a[1] * scale, a[2] * scale]
}
fn length_squared(a: Vector3) -> f32 {
    a[0] * a[0] + a[1] * a[1] + a[2] * a[2]
}
fn normalize(a: Vector3, fallback: Vector3) -> Vector3 {
    let length = length_squared(a);
    if length > 1e-12 {
        mul(a, 1.0 / length.sqrt())
    } else {
        fallback
    }
}
impl Scratch {
    fn new(count: usize) -> Self {
        Self {
            snapshot: vec![Boid::default(); count],
            cells: vec![Cell::default(); count],
            row_cells: vec![0; count],
            slots: vec![0; (count * 2).next_power_of_two()],
            cell_count: 0,
        }
    }
    fn build_cells(&mut self) {
        self.slots.fill(0);
        self.cell_count = 0;
        let mask = self.slots.len() - 1;
        for (i, row) in self.snapshot.iter().enumerate() {
            let coordinates = row.position.map(|v| (v * 0.125).floor() as i32);
            let mut slot = ((coordinates[0] as u32).wrapping_mul(73856093)
                ^ (coordinates[1] as u32).wrapping_mul(19349663)
                ^ (coordinates[2] as u32).wrapping_mul(83492791))
                as usize
                & mask;
            while self.slots[slot] != 0 {
                if self.cells[(self.slots[slot] - 1) as usize].coordinates == coordinates {
                    break;
                }
                slot = (slot + 1) & mask;
            }
            if self.slots[slot] == 0 {
                self.cells[self.cell_count] = Cell {
                    coordinates,
                    ..Default::default()
                };
                self.cell_count += 1;
                self.slots[slot] = self.cell_count as u32;
            }
            let index = self.slots[slot] - 1;
            let cell = &mut self.cells[index as usize];
            cell.count += 1;
            cell.position_sum = add(cell.position_sum, row.position);
            cell.velocity_sum = add(cell.velocity_sum, row.velocity);
            self.row_cells[i] = index;
        }
        for cell in &mut self.cells[..self.cell_count] {
            let center = mul(cell.position_sum, 1.0 / cell.count as f32);
            cell.target = u32::from(
                length_squared(sub(TARGETS[0], center)) > length_squared(sub(TARGETS[1], center)),
            );
            cell.avoid = length_squared(center) < 900.0;
        }
    }
}
fn steer(row: Boid, cell: Cell) -> Boid {
    let zero = [0.0; 3];
    let heading = normalize(row.velocity, [0.0, 0.0, 1.0]);
    let alignment = normalize(
        sub(
            mul(cell.velocity_sum, 1.0 / cell.count as f32),
            row.velocity,
        ),
        zero,
    );
    let separation = normalize(
        sub(mul(row.position, cell.count as f32), cell.position_sum),
        zero,
    );
    let attraction = mul(
        normalize(sub(TARGETS[cell.target as usize], row.position), zero),
        2.0,
    );
    let mut desired = normalize(add(add(alignment, separation), attraction), heading);
    if cell.avoid {
        let away = normalize(row.position, heading);
        desired = normalize(sub(mul(away, 30.0), row.position), away);
    }
    let heading = normalize(add(heading, mul(sub(desired, heading), 0.015625)), heading);
    let velocity = mul(heading, 25.0);
    Boid {
        position: add(row.position, mul(velocity, 0.015625)),
        velocity,
    }
}
fn checksum(row: Boid) -> f64 {
    (0..3)
        .map(|axis| (row.position[axis] as f64).powi(2) + (row.velocity[axis] as f64).powi(2))
        .sum()
}
fn boids_bevy<F: QueryFilter>(
    world: &mut World,
    query: &mut FlockQuery<F>,
    scratch: &mut Scratch,
) -> Stats {
    // Keep query iteration order stable between snapshot and writeback; no structural changes.
    for (i, (position, velocity)) in query.iter(world).enumerate() {
        scratch.snapshot[i] = Boid {
            position: position.0,
            velocity: velocity.0,
        };
    }
    scratch.build_cells();
    let mut stats = Stats::default();
    for (i, (mut position, mut velocity)) in query.iter_mut(world).enumerate() {
        let cell = scratch.cells[scratch.row_cells[i] as usize];
        let result = steer(scratch.snapshot[i], cell);
        position.0 = result.position;
        velocity.0 = result.velocity;
        stats.sum += checksum(result);
        stats.hits += u64::from(cell.avoid);
        stats.rows += 1;
    }
    stats
}
pub(super) fn measure(run: &Run, store: &mut Backend, queries: &mut Queries) {
    memory::domain(memory::BACKEND);
    let mut scratch = [
        Scratch::new(run.input.counts[2]),
        Scratch::new(run.input.counts[3]),
    ];
    let start = Measurement::begin();
    let mut stats = boids_bevy(&mut store.world, &mut queries.players, &mut scratch[0]);
    stats.add(boids_bevy(
        &mut store.world,
        &mut queries.enemies,
        &mut scratch[1],
    ));
    let elapsed = start.end();
    assert_eq!(
        stats.rows as usize,
        run.input.counts[2] + run.input.counts[3]
    );
    for kind in 2..=3 {
        let reference = &mut scratch[kind - 2];
        for (row, input) in reference.snapshot.iter_mut().zip(&run.input.values[kind]) {
            *row = Boid {
                position: input.position,
                velocity: [1.0, 0.0, -1.0],
            };
        }
        reference.build_cells();
        for r in 0..run.input.counts[kind] {
            let expected = steer(
                reference.snapshot[r],
                reference.cells[reference.row_cells[r] as usize],
            );
            let id = store.ids[run.input.offsets[kind] + r];
            let actual = Boid {
                position: store.world.get::<Position>(id).unwrap().0,
                velocity: store.world.get::<Velocity>(id).unwrap().0,
            };
            for axis in 0..3 {
                assert!(actual.position[axis].is_finite() && actual.velocity[axis].is_finite());
                assert!((actual.position[axis] - expected.position[axis]).abs() < 1e-4);
                assert!((actual.velocity[axis] - expected.velocity[axis]).abs() < 1e-4);
            }
            assert!((length_squared(actual.velocity) - 625.0).abs() < 0.001);
        }
    }
    run.record("boids", elapsed, stats.rows, stats);
}
