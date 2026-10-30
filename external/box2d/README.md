# Box2D v3

Vendored from [Box2D v3.2.0](https://github.com/erincatto/box2d/releases/tag/v3.2.0).
The original source archive is `box2d-3.2.0.tar.gz`.

SHA-256: `ffec747b27a2267016f8d0240d658c82d187fb2271fa0f69fcbe53fbc067180d`.

## Updating the sources

Run from `external/box2d`:

```sh
curl -L --fail https://codeload.github.com/erincatto/box2d/tar.gz/refs/tags/v3.2.0 -o box2d-3.2.0.tar.gz
tar -xzf box2d-3.2.0.tar.gz
rm -rf Box2D
mkdir Box2D
cp -R box2d-3.2.0/include box2d-3.2.0/src Box2D/
cp box2d-3.2.0/LICENSE Box2D/
(cd Box2D && patch --binary -p1 < ../patch_3.2.0)
```

The patch preserves Defold's kinematic contacts, 16-vertex polygons, world lock
query, total force query, and the `posix_memalign` fallback for older iOS targets.
Kinematic-only contacts use the scalar overflow solver because upstream's graph
colors require a dynamic body.

Build both `box2d` and `box2d_nosimd` with the external CMake build and install
the updated headers and archives before rebuilding the engine.

## Lua changes in 3.2

The `physics` API keeps its existing joint definitions. The native `b2d` bindings
follow the changed Box2D APIs:

- Joint definitions use `local_frame_a` and `local_frame_b` tables containing
  `position` and `angle`. Use `get_local_frame_a/b` and `set_local_frame_a/b` to
  read or update them. The first frame's x-axis defines the prismatic or wheel
  axis; vertical suspension needs an angle of `math.pi / 2`.
- Mouse joints are removed. Use a motor joint with local frames, spring hertz,
  damping ratios, and maximum spring force/torque for position control.
- Motor joints use `linear_velocity`, `angular_velocity`, `max_velocity_force`,
  `max_velocity_torque`, `linear_hertz`, `angular_hertz`,
  `linear_damping_ratio`, `angular_damping_ratio`, `max_spring_force`, and
  `max_spring_torque`. Matching getters and setters replace the offset,
  correction factor, and maximum force/torque calls. Motor limits default to
  zero, so a default motor joint applies no force.
- `b2d.world.set_joint_tuning` is replaced by
  `b2d.joint.set_constraint_tuning`. `b2d.world.enable_speculative` is removed.
- World profiling uses `solver_setup`, `constraints`, and `restitution` in
  place of `prepare_stages`, `solve_constraints`, and `apply_restitution`;
  `merge_islands` is removed. Constraint color counts contain 24 entries.
- Body names are truncated to 10 characters. Joint type constants use the
  current Box2D enumeration; stored numeric joint types must be updated.

Box2D v2 bindings retain their existing APIs.
