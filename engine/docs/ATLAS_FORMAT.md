# Atlas format

This document describes the atlas format and its conventions

## Atlas modes

An atlas contains either raster images or SVG images, including images referenced only by animations. Mixing the two is a build error. Empty atlases use raster mode. The compiled `TextureSet.mode` stores the inferred mode; an absent field defaults to raster for compatibility with existing resources.

SVG atlases have no packed bitmap or `.texturec` output. Their `TextureSet.vector` payload contains three little-endian numeric buffers: RGBA16F curves, R32UI bands, and RGBA32F paints. Each buffer contains complete rows of 4096 texels, with a maximum of 256 rows. The atlas resource owns the corresponding GPU textures. These textures use nearest filtering and clamp-to-edge wrapping, without compression, mipmaps, or color conversion. `TextureSetResource.m_Texture` is null for vector atlases.

Each vector image records its floating-point viewport width and height and the index of its paint header. Image names, frame indices, animations and geometry pivots retain their normal atlas conventions. The rectangles in a vector atlas form a deterministic editor contact sheet; sprites do not sample that layout.

Vector sprites use `/builtins/materials/sprite-vector.material`, one atlas binding, and four vertices with six indices. With manual size and Slice 9 enabled, the existing 4×4 grid uses sixteen vertices and fifty-four indices. Slice borders are measured in resolved SVG viewport pixels, and all cells share the atlas paint and textures; resizing preserves corners while scaling the sprite scales everything. Compatible custom materials declare `curve_texture`, `band_texture` and `paint_texture` samplers; the band texture uses `usampler2D`. The engine supplies all three textures from the atlas. The `texcoord0` attribute contains SVG viewport coordinates with Y increasing downwards, and `page_index` contains the paint-header index. Bob and the editor compile parsed geometry through dlib's `SlugCompiler` bridge. The native encoder in `engine/dlib/src/slug` also supplies vector fonts; SVG compilation has no font dependency. The built-in shaders share Slug coverage and paint decoding with vector fonts, while SVG paints interpolate and composite in sRGB. The material keeps the normal `tile` render tag.

Vector atlases bypass raster packing and texture profiles. Trimming is unsupported. GUI pie nodes, tilemaps, particles and the raster atlas manipulation APIs reject vector atlases. Atlas/material compatibility is checked during builds, loading and overrides. Hot reload can replace vector content, but changing an atlas between raster and vector mode requires restarting the scene.

## Vector GUI boxes

GUI box nodes use `/builtins/materials/gui-vector.material` (or a compatible material with the same sampler contract) and one named SVG atlas resource. A GUI scene can contain both raster and vector atlas resources; each box selects a matching material. The normal `gui` render tag, node transforms, size modes, pivots, tint, inherited opacity, flipbook animation and stencil clipping are retained. Vector GUI boxes use six vertices, or fifty-four vertices for nine-slice, following the GUI component's existing triangle-list batching. Slice borders are measured in SVG viewport pixels. The atlas owns all three textures, including hot reload; no extra GUI or Lua texture bindings are exposed. SVG textures on pie nodes are rejected in this version.

## SVG animation

SVG animations retain one geometry image per SVG and compact authored keyframe tracks. Curves and band lists are compiled once. Immutable track records, timing, interpolation, transforms and layer-property bindings are appended to the paint texture. `VectorImage.duration` and `loop` describe the embedded timeline. `TextureSetAnimation.duration` and `frame_durations` retain clip timing for implicit image animations and explicit atlas groups, including groups with clips of different lengths. Ordinary raster flipbook timing is unchanged.

Each sprite or GUI node supplies its own time in seconds through the `svg_time` vertex attribute, derived from its existing cursor and playback rate. Compatible animated custom materials consume that scalar attribute and the shared Slug animation/paint code. GUI time uses a separate vertex stream; the existing raster/custom-node vertex layout is unchanged. The shaders evaluate affine transforms, solid fill/stop colors, fill/stop opacity, gradient coordinates/transforms and discrete visibility/display against that time. There is no sampled-frame array, global clock, atlas texture mutation, or new Lua API. Named-layer visibility overrides remain deferred. CPU source previews use the same SVG parser at time zero; sprite and atlas animation previews advance their own continuous time.

The numeric layout remains little-endian and uses 4096-texel rows. The image paint header's Z component is zero for static content, or points to nine texels per layer for animated SVGs. Those records reference shared transform chains, property bindings, gradient data, the SVG viewport and each path’s canonical normalization. A property binding contains a base value and an ordered list of track references. Each track has three header texels (key count/interpolation/type, timing, key pointer); each authored key uses three texels for its value, normalized key time and cubic easing parameters. Finite clips retain begin/repeat/fill timing; indefinite tracks use their common period. Limits are 256 tracks per SVG, 256 keys per track, and the existing numeric-texture capacity. Clip duration does not multiply geometry or texture allocation.

## The TextureSetLayout.SourceImage struct

This is our new public interface for extensions, and holds the sprite image geometry and the final (trimmed and rotated) rectangle in the atlas.
The `indices` member is a list of of indices, where each group of 3 define a CCW triangle.
The `vertices` member is a list of Point's
The points are in source image space: [(0,0) .. (image width, image height)] (i.e. not rotated)


## The TextureSetLayout.Rect struct

While originally a much more simple rectangle structure, it now is holding data for a single sprite image and its geometry.
It also holds the page index, and if the image is rotated (90 degrees CW) in the texture.

## Texture

The texture holds all packed images, and may span over several "pages". Each packed Rect has an index into which page it is located.

Rotated images are stored in a 90 degrees CW rotation.

## Runtime Sprite Geometry

The runtime sprite geometry is defined by the struct `SpriteGeometry` in [texture_set_ddf.proto](../gamesys/proto/gamesys/texture_set_ddf.proto)
The vertices are in local UV space [-0.5, 0.5] where the origin (0,0) is at the image center.
The vertices are also not rotated, as they're used the final vertex calculation.
They're also used at runtime to calculate the actual UV coordinates in the texture.

## Deprecated

The editor currently uses the precalculated UV coordinates of the output file format.
Since the engine doesn't need the data, we intend to remove it from the file format, and the editor will have to render the sprites the same way as the engine does.
