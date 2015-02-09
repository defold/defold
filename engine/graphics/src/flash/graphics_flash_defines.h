#ifndef DMGRAPHICS_FLASH_DEFINES_H
#define DMGRAPHICS_FLASH_DEFINES_H

#include <AS3/AS3.h>
#include <Flash++.h>
#include <AS3/AVM2.h>
#include <AS3/AS3++.h>
using namespace AS3::ui;

// Primitive types
#define DMGRAPHICS_PRIMITIVE_POINTS                         (0)
#define DMGRAPHICS_PRIMITIVE_LINES                          (1)
#define DMGRAPHICS_PRIMITIVE_LINE_LOOP                      (2)
#define DMGRAPHICS_PRIMITIVE_LINE_STRIP                     (3)
#define DMGRAPHICS_PRIMITIVE_TRIANGLES                      (4)
#define DMGRAPHICS_PRIMITIVE_TRIANGLE_STRIP                 (5)
#define DMGRAPHICS_PRIMITIVE_TRIANGLE_FAN                   (6)

// Buffer type flags
#define DMGRAPHICS_BUFFER_TYPE_COLOR_BIT                    (0)
#define DMGRAPHICS_BUFFER_TYPE_DEPTH_BIT                    (1)
#define DMGRAPHICS_BUFFER_TYPE_STENCIL_BIT                  (2)

// Render states
#define DMGRAPHICS_STATE_DEPTH_TEST                         (0)
#define DMGRAPHICS_STATE_SCISSOR_TEST                       (1)
#define DMGRAPHICS_STATE_STENCIL_TEST                       (2)
#define DMGRAPHICS_STATE_ALPHA_TEST                         (3)
#define DMGRAPHICS_STATE_BLEND                              (4)
#define DMGRAPHICS_STATE_CULL_FACE                          (5)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_FILL                (6)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_LINE                (7)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_POINT               (8)

// Types
#define DMGRAPHICS_TYPE_BYTE                                (0)
#define DMGRAPHICS_TYPE_UNSIGNED_BYTE                       (1)
#define DMGRAPHICS_TYPE_SHORT                               (2)
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT                      (3)
#define DMGRAPHICS_TYPE_INT                                 (4)
#define DMGRAPHICS_TYPE_UNSIGNED_INT                        (5)
#define DMGRAPHICS_TYPE_FLOAT                               (6)
#define DMGRAPHICS_TYPE_FLOAT_VEC4                          (7)
#define DMGRAPHICS_TYPE_FLOAT_MAT4                          (8)
#define DMGRAPHICS_TYPE_SAMPLER_2D                          (9)

#define DMGRAPHICS_TEXTURE_FORMAT_LUMINANCE                 (0)
#define DMGRAPHICS_TEXTURE_FORMAT_RGB                       (1)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA                      (2)
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_DXT1                  0x83F0
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT1                 0x83F1
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT3                 0x83F2
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT5                 0x83F3
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_2BPPV1          0x8C01
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_PVRTC_4BPPV1          0x8C00
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1         0x8C03
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1         0x8C02
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_ETC1                  0x8D64

// Texture filter
#define DMGRAPHICS_TEXTURE_FILTER_LINEAR                    (0)
#define DMGRAPHICS_TEXTURE_FILTER_NEAREST                   (1)
#define DMGRAPHICS_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST    (2)
#define DMGRAPHICS_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST     (3)

// Texture wrap
#define DMGRAPHICS_TEXTURE_WRAP_REPEAT                      (0)
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_BORDER             (1)
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE               (2)
#define DMGRAPHICS_TEXTURE_WRAP_MIRRORED_REPEAT             (3)

// Blend factors
#define DMGRAPHICS_BLEND_FACTOR_ZERO                        (0)
#define DMGRAPHICS_BLEND_FACTOR_ONE                         (1)
#define DMGRAPHICS_BLEND_FACTOR_SRC_COLOR                   (2)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_SRC_COLOR         (3)
#define DMGRAPHICS_BLEND_FACTOR_DST_COLOR                   (4)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_DST_COLOR         (5)
#define DMGRAPHICS_BLEND_FACTOR_SRC_ALPHA                   (6)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA         (7)
#define DMGRAPHICS_BLEND_FACTOR_DST_ALPHA                   (8)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_DST_ALPHA         (9)
#define DMGRAPHICS_BLEND_FACTOR_SRC_ALPHA_SATURATE          (10)

#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_COLOR              (0x8001)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR    (0x8002)
#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_ALPHA              (0x8003)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA    (0x8004)

#define DMGRAPHICS_BUFFER_USAGE_STREAM_DRAW                 (0x88E0)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_READ                 (0x88E1)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_COPY                 (0x88E2)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_DRAW                (0x88E8)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_READ                (0x88E9)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_COPY                (0x88EA)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_DRAW                 (0x88E4)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_READ                 (0x88E5)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_COPY                 (0x88E6)
#define DMGRAPHICS_BUFFER_ACCESS_READ_ONLY                  (0x88B8)
#define DMGRAPHICS_BUFFER_ACCESS_WRITE_ONLY                 (0x88B9)
#define DMGRAPHICS_BUFFER_ACCESS_READ_WRITE                 (0x88BA)

#define DMGRAPHICS_WINDOW_STATE_OPENED                      0x00020001
#define DMGRAPHICS_WINDOW_STATE_ACTIVE                      0x00020002
#define DMGRAPHICS_WINDOW_STATE_ICONIFIED                   0x00020003
#define DMGRAPHICS_WINDOW_STATE_ACCELERATED                 0x00020004
#define DMGRAPHICS_WINDOW_STATE_RED_BITS                    0x00020005
#define DMGRAPHICS_WINDOW_STATE_GREEN_BITS                  0x00020006
#define DMGRAPHICS_WINDOW_STATE_BLUE_BITS                   0x00020007
#define DMGRAPHICS_WINDOW_STATE_ALPHA_BITS                  0x00020008
#define DMGRAPHICS_WINDOW_STATE_DEPTH_BITS                  0x00020009
#define DMGRAPHICS_WINDOW_STATE_STENCIL_BITS                0x0002000A
#define DMGRAPHICS_WINDOW_STATE_REFRESH_RATE                0x0002000B
#define DMGRAPHICS_WINDOW_STATE_ACCUM_RED_BITS              0x0002000C
#define DMGRAPHICS_WINDOW_STATE_ACCUM_GREEN_BITS            0x0002000D
#define DMGRAPHICS_WINDOW_STATE_ACCUM_BLUE_BITS             0x0002000E
#define DMGRAPHICS_WINDOW_STATE_ACCUM_ALPHA_BITS            0x0002000F
#define DMGRAPHICS_WINDOW_STATE_AUX_BUFFERS                 0x00020010
#define DMGRAPHICS_WINDOW_STATE_STEREO                      0x00020011
#define DMGRAPHICS_WINDOW_STATE_WINDOW_NO_RESIZE            0x00020012
#define DMGRAPHICS_WINDOW_STATE_FSAA_SAMPLES                0x00020013

#define DMGRAPHICS_FACE_TYPE_FRONT                          (0)
#define DMGRAPHICS_FACE_TYPE_BACK                           (1)
#define DMGRAPHICS_FACE_TYPE_FRONT_AND_BACK                 (2)

#endif // DMGRAPHICS_FLASH_DEFINES_H
