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

use bevy_ecs::{component::Mutable, prelude::*};

pub const SEED: u32 = 0x12345678;
pub const PERCENT: [usize; 6] = [10, 15, 1, 24, 25, 25];
pub const TYPE_IDS: [u64; 6] = [
    5814404822828456249,
    14500896599220142080,
    11577225589693920664,
    10406201373173866896,
    9480789839722286861,
    7266391380641106441,
];

pub type Vector3 = [f32; 3];

#[derive(Component, Clone, Copy, Default)]
#[repr(C)]
pub struct Light {
    pub color: Vector3,
    pub intensity: f64,
}

#[derive(Component, Clone, Copy)]
#[repr(C)]
pub struct SpotLight {
    pub position: Vector3,
    pub light: Light,
    pub range: f64,
    pub inner_cone_angle: f64,
    pub outer_cone_angle: f64,
}

#[derive(Component, Clone, Copy)]
#[repr(C)]
pub struct PointLight {
    pub light: Light,
    pub position: Vector3,
    pub range: f64,
}

#[derive(Component, Clone, Copy)]
#[repr(C)]
pub struct Player {
    pub position: Vector3,
    pub health: f64,
    pub velocity: Vector3,
}

#[derive(Component, Clone, Copy)]
#[repr(C)]
pub struct Enemy {
    pub health: f64,
    pub velocity: Vector3,
    pub position: Vector3,
}

#[derive(Component, Clone, Copy)]
#[repr(C)]
pub struct Pickup {
    pub position: Vector3,
    pub amount: f64,
}

#[derive(Component, Clone, Copy)]
#[repr(C)]
pub struct Breakable {
    pub health: f64,
    pub position: Vector3,
}

pub trait Row: Component<Mutability = Mutable> + Copy {
    fn from_input(v: &Input) -> Self;
    fn position(&self) -> Vector3;
    fn position_mut(&mut self) -> &mut Vector3;
    fn scalar(&self) -> f64;
    fn scalar_mut(&mut self) -> &mut f64;
}

macro_rules! row {
    ($name:ty, $scalar:ident, $v:ident, $init:expr) => {
        impl Row for $name {
            fn from_input($v: &Input) -> Self {
                $init
            }
            fn position(&self) -> Vector3 {
                self.position
            }
            fn position_mut(&mut self) -> &mut Vector3 {
                &mut self.position
            }
            fn scalar(&self) -> f64 {
                self.$scalar
            }
            fn scalar_mut(&mut self) -> &mut f64 {
                &mut self.$scalar
            }
        }
    };
}
row!(
    SpotLight,
    range,
    v,
    Self {
        position: v.position,
        light: v.light,
        range: 10.0,
        inner_cone_angle: 0.0,
        outer_cone_angle: 45.0
    }
);
row!(
    PointLight,
    range,
    v,
    Self {
        light: v.light,
        position: v.position,
        range: 10.0
    }
);
row!(
    Player,
    health,
    v,
    Self {
        position: v.position,
        health: v.health,
        velocity: [1.0, 0.0, -1.0]
    }
);
row!(
    Enemy,
    health,
    v,
    Self {
        health: v.health,
        velocity: [1.0, 0.0, -1.0],
        position: v.position
    }
);
row!(
    Pickup,
    amount,
    v,
    Self {
        position: v.position,
        amount: 10.0
    }
);
row!(
    Breakable,
    health,
    v,
    Self {
        health: v.health,
        position: v.position
    }
);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct Position(pub Vector3);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct Velocity(pub Vector3);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct Health(pub f64);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct Range(pub f64);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct InnerAngle(pub f64);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct OuterAngle(pub f64);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct Amount(pub f64);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct Owner(pub u64);

#[derive(Component, Clone, Copy)]
#[repr(transparent)]
pub struct ComponentId(pub u64);

#[derive(Component, Clone, Copy)]
pub struct ActorTag;

#[derive(Component, Clone, Copy)]
pub struct BreakableTag;

#[derive(Component, Clone, Copy)]
pub struct DamageableTag;

#[derive(Component, Clone, Copy)]
pub struct EnemyTag;

#[derive(Component, Clone, Copy)]
pub struct LightTag;

#[derive(Component, Clone, Copy)]
pub struct PickupTag;

#[derive(Component, Clone, Copy)]
pub struct PlayerTag;

#[derive(Component, Clone, Copy)]
pub struct PointTag;

#[derive(Component, Clone, Copy)]
pub struct SpotTag;

pub struct Rows {
    pub spot: Vec<(SpotLight, Owner, ComponentId, LightTag, SpotTag)>,
    pub point: Vec<(PointLight, Owner, ComponentId, LightTag, PointTag)>,
    pub player: Vec<(
        Player,
        Owner,
        ComponentId,
        ActorTag,
        PlayerTag,
        DamageableTag,
    )>,
    pub enemy: Vec<(Enemy, Owner, ComponentId, ActorTag, EnemyTag, DamageableTag)>,
    pub pickup: Vec<(Pickup, Owner, ComponentId, PickupTag)>,
    pub breakable: Vec<(Breakable, Owner, ComponentId, BreakableTag, DamageableTag)>,
}

impl Rows {
    fn new(values: &[Vec<Input>; 6]) -> Self {
        Self {
            spot: values[0]
                .iter()
                .map(|v| {
                    (
                        SpotLight::from_input(v),
                        Owner(v.owner),
                        ComponentId(v.component),
                        LightTag,
                        SpotTag,
                    )
                })
                .collect(),
            point: values[1]
                .iter()
                .map(|v| {
                    (
                        PointLight::from_input(v),
                        Owner(v.owner),
                        ComponentId(v.component),
                        LightTag,
                        PointTag,
                    )
                })
                .collect(),
            player: values[2]
                .iter()
                .map(|v| {
                    (
                        Player::from_input(v),
                        Owner(v.owner),
                        ComponentId(v.component),
                        ActorTag,
                        PlayerTag,
                        DamageableTag,
                    )
                })
                .collect(),
            enemy: values[3]
                .iter()
                .map(|v| {
                    (
                        Enemy::from_input(v),
                        Owner(v.owner),
                        ComponentId(v.component),
                        ActorTag,
                        EnemyTag,
                        DamageableTag,
                    )
                })
                .collect(),
            pickup: values[4]
                .iter()
                .map(|v| {
                    (
                        Pickup::from_input(v),
                        Owner(v.owner),
                        ComponentId(v.component),
                        PickupTag,
                    )
                })
                .collect(),
            breakable: values[5]
                .iter()
                .map(|v| {
                    (
                        Breakable::from_input(v),
                        Owner(v.owner),
                        ComponentId(v.component),
                        BreakableTag,
                        DamageableTag,
                    )
                })
                .collect(),
        }
    }
}

pub struct Columns {
    pub spot: Vec<(
        Position,
        Light,
        Range,
        InnerAngle,
        OuterAngle,
        Owner,
        ComponentId,
        LightTag,
        SpotTag,
    )>,
    pub point: Vec<(
        Light,
        Position,
        Range,
        Owner,
        ComponentId,
        LightTag,
        PointTag,
    )>,
    pub player: Vec<(
        Position,
        Health,
        Velocity,
        Owner,
        ComponentId,
        ActorTag,
        PlayerTag,
        DamageableTag,
    )>,
    pub enemy: Vec<(
        Health,
        Velocity,
        Position,
        Owner,
        ComponentId,
        ActorTag,
        EnemyTag,
        DamageableTag,
    )>,
    pub pickup: Vec<(Position, Amount, Owner, ComponentId, PickupTag)>,
    pub breakable: Vec<(
        Health,
        Position,
        Owner,
        ComponentId,
        BreakableTag,
        DamageableTag,
    )>,
}

impl Columns {
    fn new(values: &[Vec<Input>; 6]) -> Self {
        Self {
            spot: values[0]
                .iter()
                .map(|v| {
                    (
                        Position(v.position),
                        v.light,
                        Range(10.0),
                        InnerAngle(0.0),
                        OuterAngle(45.0),
                        Owner(v.owner),
                        ComponentId(v.component),
                        LightTag,
                        SpotTag,
                    )
                })
                .collect(),
            point: values[1]
                .iter()
                .map(|v| {
                    (
                        v.light,
                        Position(v.position),
                        Range(10.0),
                        Owner(v.owner),
                        ComponentId(v.component),
                        LightTag,
                        PointTag,
                    )
                })
                .collect(),
            player: values[2]
                .iter()
                .map(|v| {
                    (
                        Position(v.position),
                        Health(v.health),
                        Velocity([1.0, 0.0, -1.0]),
                        Owner(v.owner),
                        ComponentId(v.component),
                        ActorTag,
                        PlayerTag,
                        DamageableTag,
                    )
                })
                .collect(),
            enemy: values[3]
                .iter()
                .map(|v| {
                    (
                        Health(v.health),
                        Velocity([1.0, 0.0, -1.0]),
                        Position(v.position),
                        Owner(v.owner),
                        ComponentId(v.component),
                        ActorTag,
                        EnemyTag,
                        DamageableTag,
                    )
                })
                .collect(),
            pickup: values[4]
                .iter()
                .map(|v| {
                    (
                        Position(v.position),
                        Amount(10.0),
                        Owner(v.owner),
                        ComponentId(v.component),
                        PickupTag,
                    )
                })
                .collect(),
            breakable: values[5]
                .iter()
                .map(|v| {
                    (
                        Health(v.health),
                        Position(v.position),
                        Owner(v.owner),
                        ComponentId(v.component),
                        BreakableTag,
                        DamageableTag,
                    )
                })
                .collect(),
        }
    }
}

#[derive(Clone, Copy)]
pub struct Input {
    pub position: Vector3,
    pub light: Light,
    pub health: f64,
    pub owner: u64,
    pub component: u64,
}

#[derive(Clone, Copy)]
pub struct Key {
    pub kind: usize,
    pub row: usize,
}

pub struct Fixture {
    pub count: usize,
    pub total: usize,
    pub group: usize,
    pub counts: [usize; 6],
    pub offsets: [usize; 6],
    pub values: [Vec<Input>; 6],
    pub order: Vec<Key>,
    pub rows: Rows,
    pub columns: Columns,
}

fn random(state: &mut u32) -> u32 {
    *state = state.wrapping_mul(1664525).wrapping_add(1013904223);
    *state
}

impl Fixture {
    pub fn new(count: usize, group: usize) -> Self {
        let counts = PERCENT.map(|p| count / 100 * p);
        let mut offsets = [0; 6];
        let mut total = 0;
        let mut state = SEED;
        let mut order = Vec::with_capacity(count);
        let values = std::array::from_fn(|kind| {
            assert!(group == 0 || counts[kind] % group == 0);
            offsets[kind] = total;
            let size = counts[kind] + counts[kind] / 10;
            total += size;
            (0..size)
                .map(|row| {
                    if group != 0 {
                        state = SEED + kind as u32 * 65537 + (row % group) as u32;
                    }
                    let position =
                        std::array::from_fn(|_| ((random(&mut state) >> 25) as i32 - 64) as f32);
                    let color = std::array::from_fn(|_| (random(&mut state) >> 24) as f32 / 256.0);
                    let health = (100 + (random(&mut state) >> 16) % 101) as f64;
                    if row < counts[kind] {
                        order.push(Key { kind, row });
                    }
                    Input {
                        position,
                        light: Light {
                            color,
                            intensity: 1.0,
                        },
                        health,
                        owner: (offsets[kind]
                            + if group == 0 { row } else { row / group * group }
                            + 1) as u64,
                        component: if group == 0 {
                            TYPE_IDS[kind]
                        } else {
                            ((kind + 1) * 100 + row % group) as u64
                        },
                    }
                })
                .collect()
        });
        for i in (1..count).rev() {
            let j = random(&mut state) as usize % (i + 1);
            order.swap(i, j);
        }
        Self {
            count,
            total,
            group,
            counts,
            offsets,
            rows: Rows::new(&values),
            columns: Columns::new(&values),
            values,
            order,
        }
    }
}

pub fn sum_vector(v: Vector3) -> f64 {
    v[0] as f64 + v[1] as f64 + v[2] as f64
}
pub fn hit(p: Vector3, radius: i32) -> bool {
    radius >= 0 && p[0] * p[0] + p[1] * p[1] + p[2] * p[2] <= (radius * radius) as f32
}
pub fn damaged(health: f64, writes: u32) -> f64 {
    (health - 25.0 * writes as f64).max(0.0)
}
pub fn has_health(kind: usize) -> bool {
    matches!(kind, 2 | 3 | 5)
}
pub fn scalar(v: &Input, kind: usize) -> f64 {
    if has_health(kind) { v.health } else { 10.0 }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn native_layouts_match_cpp_fixture() {
        // repr(C) preserves the field orders/padding used by benchmark_data_mixed.cpp.
        assert_eq!(std::mem::size_of::<Vector3>(), 12);
        assert_eq!(std::mem::size_of::<Light>(), 24);
        assert_eq!(std::mem::size_of::<SpotLight>(), 64);
        assert_eq!(std::mem::size_of::<PointLight>(), 48);
        assert_eq!(std::mem::size_of::<Player>(), 40);
        assert_eq!(std::mem::size_of::<Enemy>(), 32);
        assert_eq!(std::mem::size_of::<Pickup>(), 24);
        assert_eq!(std::mem::size_of::<Breakable>(), 24);
        assert_eq!(std::mem::size_of::<Owner>(), 8);
        assert_eq!(std::mem::size_of::<ComponentId>(), 8);
    }
}
