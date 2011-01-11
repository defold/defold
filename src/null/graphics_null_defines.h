#ifndef DMGRAPHICS_NULL_DEFINES_H
#define DMGRAPHICS_NULL_DEFINES_H

// Primitive types
#define DMGRAPHICS_PRIMITIVE_POINTS                         (1)
#define DMGRAPHICS_PRIMITIVE_LINES                          (2)
#define DMGRAPHICS_PRIMITIVE_LINE_LOOP                      (3)
#define DMGRAPHICS_PRIMITIVE_LINE_STRIP                     (4)
#define DMGRAPHICS_PRIMITIVE_TRIANGLES                      (5)
#define DMGRAPHICS_PRIMITIVE_TRIANGLE_STRIP                 (6)
#define DMGRAPHICS_PRIMITIVE_TRIANGLE_FAN                   (7)
#define DMGRAPHICS_PRIMITIVE_QUADS                          (8)
#define DMGRAPHICS_PRIMITIVE_QUAD_STRIP                     (9)

// Buffer type flags
#define DMGRAPHICS_BUFFER_TYPE_COLOR_BIT                    (1)
#define DMGRAPHICS_BUFFER_TYPE_DEPTH_BIT                    (2)
#define DMGRAPHICS_BUFFER_TYPE_STENCIL_BIT                  (4)

// Render states
#define DMGRAPHICS_STATE_DEPTH_TEST                         (13)
#define DMGRAPHICS_STATE_ALPHA_TEST                         (14)
#define DMGRAPHICS_STATE_BLEND                              (15)
#define DMGRAPHICS_STATE_CULL_FACE                          (16)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_FILL                (17)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_LINE                (18)
#define DMGRAPHICS_STATE_POLYGON_OFFSET_POINT               (19)

// Types
#define DMGRAPHICS_TYPE_BYTE                                (20)
#define DMGRAPHICS_TYPE_UNSIGNED_BYTE                       (21)
#define DMGRAPHICS_TYPE_SHORT                               (22)
#define DMGRAPHICS_TYPE_UNSIGNED_SHORT                      (23)
#define DMGRAPHICS_TYPE_INT                                 (24)
#define DMGRAPHICS_TYPE_UNSIGNED_INT                        (25)
#define DMGRAPHICS_TYPE_FLOAT                               (26)

// Texture format
#define DMGRAPHICS_TEXTURE_FORMAT_ALPHA                     (27)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA                      (28)
#define DMGRAPHICS_TEXTURE_FORMAT_RGB_DXT1                  (29)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT1                 (30)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT3                 (31)
#define DMGRAPHICS_TEXTURE_FORMAT_RGBA_DXT5                 (32)

// Texture filter
#define DMGRAPHICS_TEXTURE_FILTER_LINEAR                    (33)
#define DMGRAPHICS_TEXTURE_FILTER_NEAREST                   (34)

// Texture wrap
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP                       (35)
#define DMGRAPHICS_TEXTURE_WRAP_REPEAT                      (36)
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_BORDER             (37)
#define DMGRAPHICS_TEXTURE_WRAP_CLAMP_TO_EDGE               (38)
#define DMGRAPHICS_TEXTURE_WRAP_MIRRORED_REPEAT             (39)

// Blend factors
#define DMGRAPHICS_BLEND_FACTOR_ZERO                        (40)
#define DMGRAPHICS_BLEND_FACTOR_ONE                         (41)
#define DMGRAPHICS_BLEND_FACTOR_SRC_COLOR                   (42)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_SRC_COLOR         (43)
#define DMGRAPHICS_BLEND_FACTOR_DST_COLOR                   (44)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_DST_COLOR         (45)
#define DMGRAPHICS_BLEND_FACTOR_SRC_ALPHA                   (46)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA         (47)
#define DMGRAPHICS_BLEND_FACTOR_DST_ALPHA                   (48)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_DST_ALPHA         (49)
#define DMGRAPHICS_BLEND_FACTOR_SRC_ALPHA_SATURATE          (50)

#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_COLOR              (51)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR    (52)
#define DMGRAPHICS_BLEND_FACTOR_CONSTANT_ALPHA              (53)
#define DMGRAPHICS_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA    (54)

#define DMGRAPHICS_BUFFER_USAGE_STREAM_DRAW                 (55)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_READ                 (56)
#define DMGRAPHICS_BUFFER_USAGE_STREAM_COPY                 (57)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_DRAW                (58)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_READ                (59)
#define DMGRAPHICS_BUFFER_USAGE_DYNAMIC_COPY                (60)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_DRAW                 (61)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_READ                 (62)
#define DMGRAPHICS_BUFFER_USAGE_STATIC_COPY                 (63)
#define DMGRAPHICS_BUFFER_ACCESS_READ_ONLY                  (64)
#define DMGRAPHICS_BUFFER_ACCESS_WRITE_ONLY                 (65)
#define DMGRAPHICS_BUFFER_ACCESS_READ_WRITE                 (66)

#define DMGRAPHICS_WINDOW_STATE_OPENED                      (67)
#define DMGRAPHICS_WINDOW_STATE_ACTIVE                      (68)
#define DMGRAPHICS_WINDOW_STATE_ICONIFIED                   (69)
#define DMGRAPHICS_WINDOW_STATE_ACCELERATED                 (70)
#define DMGRAPHICS_WINDOW_STATE_RED_BITS                    (71)
#define DMGRAPHICS_WINDOW_STATE_GREEN_BITS                  (72)
#define DMGRAPHICS_WINDOW_STATE_BLUE_BITS                   (73)
#define DMGRAPHICS_WINDOW_STATE_ALPHA_BITS                  (74)
#define DMGRAPHICS_WINDOW_STATE_DEPTH_BITS                  (75)
#define DMGRAPHICS_WINDOW_STATE_STENCIL_BITS                (76)
#define DMGRAPHICS_WINDOW_STATE_REFRESH_RATE                (77)
#define DMGRAPHICS_WINDOW_STATE_ACCUM_RED_BITS              (78)
#define DMGRAPHICS_WINDOW_STATE_ACCUM_GREEN_BITS            (79)
#define DMGRAPHICS_WINDOW_STATE_ACCUM_BLUE_BITS             (80)
#define DMGRAPHICS_WINDOW_STATE_ACCUM_ALPHA_BITS            (81)
#define DMGRAPHICS_WINDOW_STATE_AUX_BUFFERS                 (82)
#define DMGRAPHICS_WINDOW_STATE_STEREO                      (83)
#define DMGRAPHICS_WINDOW_STATE_WINDOW_NO_RESIZE            (84)
#define DMGRAPHICS_WINDOW_STATE_FSAA_SAMPLES                (85)

#define DMGRAPHICS_FACE_TYPE_FRONT                          (86)
#define DMGRAPHICS_FACE_TYPE_BACK                           (87)
#define DMGRAPHICS_FACE_TYPE_FRONT_AND_BACK                 (88)

#endif // DMGRAPHICS_NULL_DEFINES_H
