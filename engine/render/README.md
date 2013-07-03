Rendering
=========

Textures
--------

Currently supported texture formats:

* **L**: 8-bit luminosity
* **RGB**: 24-bit RGB
* **RGBA**: 32-bit RGBA

All runtime textures have pre-multiplied alpha.

Blending
--------

Blending assumes premultiplied alpha. Blend modes:

* **Alpha**: src_color + dst_color * (1-src_alpha)
* **Add**: src_color + dst_color
* **Multiply**: src_color * dst_color + dst_color * (1-src_alpha)

The blend modes are available for 2D components:

* Gui
* Sprite
* Tile grid
* ParticleFX