# Atlas format

This document describes the atlas format and its conventions

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

The runtime sprite geometry is defined by the struct `SpriteGeometry` in [texture_set_ddf.proto](../../gamesys/proto/gamesys/texture_set_ddf.proto)
The vertices are in local UV space [-0.5, 0.5] where the origin (0,0) is at the image center.
The vertices are also not rotated, as they're used the final vertex calculation.
They're also used at runtime to calculate the actual UV coordinates in the texture.

## Deprecated

The editor currently uses the precalculated UV coordinates of the output file format.
Since the engine doesn't need the data, we intend to remove it from the file format, and the editor will have to render the sprites the same way as the engine does.
