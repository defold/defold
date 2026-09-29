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

// The scheduler owns its thread pool; the caller keeps content preparation and
// structural edits outside the world borrow held by Schedule::run.
#[allow(dead_code)]
mod common;
mod explosion;
mod lights;
#[allow(dead_code, unused_imports)]
#[path = "../memory.rs"]
mod memory;
mod movement;
mod regenerate;
use bevy_ecs::{prelude::*, schedule::MultiThreadedExecutor};
use bevy_tasks::{ComputeTaskPool, TaskPoolBuilder};
use common::*;
use std::{sync::mpsc::sync_channel, thread, time::Instant};

fn add_group(
    world: &mut World,
    defaults: &[Row; 100],
    entities: &mut Vec<Entity>,
    live: &mut Vec<usize>,
    enemies: bool,
) {
    live.push(entities.len() / 100);
    let mut first = 0;
    for kind in 0..COUNTS.len() {
        let count = type_rows(kind, enemies);
        if count == 0 {
            continue;
        }
        let rows = &defaults[first..first + count];
        match kind {
            0 => entities.extend(
                world.spawn_batch(rows.iter().map(|r| (r.position, r.light, Spot([10.0; 3])))),
            ),
            1 => entities
                .extend(world.spawn_batch(rows.iter().map(|r| (r.position, r.light, Point(10.0))))),
            2 => entities.extend(
                world.spawn_batch(
                    rows.iter()
                        .map(|r| (r.position, r.velocity, r.health, Player)),
                ),
            ),
            3 => entities.extend(
                world.spawn_batch(
                    rows.iter()
                        .map(|r| (r.position, r.velocity, r.health, Enemy)),
                ),
            ),
            4 => {
                entities.extend(world.spawn_batch(rows.iter().map(|r| (r.position, Pickup(10.0)))))
            }
            5 => entities
                .extend(world.spawn_batch(rows.iter().map(|r| (r.position, r.health, Breakable)))),
            _ => unreachable!(),
        }
        first += count;
    }
}
fn validate(world: &World, reference: &[Row], entities: &[Entity]) {
    for (row, entity) in reference.iter().zip(entities) {
        assert_eq!(
            world.get_entity(*entity).is_ok(),
            row.live,
            "stale entity after reuse"
        );
        if !row.live {
            continue;
        }
        assert_eq!(world.get::<Position>(*entity).unwrap().0, row.position.0);
        if has_health(row.kind) {
            assert_eq!(world.get::<Health>(*entity).unwrap().0, row.health.0);
        }
    }
}
fn schedule() -> Schedule {
    let mut schedule = Schedule::default();
    schedule.set_executor(MultiThreadedExecutor::new());
    // No chain or before/after constraint: Query<&mut Health> makes Explosion
    // and Regenerate conflict. Bevy decides which ready system executes first.
    schedule.add_systems((
        movement::movement_bevy,
        explosion::explosion_bevy,
        regenerate::regenerate_bevy,
        lights::nearby_lights_bevy,
    ));
    schedule
}
fn run(population: usize, frames: usize, workers: usize) {
    let defaults = [defaults(false), defaults(true)];
    let initial = population / 100;
    let capacity = group_capacity(initial, frames);
    let mut entities = Vec::with_capacity(capacity * 100);
    let mut reference = Vec::with_capacity(capacity * 100);
    let mut live = [Vec::with_capacity(capacity), Vec::with_capacity(capacity)];
    let mut removed = Vec::with_capacity(capacity);
    memory::domain(memory::BACKEND);
    ComputeTaskPool::get_or_init(|| {
        TaskPoolBuilder::new()
            .num_threads(workers)
            .thread_name("bevy-benchmark".into())
            .on_thread_spawn(|| memory::domain(memory::BACKEND))
            .build()
    });
    assert_eq!(ComputeTaskPool::get().thread_num(), workers);
    let mut world = World::new();
    let mut trace = TraceState::default();
    trace.order.reserve(4);
    world.insert_resource(Trace(std::sync::Mutex::new(trace)));
    for _ in 0..initial {
        add_group(&mut world, &defaults[0], &mut entities, &mut live[0], false);
        reference.extend_from_slice(&defaults[0]);
    }
    let mut schedule = schedule();
    schedule.initialize(&mut world).unwrap();
    // The persistent coordinator exclusively owns World while updates run. The
    // application thread prepares incoming content concurrently, then receives
    // World back before despawning/spawning. Channels transfer ownership.
    let (send, receive) = sync_channel::<Option<World>>(0);
    let (done, completed) = sync_channel::<World>(0);
    let coordinator = thread::Builder::new()
        .name("bevy-schedule".into())
        .spawn(move || {
            memory::domain(memory::BACKEND);
            while let Some(mut world) = receive.recv().unwrap() {
                schedule.run(&mut world);
                done.send(world).unwrap();
            }
        })
        .unwrap();
    let mut state = 0x5678;
    for frame in 0..frames {
        let wave = plan_wave(initial, frame, live[1].len());
        // Consume the same candidate shuffle as the caller-scheduled backends so
        // streaming selects identical groups. Bevy owns its own admission order.
        for u in (1..4).rev() {
            let _ = (random(&mut state) >> 16) % (u + 1);
        }
        {
            let mut trace = world.resource::<Trace>().0.lock().unwrap();
            trace.order.clear();
            assert_eq!(trace.active, 0);
        }
        let before = memory::begin();
        let start = Instant::now();
        send.send(Some(world)).unwrap();
        let pending = Box::new(defaults[0]);
        let pending_enemies = (wave.enemy_add != 0).then(|| Box::new(defaults[1]));
        world = completed.recv().unwrap();
        let update = memory::counts(memory::BACKEND);
        let mutation_start = Instant::now();
        removed.clear();
        for (kind, groups) in live.iter_mut().enumerate() {
            let count = if kind == 0 {
                wave.content
            } else {
                wave.enemy_remove
            };
            for _ in 0..count {
                let slot = (random(&mut state) >> 8) as usize % groups.len();
                let group = groups.swap_remove(slot);
                removed.push(group);
                for entity in &entities[group * 100..(group + 1) * 100] {
                    assert!(world.despawn(*entity));
                }
            }
        }
        let first_added = entities.len() / 100;
        for _ in 0..wave.content {
            add_group(&mut world, &pending, &mut entities, &mut live[0], false);
        }
        let first_enemy = entities.len() / 100;
        if let Some(pending) = &pending_enemies {
            for _ in 0..wave.enemy_add {
                add_group(&mut world, pending, &mut entities, &mut live[1], true);
            }
        }
        drop(pending);
        drop(pending_enemies);
        let mutation_us = mutation_start.elapsed().as_micros();
        let elapsed_us = start.elapsed().as_micros();
        let after = memory::counts(memory::BACKEND);
        memory::domain(memory::NONE);
        let trace = world.resource::<Trace>().0.lock().unwrap();
        assert_eq!(trace.active, 0);
        assert_eq!(trace.order.len(), 4);
        let mut seen = 0;
        for task in &trace.order {
            assert_eq!(seen & (1 << task), 0);
            seen |= 1 << task;
            let mut expected = Stats::default();
            let replay = [
                movement::reference,
                explosion::reference,
                regenerate::reference,
                lights::reference,
            ][*task];
            for row in &mut reference {
                if row.live {
                    replay(row, &mut expected);
                }
            }
            expected.check(&trace.results[*task]);
        }
        for group in &removed {
            for row in &mut reference[group * 100..(group + 1) * 100] {
                row.live = false;
            }
        }
        for group in first_added..entities.len() / 100 {
            reference.extend_from_slice(&defaults[usize::from(group >= first_enemy)]);
        }
        validate(&world, &reference, &entities);
        let order: String = trace
            .order
            .iter()
            .map(|v| char::from(b'0' + *v as u8))
            .collect();
        print!(
            "Bevy,{workers},{frame},{},{elapsed_us},{mutation_us},,,{order},{:.9}",
            (live[0].len() + live[1].len()) * 100,
            trace.results[3].sum
        );
        if cfg!(feature = "memory") {
            // Bevy scheduler/allocator does not expose a reliable per-allocation
            // world/job domain. Store column is combined world+scheduler here.
            let requests = |c: memory::Counts| c.allocations + c.reallocations;
            print!(
                ",{},{},0,0,0,0,0,0,{},{},{}",
                requests(after) - requests(before),
                after.bytes as i64 - before.bytes as i64,
                after.peak_bytes - before.bytes,
                requests(update) - requests(before),
                requests(after) - requests(update)
            );
        }
        println!(
            ",,,,,{},{},{},{},{}",
            wave.content * 100,
            wave.content * 100,
            wave.enemy_add * 100,
            wave.enemy_remove * 100,
            live[0].len() * COUNTS[3] + live[1].len() * 100
        );
        drop(trace);
        memory::domain(memory::BACKEND);
    }
    send.send(None).unwrap();
    coordinator.join().unwrap();
    for group in live.into_iter().flatten() {
        for entity in &entities[group * 100..(group + 1) * 100] {
            assert!(world.despawn(*entity));
        }
    }
    assert_eq!(world.query::<&Position>().iter(&world).count(), 0);
    drop(world);
    memory::domain(memory::NONE);
}
fn main() {
    let args: Vec<String> = std::env::args().collect();
    let population = args.get(1).map(|s| s.parse().unwrap()).unwrap_or(1_000_000);
    let frames = args.get(2).map(|s| s.parse().unwrap()).unwrap_or(32);
    let workers = args.get(3).map(|s| s.parse().unwrap()).unwrap_or(4);
    assert!(
        population >= 1000
            && population <= 10_000_000
            && population % 100 == 0
            && frames > 0
            && frames <= 1000
    );
    assert!([1, 2, 4, 8].contains(&workers));
    #[cfg(data_tsan)]
    println!("# sanitizer=thread; validation only; do not compare these timings");
    #[cfg(not(data_tsan))]
    println!(
        "# sanitizer=none; frame includes scheduling, content preparation and structural edits; validation excluded"
    );
    println!(
        "# tasks: 0=movement,1=explosion,2=regenerate,3=nearby_lights; Bevy automatic conflicts; par_iter batches of 4096; row work stays on the configured pool; separate schedule coordinator"
    );
    if cfg!(feature = "memory") {
        println!(
            "# memory=Rust global allocator; combined world+scheduler in store columns; excludes libc/pthread malloc, OS thread stacks, allocator headers and TSAN runtime; timings not comparable"
        );
    }
    print!(
        "backend,workers,frame,live_rows,frame_us,mutation_us,busy_attempts,access_wait_us,admission_order,light_sum"
    );
    if cfg!(feature = "memory") {
        print!(
            ",store_requests,store_delta_bytes,job_requests,job_delta_bytes,caller_requests,caller_delta_bytes,resource_requests,resource_delta_bytes,peak_additional_bytes,update_store_requests,stream_store_requests"
        );
    }
    println!(
        ",movement_wait_us,explosion_wait_us,regenerate_wait_us,lights_wait_us,content_spawned,content_despawned,enemies_spawned,enemies_despawned,live_enemies"
    );
    run(population, frames, workers);
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::{Barrier, Mutex};

    #[derive(Resource)]
    struct Rendezvous {
        barrier: Barrier,
        threads: Mutex<Vec<thread::ThreadId>>,
    }
    fn move_position(mut query: Query<&mut Position>, gate: Res<Rendezvous>) {
        gate.threads.lock().unwrap().push(thread::current().id());
        gate.barrier.wait();
        for mut position in &mut query {
            position.0[0] += 1.0;
        }
    }
    fn heal(mut query: Query<&mut Health>, gate: Res<Rendezvous>) {
        gate.threads.lock().unwrap().push(thread::current().id());
        gate.barrier.wait();
        for mut health in &mut query {
            health.0 += 1.0;
        }
    }
    #[test]
    fn independent_fields_run_on_distinct_threads() {
        assert!(cfg!(data_tsan), "threaded tests must run with TSAN");
        ComputeTaskPool::get_or_init(|| {
            TaskPoolBuilder::new()
                .num_threads(4)
                .thread_name("bevy-benchmark".into())
                .build()
        });
        let mut world = World::new();
        let entity = world.spawn((Position::default(), Health(1.0))).id();
        world.insert_resource(Rendezvous {
            barrier: Barrier::new(2),
            threads: Mutex::new(Vec::new()),
        });
        let mut schedule = Schedule::default();
        schedule.set_executor(MultiThreadedExecutor::new());
        schedule.add_systems((move_position, heal));
        schedule.run(&mut world);
        let threads = world.resource::<Rendezvous>().threads.lock().unwrap();
        assert_eq!(threads.len(), 2);
        assert_ne!(threads[0], threads[1]);
        assert_eq!(world.get::<Position>(entity).unwrap().0[0], 1.0);
        assert_eq!(world.get::<Health>(entity).unwrap().0, 2.0);
    }
    #[test]
    fn conflicting_updates_and_streaming_match_sequential_replay() {
        assert!(cfg!(data_tsan), "threaded tests must run with TSAN");
        run(1000, 20, 4);
    }
}
