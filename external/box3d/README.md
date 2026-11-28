# Box3D

The include and src directories and MIT license in Box3D come from
[Erin Catto’s Box3D at 47d7f7cc](https://github.com/erincatto/box3d/tree/47d7f7cc7e091142c08d11dc7d2e493c5d34f536).

The local changes in `Box3D/src/convex_manifold.c` optimize the separating-axis test.
The convex hull collision routine skips redundant edge tests for right
prisms with a common extrusion axis. It also reuses support vertices for
identically oriented instances of the same hull and reads transformed edge data
directly during the general edge tests.

Configure Defold with `-DDEFOLD_ENABLE_FEATURES=box3d` to build the experimental
3D backend and its `b3d` Lua module. Select `physics.type = Box3D` in the game
project. Set `box3d.sub_step_count` to the number of solver substeps per engine physics
update. The default is two. The engine owns the collection world and the bodies
created from collision components. The CMake feature selects the 3D implementation
at build time. For native-extension builds, build the engine and its external
libraries with the same feature, then select `script_box3d`, `box3d`, and
`ScriptBox3DExt` in the application's Extender manifest.

The backend supports boxes, spheres, capsules, convex hulls and static triangle meshes.
Triangle meshes use an identity pose within the collision object. The object itself can
be positioned and rotated. Debug drawing shows the shape bounds.

Collision events report the start of contact. Contact point events report impacts
from Box3D's hit event buffer. Triggers report entries and exits. Native extensions
can read the borrowed world ID during the existing post-step broadcast.
`b3d.world.set_component_events(world, false)` disables component event delivery
for that collection while preserving its native event buffers.

For WebAssembly, enable both CMake features with
`-DDEFOLD_ENABLE_FEATURES=box3d,simd`. The WebAssembly SDK ships
`libbox3d.a` with four-lane SIMD and `libbox3d_nosimd.a` for hosts
without SIMD. Select the matching library in the app manifest.

`box3d.worker_count` sets the number of background workers per physics world.
The default is zero. With one background worker the calling thread and its
helper run the solver together. For a threaded WebAssembly build, use the
`wasm_pthread-web` target and preallocate workers for the active worlds.

The Defold contact adapters in `defold/` compile against the pinned Box3D
sources. Each worker can reuse identical cold hull queries during a step.
Call `b3d.world.prepare_sleeping_contacts(world)` after spawning a sleeping
assembly to spread contact preparation across physics ticks. A matching pose
can reuse the complete contact geometry. Prisms with matching relative
orientation can also reuse supporting vertices after a change in translation.
The simulation computes impulses and emits events when the bodies wake.
