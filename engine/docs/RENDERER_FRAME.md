
# The renderer frame

This document describes the steps the renderer takes to produce a draw call.

## Render script

When the user calls `render.draw(pred, {frustum = frustum, constants = constants})` in the [render script](https://defold.com/manuals/render/?q=default.render_script#render) it may generate one or more draw calls.

Also note that each frame, the render script will most often call `render.draw()` multiple times in order to produce multiple effects.

## Render list entries

In the renderer, a render list entry represents a single renderable "thing" (E.g. a single sprite). It holds meta data about the thing and its owner (the component type).

Each frame, each component type (e.g. Sprite, TileMap, Mesh) registers all its enabled render list entries to the renderer.

## Pruning

### Material tags

The predicate in the `render.draw(pred)` call tells the renderer which material tags to select. If a render list entry doesn't has the specified material tags, the entry is skipped.

### Frustum culling

If a frustum is being passed to the draw function (`render.draw(pred, {frustum = frustum})`), each render list entry will be checked if it's intersecting the frustum.
Each component type will calculate this visibility based, allowing each type to choose bounding volume representations (sphere, OBB etc).

### GPU model culling

Set `render.gpu_culling = 1` in `game.project` to enable GPU frustum culling and indexed indirect drawing for eligible instanced models. It is disabled by default. Metal, Vulkan and WebGPU support this path when the device provides compute shaders, storage buffers and indirect drawing.

Eligible models use indexed geometry, local vertex space and an instance layout containing world and/or normal matrices. Skinned models, morph targets and dynamic vertex attributes retain CPU culling. The renderer also uses the CPU path for material overrides, blending, stencil tests, disabled depth testing or depth writes, and depth comparisons other than less/less-equal/greater/greater-equal. These passes require instance order, which GPU compaction does not preserve. Storage-buffer allocation failures or device limits fall back to CPU culling and conventional instancing.

GPU batches retain bounds and world matrices between frames and upload changed values. Compute compacts visible instances, and indirect drawing consumes their count without CPU readback. Compatible shaders fetch world matrices through compacted instance IDs; other supported layouts use compacted matrices.

Set `render.gpu_culling_stats = 1` to collect optional asynchronous visibility statistics in the profiler. This requires GPU culling and is disabled by default. Counts can describe an earlier completed frame; rendering does not wait for or depend on them.

## Batching

Once any pruning is done based on material tags and/or frustum intersection, the remaining entries' are updated with a camera view Z value and is put into its batch key.

The [batch key](https://realtimecollisiondetection.net/blog/?p=86) holds info about which material is used, the component type, the Z value and so on.
When the batch key is updated, the list is sorted into one or more batches.

Each batch is sent to the corresponding component type's render dispatch function.
The batching call has 3 stages:

* `RENDER_LIST_OPERATION_BEGIN` - Called once per collection. Allows for resetting internal data
* `RENDER_LIST_OPERATION_BATCH` - Called once per batch.
* `RENDER_LIST_OPERATION_END` - Called once per collection.


## Render Object

For each render batch call, the component type takes the list of render list entries and converts them into a single `RenderObject`.
It holds info about render constants, material, vertex/index buffer etc.

The render object is then registered with the renderer. The render object represents an actual draw call.

## Render constants

Each `RenderObject` may have a set of render constants assigned to it.
Usually they come from the material that is used.

The render script has the ability to override the constants in the `render.draw(pred, {constants = constants })` call.
