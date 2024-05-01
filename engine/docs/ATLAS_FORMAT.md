# Atlas format

This document describes the atlas format and its conventions

## The TextureSetLayout.SourceImage struct

This is our new public interface for extensions, and holds the sprite image geometry and the final (trimmed and rotated) rectangle in the atlas.
The `indices` is a list of of indices, where each group of 3 define a CCW triangle.
The `vertices` list is a list of integers, where each group of 2 define a point.
The points are in source image space. [(0,0) .. (image width, image height)]


## The TextureSetLayout.Rect struct

While originally a much more simple rectangle structure, it now is holding data for a single sprite image and its geometry.


## Sprite Geometry

The runtime sprite geometry is defined by the struct `SpriteGeometry` in [texture_set_ddf.proto](../../gamesys/proto/gamesys/texture_set_ddf.proto)
The

## Deprecated

The editor currently uses the precalculated UV coordinates of the output file format.
Since the engine doesn't need the data, we intend to remove it from the file format, and the editor will have to render the sprites the same way as the engine does.
