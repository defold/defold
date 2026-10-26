These tiny fixtures were generated for Defold; they contain no third-party artwork.

`etc1s.ktx2` and `uastc.ktx2` use the bundled Basis Universal encoder, quality 128,
linear color, with an 8x4 opaque image: `(r,g,b) = (x*31,y*63,120)`. Their authored
4x2 mip is uniformly `(20,200,40)`, intentionally different from a generated mip.
`uastc-zstd.ktx2` and `uastc-zlib.ktx2` contain the same blocks, independently
compressed per level with the named lossless wrapper.
`bc7.ktx2` contains the base image transcoded from UASTC to BC7 by the bundled
transcoder. `bc7-zlib.ktx2` wraps those same BC7 blocks in Zlib. BC7 fixtures have
one mip. Raw, orientation, alpha, channel, and malformed fixtures are constructed
in Ktx2TextureGeneratorTest and test_texc_ktx2.cpp.
