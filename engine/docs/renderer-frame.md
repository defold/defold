
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
