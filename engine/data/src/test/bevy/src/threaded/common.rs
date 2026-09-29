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

use bevy_ecs::prelude::*;
use std::sync::Mutex;

#[derive(Component, Clone, Copy, Default)]
pub struct Position(pub [f32; 3]);
#[derive(Component, Clone, Copy, Default)]
pub struct Velocity(pub [f32; 3]);
#[derive(Component, Clone, Copy, Default)]
pub struct Health(pub f64);
#[derive(Component, Clone, Copy, Default)]
pub struct Light {
    pub color: [f32; 3],
    pub intensity: f64,
}
#[derive(Component)]
pub struct Spot(pub [f64; 3]);
#[derive(Component)]
pub struct Point(pub f64);
#[derive(Component)]
pub struct Player;
#[derive(Component)]
pub struct Enemy;
#[derive(Component)]
pub struct Pickup(pub f64);
#[derive(Component)]
pub struct Breakable;

#[derive(Clone, Copy, Default, Debug)]
pub struct Stats {
    pub sum: f64,
    pub rows: u32,
    pub hits: u32,
}
impl Stats {
    pub fn add(&mut self, rhs: &Stats) {
        self.sum += rhs.sum;
        self.rows += rhs.rows;
        self.hits += rhs.hits;
    }
    pub fn check(&self, rhs: &Self) {
        assert_eq!((self.rows, self.hits), (rhs.rows, rhs.hits));
        assert!(
            (self.sum - rhs.sum).abs() <= 1e-7 * (1.0 + self.sum.abs()),
            "checksum: {self:?} != {rhs:?}"
        );
    }
}

#[derive(Default)]
pub struct TraceState {
    pub order: Vec<usize>,
    pub active: u32,
    pub results: [Stats; 4],
}
#[derive(Resource, Default)]
pub struct Trace(pub Mutex<TraceState>);
impl Trace {
    // These assertions observe scheduling; they do not wait or synchronize the
    // component accesses. Only Bevy's Query access declarations protect the rows.
    pub fn begin(&self, task: usize) {
        let mut state = self.0.lock().unwrap();
        let conflicts = [0b0010, 0b0101, 0b0010, 0];
        assert_eq!(
            state.active & conflicts[task],
            0,
            "conflicting systems overlapped"
        );
        assert_eq!(state.active & (1 << task), 0);
        state.active |= 1 << task;
        state.order.push(task);
    }
    pub fn finish(&self, task: usize, stats: Stats) {
        let mut state = self.0.lock().unwrap();
        state.results[task] = stats;
        state.active &= !(1 << task);
    }
}

#[derive(Clone, Copy, Default)]
pub struct Row {
    pub position: Position,
    pub velocity: Velocity,
    pub health: Health,
    pub light: Light,
    pub kind: usize,
    pub live: bool,
}
pub const COUNTS: [usize; 6] = [10, 15, 1, 24, 25, 25];
pub fn random(state: &mut u32) -> u32 {
    *state = state.wrapping_mul(1664525).wrapping_add(1013904223);
    *state
}
pub fn defaults(enemies: bool) -> [Row; 100] {
    let mut output = [Row::default(); 100];
    let mut state = if enemies { 0x9abc } else { 0x1234 };
    let mut offset = 0;
    for kind in 0..COUNTS.len() {
        for r in 0..type_rows(kind, enemies) {
            let row = &mut output[offset];
            offset += 1;
            row.kind = kind;
            row.live = true;
            for axis in 0..3 {
                row.position.0[axis] = ((random(&mut state) >> 25) as i32 - 64) as f32;
                row.light.color[axis] = (random(&mut state) >> 24) as f32 / 256.0;
            }
            row.velocity.0 = [1.0, 0.0, -1.0];
            row.health.0 = match r % 4 {
                0 => 100.0,
                1 => 99.9,
                _ => ((random(&mut state) >> 16) % 101) as f64,
            };
            row.light.intensity = 1.0;
        }
    }
    output
}
pub fn type_rows(kind: usize, enemies: bool) -> usize {
    if enemies {
        if kind == 3 { 100 } else { 0 }
    } else {
        COUNTS[kind]
    }
}

// Same frame seed and integer operations as ThreadedPlanWave in the C++ fixture.
#[derive(Default)]
pub struct Wave {
    pub content: usize,
    pub enemy_add: usize,
    pub enemy_remove: usize,
}
pub fn plan_wave(initial: usize, frame: usize, live_enemies: usize) -> Wave {
    let mut state = 0x91e10da5 ^ frame as u32;
    let content_limit = initial / 100 + 1;
    let enemy_limit = initial / 10;
    let mut wave = Wave {
        content: 1 + (random(&mut state) >> 16) as usize % content_limit,
        ..Default::default()
    };
    match frame % 8 {
        0 => wave.enemy_add = enemy_limit + (random(&mut state) >> 16) as usize % enemy_limit,
        1 | 3 => {
            wave.enemy_add = (random(&mut state) >> 16) as usize % (enemy_limit / 2 + 1);
            wave.enemy_remove = (random(&mut state) >> 16) as usize % (enemy_limit / 2 + 1);
        }
        2 => {
            wave.enemy_remove = enemy_limit / 2 + (random(&mut state) >> 16) as usize % enemy_limit
        }
        4 | 7 => wave.enemy_remove = live_enemies,
        5 => {}
        6 => wave.enemy_add = 1 + (random(&mut state) >> 16) as usize % enemy_limit,
        _ => unreachable!(),
    }
    wave.enemy_remove = wave.enemy_remove.min(live_enemies);
    wave
}
pub fn group_capacity(initial: usize, frames: usize) -> usize {
    let (mut capacity, mut enemies) = (initial, 0);
    for frame in 0..frames {
        let wave = plan_wave(initial, frame, enemies);
        capacity += wave.content + wave.enemy_add;
        enemies = enemies - wave.enemy_remove + wave.enemy_add;
    }
    capacity
}
pub fn has_health(kind: usize) -> bool {
    matches!(kind, 2 | 3 | 5)
}
pub fn distance(position: &Position) -> f32 {
    let p = position.0;
    p[0] * p[0] + p[1] * p[1] + p[2] * p[2]
}
pub fn in_radius(position: &Position) -> bool {
    distance(position) <= 2500.0
}
pub fn contribution(position: &Position, light: &Light) -> f64 {
    (light.color[0] as f64 + light.color[1] as f64 + light.color[2] as f64)
        * light.intensity
        * (1.0 - distance(position) as f64 / 2500.0)
}

// Verify the configured worker budget without adding work to release timings.
#[cfg(data_tsan)]
pub fn check_worker() {
    assert!(
        std::thread::current()
            .name()
            .unwrap_or("")
            .starts_with("bevy-benchmark"),
        "row work escaped the configured worker pool"
    );
}
