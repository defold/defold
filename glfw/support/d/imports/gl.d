/************************************************************************
 *
 * OpenGL 1.0 - 1.4 module for the D programming language
 * (http://www.digitalmars.com/d/)
 *
 * Conversion by Marcus Geelnard
 *
 * Originally based on gl.h from the Mesa 4.0 distribution by Brian Paul,
 * and glext.h from the SGI Open Source Sample Implementation.
 *
 * This file contains constants for OpenGL 1.0 - 1.4, aswell as for over
 * 200 different OpenGL extensions. Function declarations exist for
 * OpenGL 1.0 - 1.1, and function pointer type definitions exist for
 * OpenGL 1.2 - 1.4 and all the OpenGL extensions.
 *
 ************************************************************************/


version (Win32) {
    extern (Windows):
}
version (linux) {
    extern (C):
}



/************************************************************************
 *
 * OpenGL versions supported by this module (1.0 is assumed, and requires
 * no further introduction)
 *
 ************************************************************************/

const uint GL_VERSION_1_1 = 1;
const uint GL_VERSION_1_2 = 1;
const uint GL_VERSION_1_3 = 1;
const uint GL_VERSION_1_4 = 1;



/************************************************************************
 *
 * Datatypes
 *
 ************************************************************************/

alias uint   GLenum;
alias ubyte  GLboolean;
alias uint   GLbitfield;
alias void   GLvoid;
alias byte   GLbyte;         /* 1-byte signed */
alias short  GLshort;        /* 2-byte signed */
alias int    GLint;          /* 4-byte signed */
alias ubyte  GLubyte;        /* 1-byte unsigned */
alias ushort GLushort;       /* 2-byte unsigned */
alias uint   GLuint;         /* 4-byte unsigned */
alias int    GLsizei;        /* 4-byte signed */
alias float  GLfloat;        /* single precision float */
alias float  GLclampf;       /* single precision float in [0,1] */
alias double GLdouble;       /* double precision float */
alias double GLclampd;       /* double precision float in [0,1] */



/************************************************************************
 *
 * Constants
 *
 ************************************************************************/

/* Boolean values */
const uint GL_FALSE                               = 0x0;
const uint GL_TRUE                                = 0x1;

/* Data types */
const uint GL_BYTE                                = 0x1400;
const uint GL_UNSIGNED_BYTE                       = 0x1401;
const uint GL_SHORT                               = 0x1402;
const uint GL_UNSIGNED_SHORT                      = 0x1403;
const uint GL_INT                                 = 0x1404;
const uint GL_UNSIGNED_INT                        = 0x1405;
const uint GL_FLOAT                               = 0x1406;
const uint GL_DOUBLE                              = 0x140A;
const uint GL_2_BYTES                             = 0x1407;
const uint GL_3_BYTES                             = 0x1408;
const uint GL_4_BYTES                             = 0x1409;

/* Primitives */
const uint GL_POINTS                              = 0x0000;
const uint GL_LINES                               = 0x0001;
const uint GL_LINE_LOOP                           = 0x0002;
const uint GL_LINE_STRIP                          = 0x0003;
const uint GL_TRIANGLES                           = 0x0004;
const uint GL_TRIANGLE_STRIP                      = 0x0005;
const uint GL_TRIANGLE_FAN                        = 0x0006;
const uint GL_QUADS                               = 0x0007;
const uint GL_QUAD_STRIP                          = 0x0008;
const uint GL_POLYGON                             = 0x0009;

/* Vertex Arrays */
const uint GL_VERTEX_ARRAY                        = 0x8074;
const uint GL_NORMAL_ARRAY                        = 0x8075;
const uint GL_COLOR_ARRAY                         = 0x8076;
const uint GL_INDEX_ARRAY                         = 0x8077;
const uint GL_TEXTURE_COORD_ARRAY                 = 0x8078;
const uint GL_EDGE_FLAG_ARRAY                     = 0x8079;
const uint GL_VERTEX_ARRAY_SIZE                   = 0x807A;
const uint GL_VERTEX_ARRAY_TYPE                   = 0x807B;
const uint GL_VERTEX_ARRAY_STRIDE                 = 0x807C;
const uint GL_NORMAL_ARRAY_TYPE                   = 0x807E;
const uint GL_NORMAL_ARRAY_STRIDE                 = 0x807F;
const uint GL_COLOR_ARRAY_SIZE                    = 0x8081;
const uint GL_COLOR_ARRAY_TYPE                    = 0x8082;
const uint GL_COLOR_ARRAY_STRIDE                  = 0x8083;
const uint GL_INDEX_ARRAY_TYPE                    = 0x8085;
const uint GL_INDEX_ARRAY_STRIDE                  = 0x8086;
const uint GL_TEXTURE_COORD_ARRAY_SIZE            = 0x8088;
const uint GL_TEXTURE_COORD_ARRAY_TYPE            = 0x8089;
const uint GL_TEXTURE_COORD_ARRAY_STRIDE          = 0x808A;
const uint GL_EDGE_FLAG_ARRAY_STRIDE              = 0x808C;
const uint GL_VERTEX_ARRAY_POINTER                = 0x808E;
const uint GL_NORMAL_ARRAY_POINTER                = 0x808F;
const uint GL_COLOR_ARRAY_POINTER                 = 0x8090;
const uint GL_INDEX_ARRAY_POINTER                 = 0x8091;
const uint GL_TEXTURE_COORD_ARRAY_POINTER         = 0x8092;
const uint GL_EDGE_FLAG_ARRAY_POINTER             = 0x8093;
const uint GL_V2F                                 = 0x2A20;
const uint GL_V3F                                 = 0x2A21;
const uint GL_C4UB_V2F                            = 0x2A22;
const uint GL_C4UB_V3F                            = 0x2A23;
const uint GL_C3F_V3F                             = 0x2A24;
const uint GL_N3F_V3F                             = 0x2A25;
const uint GL_C4F_N3F_V3F                         = 0x2A26;
const uint GL_T2F_V3F                             = 0x2A27;
const uint GL_T4F_V4F                             = 0x2A28;
const uint GL_T2F_C4UB_V3F                        = 0x2A29;
const uint GL_T2F_C3F_V3F                         = 0x2A2A;
const uint GL_T2F_N3F_V3F                         = 0x2A2B;
const uint GL_T2F_C4F_N3F_V3F                     = 0x2A2C;
const uint GL_T4F_C4F_N3F_V4F                     = 0x2A2D;

/* Matrix Mode */
const uint GL_MATRIX_MODE                         = 0x0BA0;
const uint GL_MODELVIEW                           = 0x1700;
const uint GL_PROJECTION                          = 0x1701;
const uint GL_TEXTURE                             = 0x1702;

/* Points */
const uint GL_POINT_SMOOTH                        = 0x0B10;
const uint GL_POINT_SIZE                          = 0x0B11;
const uint GL_POINT_SIZE_GRANULARITY              = 0x0B13;
const uint GL_POINT_SIZE_RANGE                    = 0x0B12;

/* Lines */
const uint GL_LINE_SMOOTH                         = 0x0B20;
const uint GL_LINE_STIPPLE                        = 0x0B24;
const uint GL_LINE_STIPPLE_PATTERN                = 0x0B25;
const uint GL_LINE_STIPPLE_REPEAT                 = 0x0B26;
const uint GL_LINE_WIDTH                          = 0x0B21;
const uint GL_LINE_WIDTH_GRANULARITY              = 0x0B23;
const uint GL_LINE_WIDTH_RANGE                    = 0x0B22;

/* Polygons */
const uint GL_POINT                               = 0x1B00;
const uint GL_LINE                                = 0x1B01;
const uint GL_FILL                                = 0x1B02;
const uint GL_CW                                  = 0x0900;
const uint GL_CCW                                 = 0x0901;
const uint GL_FRONT                               = 0x0404;
const uint GL_BACK                                = 0x0405;
const uint GL_POLYGON_MODE                        = 0x0B40;
const uint GL_POLYGON_SMOOTH                      = 0x0B41;
const uint GL_POLYGON_STIPPLE                     = 0x0B42;
const uint GL_EDGE_FLAG                           = 0x0B43;
const uint GL_CULL_FACE                           = 0x0B44;
const uint GL_CULL_FACE_MODE                      = 0x0B45;
const uint GL_FRONT_FACE                          = 0x0B46;
const uint GL_POLYGON_OFFSET_FACTOR               = 0x8038;
const uint GL_POLYGON_OFFSET_UNITS                = 0x2A00;
const uint GL_POLYGON_OFFSET_POINT                = 0x2A01;
const uint GL_POLYGON_OFFSET_LINE                 = 0x2A02;
const uint GL_POLYGON_OFFSET_FILL                 = 0x8037;

/* Display Lists */
const uint GL_COMPILE                             = 0x1300;
const uint GL_COMPILE_AND_EXECUTE                 = 0x1301;
const uint GL_LIST_BASE                           = 0x0B32;
const uint GL_LIST_INDEX                          = 0x0B33;
const uint GL_LIST_MODE                           = 0x0B30;

/* Depth buffer */
const uint GL_NEVER                               = 0x0200;
const uint GL_LESS                                = 0x0201;
const uint GL_EQUAL                               = 0x0202;
const uint GL_LEQUAL                              = 0x0203;
const uint GL_GREATER                             = 0x0204;
const uint GL_NOTEQUAL                            = 0x0205;
const uint GL_GEQUAL                              = 0x0206;
const uint GL_ALWAYS                              = 0x0207;
const uint GL_DEPTH_TEST                          = 0x0B71;
const uint GL_DEPTH_BITS                          = 0x0D56;
const uint GL_DEPTH_CLEAR_VALUE                   = 0x0B73;
const uint GL_DEPTH_FUNC                          = 0x0B74;
const uint GL_DEPTH_RANGE                         = 0x0B70;
const uint GL_DEPTH_WRITEMASK                     = 0x0B72;
const uint GL_DEPTH_COMPONENT                     = 0x1902;

/* Lighting */
const uint GL_LIGHTING                            = 0x0B50;
const uint GL_LIGHT0                              = 0x4000;
const uint GL_LIGHT1                              = 0x4001;
const uint GL_LIGHT2                              = 0x4002;
const uint GL_LIGHT3                              = 0x4003;
const uint GL_LIGHT4                              = 0x4004;
const uint GL_LIGHT5                              = 0x4005;
const uint GL_LIGHT6                              = 0x4006;
const uint GL_LIGHT7                              = 0x4007;
const uint GL_SPOT_EXPONENT                       = 0x1205;
const uint GL_SPOT_CUTOFF                         = 0x1206;
const uint GL_CONSTANT_ATTENUATION                = 0x1207;
const uint GL_LINEAR_ATTENUATION                  = 0x1208;
const uint GL_QUADRATIC_ATTENUATION               = 0x1209;
const uint GL_AMBIENT                             = 0x1200;
const uint GL_DIFFUSE                             = 0x1201;
const uint GL_SPECULAR                            = 0x1202;
const uint GL_SHININESS                           = 0x1601;
const uint GL_EMISSION                            = 0x1600;
const uint GL_POSITION                            = 0x1203;
const uint GL_SPOT_DIRECTION                      = 0x1204;
const uint GL_AMBIENT_AND_DIFFUSE                 = 0x1602;
const uint GL_COLOR_INDEXES                       = 0x1603;
const uint GL_LIGHT_MODEL_TWO_SIDE                = 0x0B52;
const uint GL_LIGHT_MODEL_LOCAL_VIEWER            = 0x0B51;
const uint GL_LIGHT_MODEL_AMBIENT                 = 0x0B53;
const uint GL_FRONT_AND_BACK                      = 0x0408;
const uint GL_SHADE_MODEL                         = 0x0B54;
const uint GL_FLAT                                = 0x1D00;
const uint GL_SMOOTH                              = 0x1D01;
const uint GL_COLOR_MATERIAL                      = 0x0B57;
const uint GL_COLOR_MATERIAL_FACE                 = 0x0B55;
const uint GL_COLOR_MATERIAL_PARAMETER            = 0x0B56;
const uint GL_NORMALIZE                           = 0x0BA1;

/* User clipping planes */
const uint GL_CLIP_PLANE0                         = 0x3000;
const uint GL_CLIP_PLANE1                         = 0x3001;
const uint GL_CLIP_PLANE2                         = 0x3002;
const uint GL_CLIP_PLANE3                         = 0x3003;
const uint GL_CLIP_PLANE4                         = 0x3004;
const uint GL_CLIP_PLANE5                         = 0x3005;

/* Accumulation buffer */
const uint GL_ACCUM_RED_BITS                      = 0x0D58;
const uint GL_ACCUM_GREEN_BITS                    = 0x0D59;
const uint GL_ACCUM_BLUE_BITS                     = 0x0D5A;
const uint GL_ACCUM_ALPHA_BITS                    = 0x0D5B;
const uint GL_ACCUM_CLEAR_VALUE                   = 0x0B80;
const uint GL_ACCUM                               = 0x0100;
const uint GL_ADD                                 = 0x0104;
const uint GL_LOAD                                = 0x0101;
const uint GL_MULT                                = 0x0103;
const uint GL_RETURN                              = 0x0102;

/* Alpha testing */
const uint GL_ALPHA_TEST                          = 0x0BC0;
const uint GL_ALPHA_TEST_REF                      = 0x0BC2;
const uint GL_ALPHA_TEST_FUNC                     = 0x0BC1;

/* Blending */
const uint GL_BLEND                               = 0x0BE2;
const uint GL_BLEND_SRC                           = 0x0BE1;
const uint GL_BLEND_DST                           = 0x0BE0;
const uint GL_ZERO                                = 0x0;
const uint GL_ONE                                 = 0x1;
const uint GL_SRC_COLOR                           = 0x0300;
const uint GL_ONE_MINUS_SRC_COLOR                 = 0x0301;
const uint GL_SRC_ALPHA                           = 0x0302;
const uint GL_ONE_MINUS_SRC_ALPHA                 = 0x0303;
const uint GL_DST_ALPHA                           = 0x0304;
const uint GL_ONE_MINUS_DST_ALPHA                 = 0x0305;
const uint GL_DST_COLOR                           = 0x0306;
const uint GL_ONE_MINUS_DST_COLOR                 = 0x0307;
const uint GL_SRC_ALPHA_SATURATE                  = 0x0308;
/*
const uint GL_CONSTANT_COLOR                      = 0x8001;
const uint GL_ONE_MINUS_CONSTANT_COLOR            = 0x8002;
const uint GL_CONSTANT_ALPHA                      = 0x8003;
const uint GL_ONE_MINUS_CONSTANT_ALPHA            = 0x8004;
*/

/* Render Mode */
const uint GL_FEEDBACK                            = 0x1C01;
const uint GL_RENDER                              = 0x1C00;
const uint GL_SELECT                              = 0x1C02;

/* Feedback */
const uint GL_2D                                  = 0x0600;
const uint GL_3D                                  = 0x0601;
const uint GL_3D_COLOR                            = 0x0602;
const uint GL_3D_COLOR_TEXTURE                    = 0x0603;
const uint GL_4D_COLOR_TEXTURE                    = 0x0604;
const uint GL_POINT_TOKEN                         = 0x0701;
const uint GL_LINE_TOKEN                          = 0x0702;
const uint GL_LINE_RESET_TOKEN                    = 0x0707;
const uint GL_POLYGON_TOKEN                       = 0x0703;
const uint GL_BITMAP_TOKEN                        = 0x0704;
const uint GL_DRAW_PIXEL_TOKEN                    = 0x0705;
const uint GL_COPY_PIXEL_TOKEN                    = 0x0706;
const uint GL_PASS_THROUGH_TOKEN                  = 0x0700;
const uint GL_FEEDBACK_BUFFER_POINTER             = 0x0DF0;
const uint GL_FEEDBACK_BUFFER_SIZE                = 0x0DF1;
const uint GL_FEEDBACK_BUFFER_TYPE                = 0x0DF2;

/* Selection */
const uint GL_SELECTION_BUFFER_POINTER            = 0x0DF3;
const uint GL_SELECTION_BUFFER_SIZE               = 0x0DF4;

/* Fog */
const uint GL_FOG                                 = 0x0B60;
const uint GL_FOG_MODE                            = 0x0B65;
const uint GL_FOG_DENSITY                         = 0x0B62;
const uint GL_FOG_COLOR                           = 0x0B66;
const uint GL_FOG_INDEX                           = 0x0B61;
const uint GL_FOG_START                           = 0x0B63;
const uint GL_FOG_END                             = 0x0B64;
const uint GL_LINEAR                              = 0x2601;
const uint GL_EXP                                 = 0x0800;
const uint GL_EXP2                                = 0x0801;

/* Logic Ops */
const uint GL_LOGIC_OP                            = 0x0BF1;
const uint GL_INDEX_LOGIC_OP                      = 0x0BF1;
const uint GL_COLOR_LOGIC_OP                      = 0x0BF2;
const uint GL_LOGIC_OP_MODE                       = 0x0BF0;
const uint GL_CLEAR                               = 0x1500;
const uint GL_SET                                 = 0x150F;
const uint GL_COPY                                = 0x1503;
const uint GL_COPY_INVERTED                       = 0x150C;
const uint GL_NOOP                                = 0x1505;
const uint GL_INVERT                              = 0x150A;
const uint GL_AND                                 = 0x1501;
const uint GL_NAND                                = 0x150E;
const uint GL_OR                                  = 0x1507;
const uint GL_NOR                                 = 0x1508;
const uint GL_XOR                                 = 0x1506;
const uint GL_EQUIV                               = 0x1509;
const uint GL_AND_REVERSE                         = 0x1502;
const uint GL_AND_INVERTED                        = 0x1504;
const uint GL_OR_REVERSE                          = 0x150B;
const uint GL_OR_INVERTED                         = 0x150D;

/* Stencil */
const uint GL_STENCIL_TEST                        = 0x0B90;
const uint GL_STENCIL_WRITEMASK                   = 0x0B98;
const uint GL_STENCIL_BITS                        = 0x0D57;
const uint GL_STENCIL_FUNC                        = 0x0B92;
const uint GL_STENCIL_VALUE_MASK                  = 0x0B93;
const uint GL_STENCIL_REF                         = 0x0B97;
const uint GL_STENCIL_FAIL                        = 0x0B94;
const uint GL_STENCIL_PASS_DEPTH_PASS             = 0x0B96;
const uint GL_STENCIL_PASS_DEPTH_FAIL             = 0x0B95;
const uint GL_STENCIL_CLEAR_VALUE                 = 0x0B91;
const uint GL_STENCIL_INDEX                       = 0x1901;
const uint GL_KEEP                                = 0x1E00;
const uint GL_REPLACE                             = 0x1E01;
const uint GL_INCR                                = 0x1E02;
const uint GL_DECR                                = 0x1E03;

/* Buffers, Pixel Drawing/Reading */
const uint GL_NONE                                = 0x0;
const uint GL_LEFT                                = 0x0406;
const uint GL_RIGHT                               = 0x0407;
/* GL_FRONT                                       = 0x0404; */
/* GL_BACK                                        = 0x0405; */
/* GL_FRONT_AND_BACK                              = 0x0408; */
const uint GL_FRONT_LEFT                          = 0x0400;
const uint GL_FRONT_RIGHT                         = 0x0401;
const uint GL_BACK_LEFT                           = 0x0402;
const uint GL_BACK_RIGHT                          = 0x0403;
const uint GL_AUX0                                = 0x0409;
const uint GL_AUX1                                = 0x040A;
const uint GL_AUX2                                = 0x040B;
const uint GL_AUX3                                = 0x040C;
const uint GL_COLOR_INDEX                         = 0x1900;
const uint GL_RED                                 = 0x1903;
const uint GL_GREEN                               = 0x1904;
const uint GL_BLUE                                = 0x1905;
const uint GL_ALPHA                               = 0x1906;
const uint GL_LUMINANCE                           = 0x1909;
const uint GL_LUMINANCE_ALPHA                     = 0x190A;
const uint GL_ALPHA_BITS                          = 0x0D55;
const uint GL_RED_BITS                            = 0x0D52;
const uint GL_GREEN_BITS                          = 0x0D53;
const uint GL_BLUE_BITS                           = 0x0D54;
const uint GL_INDEX_BITS                          = 0x0D51;
const uint GL_SUBPIXEL_BITS                       = 0x0D50;
const uint GL_AUX_BUFFERS                         = 0x0C00;
const uint GL_READ_BUFFER                         = 0x0C02;
const uint GL_DRAW_BUFFER                         = 0x0C01;
const uint GL_DOUBLEBUFFER                        = 0x0C32;
const uint GL_STEREO                              = 0x0C33;
const uint GL_BITMAP                              = 0x1A00;
const uint GL_COLOR                               = 0x1800;
const uint GL_DEPTH                               = 0x1801;
const uint GL_STENCIL                             = 0x1802;
const uint GL_DITHER                              = 0x0BD0;
const uint GL_RGB                                 = 0x1907;
const uint GL_RGBA                                = 0x1908;

/* Implementation limits */
const uint GL_MAX_LIST_NESTING                    = 0x0B31;
const uint GL_MAX_ATTRIB_STACK_DEPTH              = 0x0D35;
const uint GL_MAX_MODELVIEW_STACK_DEPTH           = 0x0D36;
const uint GL_MAX_NAME_STACK_DEPTH                = 0x0D37;
const uint GL_MAX_PROJECTION_STACK_DEPTH          = 0x0D38;
const uint GL_MAX_TEXTURE_STACK_DEPTH             = 0x0D39;
const uint GL_MAX_EVAL_ORDER                      = 0x0D30;
const uint GL_MAX_LIGHTS                          = 0x0D31;
const uint GL_MAX_CLIP_PLANES                     = 0x0D32;
const uint GL_MAX_TEXTURE_SIZE                    = 0x0D33;
const uint GL_MAX_PIXEL_MAP_TABLE                 = 0x0D34;
const uint GL_MAX_VIEWPORT_DIMS                   = 0x0D3A;
const uint GL_MAX_CLIENT_ATTRIB_STACK_DEPTH       = 0x0D3B;

/* Gets */
const uint GL_ATTRIB_STACK_DEPTH                  = 0x0BB0;
const uint GL_CLIENT_ATTRIB_STACK_DEPTH           = 0x0BB1;
const uint GL_COLOR_CLEAR_VALUE                   = 0x0C22;
const uint GL_COLOR_WRITEMASK                     = 0x0C23;
const uint GL_CURRENT_INDEX                       = 0x0B01;
const uint GL_CURRENT_COLOR                       = 0x0B00;
const uint GL_CURRENT_NORMAL                      = 0x0B02;
const uint GL_CURRENT_RASTER_COLOR                = 0x0B04;
const uint GL_CURRENT_RASTER_DISTANCE             = 0x0B09;
const uint GL_CURRENT_RASTER_INDEX                = 0x0B05;
const uint GL_CURRENT_RASTER_POSITION             = 0x0B07;
const uint GL_CURRENT_RASTER_TEXTURE_COORDS       = 0x0B06;
const uint GL_CURRENT_RASTER_POSITION_VALID       = 0x0B08;
const uint GL_CURRENT_TEXTURE_COORDS              = 0x0B03;
const uint GL_INDEX_CLEAR_VALUE                   = 0x0C20;
const uint GL_INDEX_MODE                          = 0x0C30;
const uint GL_INDEX_WRITEMASK                     = 0x0C21;
const uint GL_MODELVIEW_MATRIX                    = 0x0BA6;
const uint GL_MODELVIEW_STACK_DEPTH               = 0x0BA3;
const uint GL_NAME_STACK_DEPTH                    = 0x0D70;
const uint GL_PROJECTION_MATRIX                   = 0x0BA7;
const uint GL_PROJECTION_STACK_DEPTH              = 0x0BA4;
const uint GL_RENDER_MODE                         = 0x0C40;
const uint GL_RGBA_MODE                           = 0x0C31;
const uint GL_TEXTURE_MATRIX                      = 0x0BA8;
const uint GL_TEXTURE_STACK_DEPTH                 = 0x0BA5;
const uint GL_VIEWPORT                            = 0x0BA2;

/* Evaluators */
const uint GL_AUTO_NORMAL                         = 0x0D80;
const uint GL_MAP1_COLOR_4                        = 0x0D90;
const uint GL_MAP1_GRID_DOMAIN                    = 0x0DD0;
const uint GL_MAP1_GRID_SEGMENTS                  = 0x0DD1;
const uint GL_MAP1_INDEX                          = 0x0D91;
const uint GL_MAP1_NORMAL                         = 0x0D92;
const uint GL_MAP1_TEXTURE_COORD_1                = 0x0D93;
const uint GL_MAP1_TEXTURE_COORD_2                = 0x0D94;
const uint GL_MAP1_TEXTURE_COORD_3                = 0x0D95;
const uint GL_MAP1_TEXTURE_COORD_4                = 0x0D96;
const uint GL_MAP1_VERTEX_3                       = 0x0D97;
const uint GL_MAP1_VERTEX_4                       = 0x0D98;
const uint GL_MAP2_COLOR_4                        = 0x0DB0;
const uint GL_MAP2_GRID_DOMAIN                    = 0x0DD2;
const uint GL_MAP2_GRID_SEGMENTS                  = 0x0DD3;
const uint GL_MAP2_INDEX                          = 0x0DB1;
const uint GL_MAP2_NORMAL                         = 0x0DB2;
const uint GL_MAP2_TEXTURE_COORD_1                = 0x0DB3;
const uint GL_MAP2_TEXTURE_COORD_2                = 0x0DB4;
const uint GL_MAP2_TEXTURE_COORD_3                = 0x0DB5;
const uint GL_MAP2_TEXTURE_COORD_4                = 0x0DB6;
const uint GL_MAP2_VERTEX_3                       = 0x0DB7;
const uint GL_MAP2_VERTEX_4                       = 0x0DB8;
const uint GL_COEFF                               = 0x0A00;
const uint GL_DOMAIN                              = 0x0A02;
const uint GL_ORDER                               = 0x0A01;

/* Hints */
const uint GL_FOG_HINT                            = 0x0C54;
const uint GL_LINE_SMOOTH_HINT                    = 0x0C52;
const uint GL_PERSPECTIVE_CORRECTION_HINT         = 0x0C50;
const uint GL_POINT_SMOOTH_HINT                   = 0x0C51;
const uint GL_POLYGON_SMOOTH_HINT                 = 0x0C53;
const uint GL_DONT_CARE                           = 0x1100;
const uint GL_FASTEST                             = 0x1101;
const uint GL_NICEST                              = 0x1102;

/* Scissor box */
const uint GL_SCISSOR_TEST                        = 0x0C11;
const uint GL_SCISSOR_BOX                         = 0x0C10;

/* Pixel Mode / Transfer */
const uint GL_MAP_COLOR                           = 0x0D10;
const uint GL_MAP_STENCIL                         = 0x0D11;
const uint GL_INDEX_SHIFT                         = 0x0D12;
const uint GL_INDEX_OFFSET                        = 0x0D13;
const uint GL_RED_SCALE                           = 0x0D14;
const uint GL_RED_BIAS                            = 0x0D15;
const uint GL_GREEN_SCALE                         = 0x0D18;
const uint GL_GREEN_BIAS                          = 0x0D19;
const uint GL_BLUE_SCALE                          = 0x0D1A;
const uint GL_BLUE_BIAS                           = 0x0D1B;
const uint GL_ALPHA_SCALE                         = 0x0D1C;
const uint GL_ALPHA_BIAS                          = 0x0D1D;
const uint GL_DEPTH_SCALE                         = 0x0D1E;
const uint GL_DEPTH_BIAS                          = 0x0D1F;
const uint GL_PIXEL_MAP_S_TO_S_SIZE               = 0x0CB1;
const uint GL_PIXEL_MAP_I_TO_I_SIZE               = 0x0CB0;
const uint GL_PIXEL_MAP_I_TO_R_SIZE               = 0x0CB2;
const uint GL_PIXEL_MAP_I_TO_G_SIZE               = 0x0CB3;
const uint GL_PIXEL_MAP_I_TO_B_SIZE               = 0x0CB4;
const uint GL_PIXEL_MAP_I_TO_A_SIZE               = 0x0CB5;
const uint GL_PIXEL_MAP_R_TO_R_SIZE               = 0x0CB6;
const uint GL_PIXEL_MAP_G_TO_G_SIZE               = 0x0CB7;
const uint GL_PIXEL_MAP_B_TO_B_SIZE               = 0x0CB8;
const uint GL_PIXEL_MAP_A_TO_A_SIZE               = 0x0CB9;
const uint GL_PIXEL_MAP_S_TO_S                    = 0x0C71;
const uint GL_PIXEL_MAP_I_TO_I                    = 0x0C70;
const uint GL_PIXEL_MAP_I_TO_R                    = 0x0C72;
const uint GL_PIXEL_MAP_I_TO_G                    = 0x0C73;
const uint GL_PIXEL_MAP_I_TO_B                    = 0x0C74;
const uint GL_PIXEL_MAP_I_TO_A                    = 0x0C75;
const uint GL_PIXEL_MAP_R_TO_R                    = 0x0C76;
const uint GL_PIXEL_MAP_G_TO_G                    = 0x0C77;
const uint GL_PIXEL_MAP_B_TO_B                    = 0x0C78;
const uint GL_PIXEL_MAP_A_TO_A                    = 0x0C79;
const uint GL_PACK_ALIGNMENT                      = 0x0D05;
const uint GL_PACK_LSB_FIRST                      = 0x0D01;
const uint GL_PACK_ROW_LENGTH                     = 0x0D02;
const uint GL_PACK_SKIP_PIXELS                    = 0x0D04;
const uint GL_PACK_SKIP_ROWS                      = 0x0D03;
const uint GL_PACK_SWAP_BYTES                     = 0x0D00;
const uint GL_UNPACK_ALIGNMENT                    = 0x0CF5;
const uint GL_UNPACK_LSB_FIRST                    = 0x0CF1;
const uint GL_UNPACK_ROW_LENGTH                   = 0x0CF2;
const uint GL_UNPACK_SKIP_PIXELS                  = 0x0CF4;
const uint GL_UNPACK_SKIP_ROWS                    = 0x0CF3;
const uint GL_UNPACK_SWAP_BYTES                   = 0x0CF0;
const uint GL_ZOOM_X                              = 0x0D16;
const uint GL_ZOOM_Y                              = 0x0D17;

/* Texture mapping */
const uint GL_TEXTURE_ENV                         = 0x2300;
const uint GL_TEXTURE_ENV_MODE                    = 0x2200;
const uint GL_TEXTURE_1D                          = 0x0DE0;
const uint GL_TEXTURE_2D                          = 0x0DE1;
const uint GL_TEXTURE_WRAP_S                      = 0x2802;
const uint GL_TEXTURE_WRAP_T                      = 0x2803;
const uint GL_TEXTURE_MAG_FILTER                  = 0x2800;
const uint GL_TEXTURE_MIN_FILTER                  = 0x2801;
const uint GL_TEXTURE_ENV_COLOR                   = 0x2201;
const uint GL_TEXTURE_GEN_S                       = 0x0C60;
const uint GL_TEXTURE_GEN_T                       = 0x0C61;
const uint GL_TEXTURE_GEN_MODE                    = 0x2500;
const uint GL_TEXTURE_BORDER_COLOR                = 0x1004;
const uint GL_TEXTURE_WIDTH                       = 0x1000;
const uint GL_TEXTURE_HEIGHT                      = 0x1001;
const uint GL_TEXTURE_BORDER                      = 0x1005;
const uint GL_TEXTURE_COMPONENTS                  = 0x1003;
const uint GL_TEXTURE_RED_SIZE                    = 0x805C;
const uint GL_TEXTURE_GREEN_SIZE                  = 0x805D;
const uint GL_TEXTURE_BLUE_SIZE                   = 0x805E;
const uint GL_TEXTURE_ALPHA_SIZE                  = 0x805F;
const uint GL_TEXTURE_LUMINANCE_SIZE              = 0x8060;
const uint GL_TEXTURE_INTENSITY_SIZE              = 0x8061;
const uint GL_NEAREST_MIPMAP_NEAREST              = 0x2700;
const uint GL_NEAREST_MIPMAP_LINEAR               = 0x2702;
const uint GL_LINEAR_MIPMAP_NEAREST               = 0x2701;
const uint GL_LINEAR_MIPMAP_LINEAR                = 0x2703;
const uint GL_OBJECT_LINEAR                       = 0x2401;
const uint GL_OBJECT_PLANE                        = 0x2501;
const uint GL_EYE_LINEAR                          = 0x2400;
const uint GL_EYE_PLANE                           = 0x2502;
const uint GL_SPHERE_MAP                          = 0x2402;
const uint GL_DECAL                               = 0x2101;
const uint GL_MODULATE                            = 0x2100;
const uint GL_NEAREST                             = 0x2600;
const uint GL_REPEAT                              = 0x2901;
const uint GL_CLAMP                               = 0x2900;
const uint GL_S                                   = 0x2000;
const uint GL_T                                   = 0x2001;
const uint GL_R                                   = 0x2002;
const uint GL_Q                                   = 0x2003;
const uint GL_TEXTURE_GEN_R                       = 0x0C62;
const uint GL_TEXTURE_GEN_Q                       = 0x0C63;

/* Utility */
const uint GL_VENDOR                              = 0x1F00;
const uint GL_RENDERER                            = 0x1F01;
const uint GL_VERSION                             = 0x1F02;
const uint GL_EXTENSIONS                          = 0x1F03;

/* Errors */
const uint GL_NO_ERROR                            = 0x0;
const uint GL_INVALID_VALUE                       = 0x0501;
const uint GL_INVALID_ENUM                        = 0x0500;
const uint GL_INVALID_OPERATION                   = 0x0502;
const uint GL_STACK_OVERFLOW                      = 0x0503;
const uint GL_STACK_UNDERFLOW                     = 0x0504;
const uint GL_OUT_OF_MEMORY                       = 0x0505;

/* glPush/PopAttrib bits */
const uint GL_CURRENT_BIT                         = 0x00000001;
const uint GL_POINT_BIT                           = 0x00000002;
const uint GL_LINE_BIT                            = 0x00000004;
const uint GL_POLYGON_BIT                         = 0x00000008;
const uint GL_POLYGON_STIPPLE_BIT                 = 0x00000010;
const uint GL_PIXEL_MODE_BIT                      = 0x00000020;
const uint GL_LIGHTING_BIT                        = 0x00000040;
const uint GL_FOG_BIT                             = 0x00000080;
const uint GL_DEPTH_BUFFER_BIT                    = 0x00000100;
const uint GL_ACCUM_BUFFER_BIT                    = 0x00000200;
const uint GL_STENCIL_BUFFER_BIT                  = 0x00000400;
const uint GL_VIEWPORT_BIT                        = 0x00000800;
const uint GL_TRANSFORM_BIT                       = 0x00001000;
const uint GL_ENABLE_BIT                          = 0x00002000;
const uint GL_COLOR_BUFFER_BIT                    = 0x00004000;
const uint GL_HINT_BIT                            = 0x00008000;
const uint GL_EVAL_BIT                            = 0x00010000;
const uint GL_LIST_BIT                            = 0x00020000;
const uint GL_TEXTURE_BIT                         = 0x00040000;
const uint GL_SCISSOR_BIT                         = 0x00080000;
const uint GL_ALL_ATTRIB_BITS                     = 0x000FFFFF;


/* OpenGL 1.1 */
const uint GL_PROXY_TEXTURE_1D                    = 0x8063;
const uint GL_PROXY_TEXTURE_2D                    = 0x8064;
const uint GL_TEXTURE_PRIORITY                    = 0x8066;
const uint GL_TEXTURE_RESIDENT                    = 0x8067;
const uint GL_TEXTURE_BINDING_1D                  = 0x8068;
const uint GL_TEXTURE_BINDING_2D                  = 0x8069;
const uint GL_TEXTURE_INTERNAL_FORMAT             = 0x1003;
const uint GL_ALPHA4                              = 0x803B;
const uint GL_ALPHA8                              = 0x803C;
const uint GL_ALPHA12                             = 0x803D;
const uint GL_ALPHA16                             = 0x803E;
const uint GL_LUMINANCE4                          = 0x803F;
const uint GL_LUMINANCE8                          = 0x8040;
const uint GL_LUMINANCE12                         = 0x8041;
const uint GL_LUMINANCE16                         = 0x8042;
const uint GL_LUMINANCE4_ALPHA4                   = 0x8043;
const uint GL_LUMINANCE6_ALPHA2                   = 0x8044;
const uint GL_LUMINANCE8_ALPHA8                   = 0x8045;
const uint GL_LUMINANCE12_ALPHA4                  = 0x8046;
const uint GL_LUMINANCE12_ALPHA12                 = 0x8047;
const uint GL_LUMINANCE16_ALPHA16                 = 0x8048;
const uint GL_INTENSITY                           = 0x8049;
const uint GL_INTENSITY4                          = 0x804A;
const uint GL_INTENSITY8                          = 0x804B;
const uint GL_INTENSITY12                         = 0x804C;
const uint GL_INTENSITY16                         = 0x804D;
const uint GL_R3_G3_B2                            = 0x2A10;
const uint GL_RGB4                                = 0x804F;
const uint GL_RGB5                                = 0x8050;
const uint GL_RGB8                                = 0x8051;
const uint GL_RGB10                               = 0x8052;
const uint GL_RGB12                               = 0x8053;
const uint GL_RGB16                               = 0x8054;
const uint GL_RGBA2                               = 0x8055;
const uint GL_RGBA4                               = 0x8056;
const uint GL_RGB5_A1                             = 0x8057;
const uint GL_RGBA8                               = 0x8058;
const uint GL_RGB10_A2                            = 0x8059;
const uint GL_RGBA12                              = 0x805A;
const uint GL_RGBA16                              = 0x805B;
const uint GL_CLIENT_PIXEL_STORE_BIT              = 0x00000001;
const uint GL_CLIENT_VERTEX_ARRAY_BIT             = 0x00000002;
const uint GL_ALL_CLIENT_ATTRIB_BITS              = 0xFFFFFFFF;
const uint GL_CLIENT_ALL_ATTRIB_BITS              = 0xFFFFFFFF;


/************************************************************************
 * OpenGL 1.2 -> 1.4 constants
 * Note: Proper OpenGL extension/version checking must be performed to
 * use these under Windows.
 ************************************************************************/

/* OpenGL 1.2 */
const uint GL_RESCALE_NORMAL                      = 0x803A;
const uint GL_CLAMP_TO_EDGE                       = 0x812F;
const uint GL_MAX_ELEMENTS_VERTICES               = 0x80E8;
const uint GL_MAX_ELEMENTS_INDICES                = 0x80E9;
const uint GL_BGR                                 = 0x80E0;
const uint GL_BGRA                                = 0x80E1;
const uint GL_UNSIGNED_BYTE_3_3_2                 = 0x8032;
const uint GL_UNSIGNED_BYTE_2_3_3_REV             = 0x8362;
const uint GL_UNSIGNED_SHORT_5_6_5                = 0x8363;
const uint GL_UNSIGNED_SHORT_5_6_5_REV            = 0x8364;
const uint GL_UNSIGNED_SHORT_4_4_4_4              = 0x8033;
const uint GL_UNSIGNED_SHORT_4_4_4_4_REV          = 0x8365;
const uint GL_UNSIGNED_SHORT_5_5_5_1              = 0x8034;
const uint GL_UNSIGNED_SHORT_1_5_5_5_REV          = 0x8366;
const uint GL_UNSIGNED_INT_8_8_8_8                = 0x8035;
const uint GL_UNSIGNED_INT_8_8_8_8_REV            = 0x8367;
const uint GL_UNSIGNED_INT_10_10_10_2             = 0x8036;
const uint GL_UNSIGNED_INT_2_10_10_10_REV         = 0x8368;
const uint GL_LIGHT_MODEL_COLOR_CONTROL           = 0x81F8;
const uint GL_SINGLE_COLOR                        = 0x81F9;
const uint GL_SEPARATE_SPECULAR_COLOR             = 0x81FA;
const uint GL_TEXTURE_MIN_LOD                     = 0x813A;
const uint GL_TEXTURE_MAX_LOD                     = 0x813B;
const uint GL_TEXTURE_BASE_LEVEL                  = 0x813C;
const uint GL_TEXTURE_MAX_LEVEL                   = 0x813D;
const uint GL_SMOOTH_POINT_SIZE_RANGE             = 0x0B12;
const uint GL_SMOOTH_POINT_SIZE_GRANULARITY       = 0x0B13;
const uint GL_SMOOTH_LINE_WIDTH_RANGE             = 0x0B22;
const uint GL_SMOOTH_LINE_WIDTH_GRANULARITY       = 0x0B23;
const uint GL_ALIASED_POINT_SIZE_RANGE            = 0x846D;
const uint GL_ALIASED_LINE_WIDTH_RANGE            = 0x846E;
const uint GL_PACK_SKIP_IMAGES                    = 0x806B;
const uint GL_PACK_IMAGE_HEIGHT                   = 0x806C;
const uint GL_UNPACK_SKIP_IMAGES                  = 0x806D;
const uint GL_UNPACK_IMAGE_HEIGHT                 = 0x806E;
const uint GL_TEXTURE_3D                          = 0x806F;
const uint GL_PROXY_TEXTURE_3D                    = 0x8070;
const uint GL_TEXTURE_DEPTH                       = 0x8071;
const uint GL_TEXTURE_WRAP_R                      = 0x8072;
const uint GL_MAX_3D_TEXTURE_SIZE                 = 0x8073;
const uint GL_TEXTURE_BINDING_3D                  = 0x806A;


/* OpenGL 1.3 */
/* multitexture */
const uint GL_TEXTURE0                            = 0x84C0;
const uint GL_TEXTURE1                            = 0x84C1;
const uint GL_TEXTURE2                            = 0x84C2;
const uint GL_TEXTURE3                            = 0x84C3;
const uint GL_TEXTURE4                            = 0x84C4;
const uint GL_TEXTURE5                            = 0x84C5;
const uint GL_TEXTURE6                            = 0x84C6;
const uint GL_TEXTURE7                            = 0x84C7;
const uint GL_TEXTURE8                            = 0x84C8;
const uint GL_TEXTURE9                            = 0x84C9;
const uint GL_TEXTURE10                           = 0x84CA;
const uint GL_TEXTURE11                           = 0x84CB;
const uint GL_TEXTURE12                           = 0x84CC;
const uint GL_TEXTURE13                           = 0x84CD;
const uint GL_TEXTURE14                           = 0x84CE;
const uint GL_TEXTURE15                           = 0x84CF;
const uint GL_TEXTURE16                           = 0x84D0;
const uint GL_TEXTURE17                           = 0x84D1;
const uint GL_TEXTURE18                           = 0x84D2;
const uint GL_TEXTURE19                           = 0x84D3;
const uint GL_TEXTURE20                           = 0x84D4;
const uint GL_TEXTURE21                           = 0x84D5;
const uint GL_TEXTURE22                           = 0x84D6;
const uint GL_TEXTURE23                           = 0x84D7;
const uint GL_TEXTURE24                           = 0x84D8;
const uint GL_TEXTURE25                           = 0x84D9;
const uint GL_TEXTURE26                           = 0x84DA;
const uint GL_TEXTURE27                           = 0x84DB;
const uint GL_TEXTURE28                           = 0x84DC;
const uint GL_TEXTURE29                           = 0x84DD;
const uint GL_TEXTURE30                           = 0x84DE;
const uint GL_TEXTURE31                           = 0x84DF;
const uint GL_ACTIVE_TEXTURE                      = 0x84E0;
const uint GL_CLIENT_ACTIVE_TEXTURE               = 0x84E1;
const uint GL_MAX_TEXTURE_UNITS                   = 0x84E2;
/* texture_cube_map */
const uint GL_NORMAL_MAP                          = 0x8511;
const uint GL_REFLECTION_MAP                      = 0x8512;
const uint GL_TEXTURE_CUBE_MAP                    = 0x8513;
const uint GL_TEXTURE_BINDING_CUBE_MAP            = 0x8514;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_X         = 0x8515;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_X         = 0x8516;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_Y         = 0x8517;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_Y         = 0x8518;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_Z         = 0x8519;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_Z         = 0x851A;
const uint GL_PROXY_TEXTURE_CUBE_MAP              = 0x851B;
const uint GL_MAX_CUBE_MAP_TEXTURE_SIZE           = 0x851C;
/* texture_compression */
const uint GL_COMPRESSED_ALPHA                    = 0x84E9;
const uint GL_COMPRESSED_LUMINANCE                = 0x84EA;
const uint GL_COMPRESSED_LUMINANCE_ALPHA          = 0x84EB;
const uint GL_COMPRESSED_INTENSITY                = 0x84EC;
const uint GL_COMPRESSED_RGB                      = 0x84ED;
const uint GL_COMPRESSED_RGBA                     = 0x84EE;
const uint GL_TEXTURE_COMPRESSION_HINT            = 0x84EF;
const uint GL_TEXTURE_COMPRESSED_IMAGE_SIZE       = 0x86A0;
const uint GL_TEXTURE_COMPRESSED                  = 0x86A1;
const uint GL_NUM_COMPRESSED_TEXTURE_FORMATS      = 0x86A2;
const uint GL_COMPRESSED_TEXTURE_FORMATS          = 0x86A3;
/* multisample */
const uint GL_MULTISAMPLE                         = 0x809D;
const uint GL_SAMPLE_ALPHA_TO_COVERAGE            = 0x809E;
const uint GL_SAMPLE_ALPHA_TO_ONE                 = 0x809F;
const uint GL_SAMPLE_COVERAGE                     = 0x80A0;
const uint GL_SAMPLE_BUFFERS                      = 0x80A8;
const uint GL_SAMPLES                             = 0x80A9;
const uint GL_SAMPLE_COVERAGE_VALUE               = 0x80AA;
const uint GL_SAMPLE_COVERAGE_INVERT              = 0x80AB;
const uint GL_MULTISAMPLE_BIT                     = 0x20000000;
/* transpose_matrix */
const uint GL_TRANSPOSE_MODELVIEW_MATRIX          = 0x84E3;
const uint GL_TRANSPOSE_PROJECTION_MATRIX         = 0x84E4;
const uint GL_TRANSPOSE_TEXTURE_MATRIX            = 0x84E5;
const uint GL_TRANSPOSE_COLOR_MATRIX              = 0x84E6;
/* texture_env_combine */
const uint GL_COMBINE                             = 0x8570;
const uint GL_COMBINE_RGB                         = 0x8571;
const uint GL_COMBINE_ALPHA                       = 0x8572;
const uint GL_SOURCE0_RGB                         = 0x8580;
const uint GL_SOURCE1_RGB                         = 0x8581;
const uint GL_SOURCE2_RGB                         = 0x8582;
const uint GL_SOURCE0_ALPHA                       = 0x8588;
const uint GL_SOURCE1_ALPHA                       = 0x8589;
const uint GL_SOURCE2_ALPHA                       = 0x858A;
const uint GL_OPERAND0_RGB                        = 0x8590;
const uint GL_OPERAND1_RGB                        = 0x8591;
const uint GL_OPERAND2_RGB                        = 0x8592;
const uint GL_OPERAND0_ALPHA                      = 0x8598;
const uint GL_OPERAND1_ALPHA                      = 0x8599;
const uint GL_OPERAND2_ALPHA                      = 0x859A;
const uint GL_RGB_SCALE                           = 0x8573;
const uint GL_ADD_SIGNED                          = 0x8574;
const uint GL_INTERPOLATE                         = 0x8575;
const uint GL_SUBTRACT                            = 0x84E7;
const uint GL_CONSTANT                            = 0x8576;
const uint GL_PRIMARY_COLOR                       = 0x8577;
const uint GL_PREVIOUS                            = 0x8578;
/* texture_env_dot3 */
const uint GL_DOT3_RGB                            = 0x86AE;
const uint GL_DOT3_RGBA                           = 0x86AF;
/* texture_border_clamp */
const uint GL_CLAMP_TO_BORDER                     = 0x812D;


/* OpenGL 1.4 */
const uint GL_GENERATE_MIPMAP                     = 0x8191;
const uint GL_GENERATE_MIPMAP_HINT                = 0x8192;
const uint GL_DEPTH_COMPONENT16                   = 0x81A5;
const uint GL_DEPTH_COMPONENT24                   = 0x81A6;
const uint GL_DEPTH_COMPONENT32                   = 0x81A7;
const uint GL_TEXTURE_DEPTH_SIZE                  = 0x884A;
const uint GL_DEPTH_TEXTURE_MODE                  = 0x884B;
const uint GL_TEXTURE_COMPARE_MODE                = 0x884C;
const uint GL_TEXTURE_COMPARE_FUNC                = 0x884D;
const uint GL_COMPARE_R_TO_TEXTURE                = 0x884E;
const uint GL_FOG_COORDINATE_SOURCE               = 0x8450;
const uint GL_FOG_COORDINATE                      = 0x8451;
const uint GL_FRAGMENT_DEPTH                      = 0x8452;
const uint GL_CURRENT_FOG_COORDINATE              = 0x8453;
const uint GL_FOG_COORDINATE_ARRAY_TYPE           = 0x8454;
const uint GL_FOG_COORDINATE_ARRAY_STRIDE         = 0x8455;
const uint GL_FOG_COORDINATE_ARRAY_POINTER        = 0x8456;
const uint GL_FOG_COORDINATE_ARRAY                = 0x8457;
const uint GL_POINT_SIZE_MIN                      = 0x8126;
const uint GL_POINT_SIZE_MAX                      = 0x8127;
const uint GL_POINT_FADE_THRESHOLD_SIZE           = 0x8128;
const uint GL_POINT_DISTANCE_ATTENUATION          = 0x8129;
const uint GL_COLOR_SUM                           = 0x8458;
const uint GL_CURRENT_SECONDARY_COLOR             = 0x8459;
const uint GL_SECONDARY_COLOR_ARRAY_SIZE          = 0x845A;
const uint GL_SECONDARY_COLOR_ARRAY_TYPE          = 0x845B;
const uint GL_SECONDARY_COLOR_ARRAY_STRIDE        = 0x845C;
const uint GL_SECONDARY_COLOR_ARRAY_POINTER       = 0x845D;
const uint GL_SECONDARY_COLOR_ARRAY               = 0x845E;
const uint GL_BLEND_DST_RGB                       = 0x80C8;
const uint GL_BLEND_SRC_RGB                       = 0x80C9;
const uint GL_BLEND_DST_ALPHA                     = 0x80CA;
const uint GL_BLEND_SRC_ALPHA                     = 0x80CB;
const uint GL_INCR_WRAP                           = 0x8507;
const uint GL_DECR_WRAP                           = 0x8508;
const uint GL_TEXTURE_FILTER_CONTROL              = 0x8500;
const uint GL_TEXTURE_LOD_BIAS                    = 0x8501;
const uint GL_MAX_TEXTURE_LOD_BIAS                = 0x84FD;
const uint GL_MIRRORED_REPEAT                     = 0x8370;




/************************************************************************
 *
 * Function prototypes
 *
 ************************************************************************/

/* Miscellaneous */
void glClearIndex( GLfloat c );
void glClearColor( GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha );
void glClear( GLbitfield mask );
void glIndexMask( GLuint mask );
void glColorMask( GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha );
void glAlphaFunc( GLenum func, GLclampf reference );
void glBlendFunc( GLenum sfactor, GLenum dfactor );
void glLogicOp( GLenum opcode );
void glCullFace( GLenum mode );
void glFrontFace( GLenum mode );
void glPointSize( GLfloat size );
void glLineWidth( GLfloat width );
void glLineStipple( GLint factor, GLushort pattern );
void glPolygonMode( GLenum face, GLenum mode );
void glPolygonOffset( GLfloat factor, GLfloat units );
void glPolygonStipple( in GLubyte *mask );
void glGetPolygonStipple( GLubyte *mask );
void glEdgeFlag( GLboolean flag );
void glEdgeFlagv( in GLboolean *flag );
void glScissor( GLint x, GLint y, GLsizei width, GLsizei height);
void glClipPlane( GLenum plane, in GLdouble *equation );
void glGetClipPlane( GLenum plane, GLdouble *equation );
void glDrawBuffer( GLenum mode );
void glReadBuffer( GLenum mode );
void glEnable( GLenum cap );
void glDisable( GLenum cap );
GLboolean glIsEnabled( GLenum cap );
void glEnableClientState( GLenum cap );  /* 1.1 */
void glDisableClientState( GLenum cap );  /* 1.1 */
void glGetBooleanv( GLenum pname, GLboolean *params );
void glGetDoublev( GLenum pname, GLdouble *params );
void glGetFloatv( GLenum pname, GLfloat *params );
void glGetIntegerv( GLenum pname, GLint *params );
void glPushAttrib( GLbitfield mask );
void glPopAttrib();
void glPushClientAttrib( GLbitfield mask );  /* 1.1 */
void glPopClientAttrib();  /* 1.1 */
GLint glRenderMode( GLenum mode );
GLenum glGetError();
GLubyte* glGetString( GLenum name );
void glFinish();
void glFlush();
void glHint( GLenum target, GLenum mode );

/* Depth Buffer */
void glClearDepth( GLclampd depth );
void glDepthFunc( GLenum func );
void glDepthMask( GLboolean flag );
void glDepthRange( GLclampd near_val, GLclampd far_val );

/* Accumulation Buffer */
void glClearAccum( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
void glAccum( GLenum op, GLfloat value );

/* Transformation */
void glMatrixMode( GLenum mode );
void glOrtho( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
void glFrustum( GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val );
void glViewport( GLint x, GLint y, GLsizei width, GLsizei height );
void glPushMatrix();
void glPopMatrix();
void glLoadIdentity();
void glLoadMatrixd( in GLdouble *m );
void glLoadMatrixf( in GLfloat *m );
void glMultMatrixd( in GLdouble *m );
void glMultMatrixf( in GLfloat *m );
void glRotated( GLdouble angle, GLdouble x, GLdouble y, GLdouble z );
void glRotatef( GLfloat angle, GLfloat x, GLfloat y, GLfloat z );
void glScaled( GLdouble x, GLdouble y, GLdouble z );
void glScalef( GLfloat x, GLfloat y, GLfloat z );
void glTranslated( GLdouble x, GLdouble y, GLdouble z );
void glTranslatef( GLfloat x, GLfloat y, GLfloat z );

/* Display Lists */
GLboolean glIsList( GLuint list );
void glDeleteLists( GLuint list, GLsizei range );
GLuint glGenLists( GLsizei range );
void glNewList( GLuint list, GLenum mode );
void glEndList();
void glCallList( GLuint list );
void glCallLists( GLsizei n, GLenum type, in GLvoid *lists );
void glListBase( GLuint base );

/* Drawing Functions */
void glBegin( GLenum mode );
void glEnd();
void glVertex2d( GLdouble x, GLdouble y );
void glVertex2f( GLfloat x, GLfloat y );
void glVertex2i( GLint x, GLint y );
void glVertex2s( GLshort x, GLshort y );
void glVertex3d( GLdouble x, GLdouble y, GLdouble z );
void glVertex3f( GLfloat x, GLfloat y, GLfloat z );
void glVertex3i( GLint x, GLint y, GLint z );
void glVertex3s( GLshort x, GLshort y, GLshort z );
void glVertex4d( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
void glVertex4f( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
void glVertex4i( GLint x, GLint y, GLint z, GLint w );
void glVertex4s( GLshort x, GLshort y, GLshort z, GLshort w );
void glVertex2dv( in GLdouble *v );
void glVertex2fv( in GLfloat *v );
void glVertex2iv( in GLint *v );
void glVertex2sv( in GLshort *v );
void glVertex3dv( in GLdouble *v );
void glVertex3fv( in GLfloat *v );
void glVertex3iv( in GLint *v );
void glVertex3sv( in GLshort *v );
void glVertex4dv( in GLdouble *v );
void glVertex4fv( in GLfloat *v );
void glVertex4iv( in GLint *v );
void glVertex4sv( in GLshort *v );
void glNormal3b( GLbyte nx, GLbyte ny, GLbyte nz );
void glNormal3d( GLdouble nx, GLdouble ny, GLdouble nz );
void glNormal3f( GLfloat nx, GLfloat ny, GLfloat nz );
void glNormal3i( GLint nx, GLint ny, GLint nz );
void glNormal3s( GLshort nx, GLshort ny, GLshort nz );
void glNormal3bv( in GLbyte *v );
void glNormal3dv( in GLdouble *v );
void glNormal3fv( in GLfloat *v );
void glNormal3iv( in GLint *v );
void glNormal3sv( in GLshort *v );
void glIndexd( GLdouble c );
void glIndexf( GLfloat c );
void glIndexi( GLint c );
void glIndexs( GLshort c );
void glIndexub( GLubyte c );  /* 1.1 */
void glIndexdv( in GLdouble *c );
void glIndexfv( in GLfloat *c );
void glIndexiv( in GLint *c );
void glIndexsv( in GLshort *c );
void glIndexubv( in GLubyte *c );  /* 1.1 */
void glColor3b( GLbyte red, GLbyte green, GLbyte blue );
void glColor3d( GLdouble red, GLdouble green, GLdouble blue );
void glColor3f( GLfloat red, GLfloat green, GLfloat blue );
void glColor3i( GLint red, GLint green, GLint blue );
void glColor3s( GLshort red, GLshort green, GLshort blue );
void glColor3ub( GLubyte red, GLubyte green, GLubyte blue );
void glColor3ui( GLuint red, GLuint green, GLuint blue );
void glColor3us( GLushort red, GLushort green, GLushort blue );
void glColor4b( GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha );
void glColor4d( GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha );
void glColor4f( GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha );
void glColor4i( GLint red, GLint green, GLint blue, GLint alpha );
void glColor4s( GLshort red, GLshort green, GLshort blue, GLshort alpha );
void glColor4ub( GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha );
void glColor4ui( GLuint red, GLuint green, GLuint blue, GLuint alpha );
void glColor4us( GLushort red, GLushort green, GLushort blue, GLushort alpha );
void glColor3bv( in GLbyte *v );
void glColor3dv( in GLdouble *v );
void glColor3fv( in GLfloat *v );
void glColor3iv( in GLint *v );
void glColor3sv( in GLshort *v );
void glColor3ubv( in GLubyte *v );
void glColor3uiv( in GLuint *v );
void glColor3usv( in GLushort *v );
void glColor4bv( in GLbyte *v );
void glColor4dv( in GLdouble *v );
void glColor4fv( in GLfloat *v );
void glColor4iv( in GLint *v );
void glColor4sv( in GLshort *v );
void glColor4ubv( in GLubyte *v );
void glColor4uiv( in GLuint *v );
void glColor4usv( in GLushort *v );
void glTexCoord1d( GLdouble s );
void glTexCoord1f( GLfloat s );
void glTexCoord1i( GLint s );
void glTexCoord1s( GLshort s );
void glTexCoord2d( GLdouble s, GLdouble t );
void glTexCoord2f( GLfloat s, GLfloat t );
void glTexCoord2i( GLint s, GLint t );
void glTexCoord2s( GLshort s, GLshort t );
void glTexCoord3d( GLdouble s, GLdouble t, GLdouble r );
void glTexCoord3f( GLfloat s, GLfloat t, GLfloat r );
void glTexCoord3i( GLint s, GLint t, GLint r );
void glTexCoord3s( GLshort s, GLshort t, GLshort r );
void glTexCoord4d( GLdouble s, GLdouble t, GLdouble r, GLdouble q );
void glTexCoord4f( GLfloat s, GLfloat t, GLfloat r, GLfloat q );
void glTexCoord4i( GLint s, GLint t, GLint r, GLint q );
void glTexCoord4s( GLshort s, GLshort t, GLshort r, GLshort q );
void glTexCoord1dv( in GLdouble *v );
void glTexCoord1fv( in GLfloat *v );
void glTexCoord1iv( in GLint *v );
void glTexCoord1sv( in GLshort *v );
void glTexCoord2dv( in GLdouble *v );
void glTexCoord2fv( in GLfloat *v );
void glTexCoord2iv( in GLint *v );
void glTexCoord2sv( in GLshort *v );
void glTexCoord3dv( in GLdouble *v );
void glTexCoord3fv( in GLfloat *v );
void glTexCoord3iv( in GLint *v );
void glTexCoord3sv( in GLshort *v );
void glTexCoord4dv( in GLdouble *v );
void glTexCoord4fv( in GLfloat *v );
void glTexCoord4iv( in GLint *v );
void glTexCoord4sv( in GLshort *v );
void glRasterPos2d( GLdouble x, GLdouble y );
void glRasterPos2f( GLfloat x, GLfloat y );
void glRasterPos2i( GLint x, GLint y );
void glRasterPos2s( GLshort x, GLshort y );
void glRasterPos3d( GLdouble x, GLdouble y, GLdouble z );
void glRasterPos3f( GLfloat x, GLfloat y, GLfloat z );
void glRasterPos3i( GLint x, GLint y, GLint z );
void glRasterPos3s( GLshort x, GLshort y, GLshort z );
void glRasterPos4d( GLdouble x, GLdouble y, GLdouble z, GLdouble w );
void glRasterPos4f( GLfloat x, GLfloat y, GLfloat z, GLfloat w );
void glRasterPos4i( GLint x, GLint y, GLint z, GLint w );
void glRasterPos4s( GLshort x, GLshort y, GLshort z, GLshort w );
void glRasterPos2dv( in GLdouble *v );
void glRasterPos2fv( in GLfloat *v );
void glRasterPos2iv( in GLint *v );
void glRasterPos2sv( in GLshort *v );
void glRasterPos3dv( in GLdouble *v );
void glRasterPos3fv( in GLfloat *v );
void glRasterPos3iv( in GLint *v );
void glRasterPos3sv( in GLshort *v );
void glRasterPos4dv( in GLdouble *v );
void glRasterPos4fv( in GLfloat *v );
void glRasterPos4iv( in GLint *v );
void glRasterPos4sv( in GLshort *v );
void glRectd( GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2 );
void glRectf( GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2 );
void glRecti( GLint x1, GLint y1, GLint x2, GLint y2 );
void glRects( GLshort x1, GLshort y1, GLshort x2, GLshort y2 );
void glRectdv( in GLdouble *v1, in GLdouble *v2 );
void glRectfv( in GLfloat *v1, in GLfloat *v2 );
void glRectiv( in GLint *v1, in GLint *v2 );
void glRectsv( in GLshort *v1, in GLshort *v2 );

/* Lighting */
void glShadeModel( GLenum mode );
void glLightf( GLenum light, GLenum pname, GLfloat param );
void glLighti( GLenum light, GLenum pname, GLint param );
void glLightfv( GLenum light, GLenum pname, in GLfloat *params );
void glLightiv( GLenum light, GLenum pname, in GLint *params );
void glGetLightfv( GLenum light, GLenum pname, GLfloat *params );
void glGetLightiv( GLenum light, GLenum pname, GLint *params );
void glLightModelf( GLenum pname, GLfloat param );
void glLightModeli( GLenum pname, GLint param );
void glLightModelfv( GLenum pname, in GLfloat *params );
void glLightModeliv( GLenum pname, in GLint *params );
void glMaterialf( GLenum face, GLenum pname, GLfloat param );
void glMateriali( GLenum face, GLenum pname, GLint param );
void glMaterialfv( GLenum face, GLenum pname, in GLfloat *params );
void glMaterialiv( GLenum face, GLenum pname, in GLint *params );
void glGetMaterialfv( GLenum face, GLenum pname, GLfloat *params );
void glGetMaterialiv( GLenum face, GLenum pname, GLint *params );
void glColorMaterial( GLenum face, GLenum mode );

/* Raster functions */
void glPixelZoom( GLfloat xfactor, GLfloat yfactor );
void glPixelStoref( GLenum pname, GLfloat param );
void glPixelStorei( GLenum pname, GLint param );
void glPixelTransferf( GLenum pname, GLfloat param );
void glPixelTransferi( GLenum pname, GLint param );
void glPixelMapfv( GLenum map, GLint mapsize, in GLfloat *values );
void glPixelMapuiv( GLenum map, GLint mapsize, in GLuint *values );
void glPixelMapusv( GLenum map, GLint mapsize, in GLushort *values );
void glGetPixelMapfv( GLenum map, GLfloat *values );
void glGetPixelMapuiv( GLenum map, GLuint *values );
void glGetPixelMapusv( GLenum map, GLushort *values );
void glBitmap( GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, GLubyte *bitmap );
void glReadPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );
void glDrawPixels( GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );
void glCopyPixels( GLint x, GLint y, GLsizei width, GLsizei height, GLenum type );

/* Stenciling */
void glStencilFunc( GLenum func, GLint reference, GLuint mask );
void glStencilMask( GLuint mask );
void glStencilOp( GLenum fail, GLenum zfail, GLenum zpass );
void glClearStencil( GLint s );

/* Texture mapping */
void glTexGend( GLenum coord, GLenum pname, GLdouble param );
void glTexGenf( GLenum coord, GLenum pname, GLfloat param );
void glTexGeni( GLenum coord, GLenum pname, GLint param );
void glTexGendv( GLenum coord, GLenum pname, in GLdouble *params );
void glTexGenfv( GLenum coord, GLenum pname, in GLfloat *params );
void glTexGeniv( GLenum coord, GLenum pname, in GLint *params );
void glGetTexGendv( GLenum coord, GLenum pname, GLdouble *params );
void glGetTexGenfv( GLenum coord, GLenum pname, GLfloat *params );
void glGetTexGeniv( GLenum coord, GLenum pname, GLint *params );
void glTexEnvf( GLenum target, GLenum pname, GLfloat param );
void glTexEnvi( GLenum target, GLenum pname, GLint param );
void glTexEnvfv( GLenum target, GLenum pname, in GLfloat *params );
void glTexEnviv( GLenum target, GLenum pname, in GLint *params );
void glGetTexEnvfv( GLenum target, GLenum pname, GLfloat *params );
void glGetTexEnviv( GLenum target, GLenum pname, GLint *params );
void glTexParameterf( GLenum target, GLenum pname, GLfloat param );
void glTexParameteri( GLenum target, GLenum pname, GLint param );
void glTexParameterfv( GLenum target, GLenum pname, in GLfloat *params );
void glTexParameteriv( GLenum target, GLenum pname, in GLint *params );
void glGetTexParameterfv( GLenum target, GLenum pname, GLfloat *params);
void glGetTexParameteriv( GLenum target, GLenum pname, GLint *params );
void glGetTexLevelParameterfv( GLenum target, GLint level, GLenum pname, GLfloat *params );
void glGetTexLevelParameteriv( GLenum target, GLint level, GLenum pname, GLint *params );
void glTexImage1D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, in GLvoid *pixels );
void glTexImage2D( GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, in GLvoid *pixels );
void glGetTexImage( GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels );

/* Evaluators */
void glMap1d( GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, in GLdouble *points );
void glMap1f( GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, in GLfloat *points );
void glMap2d( GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, in GLdouble *points );
void glMap2f( GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, in GLfloat *points );
void glGetMapdv( GLenum target, GLenum query, GLdouble *v );
void glGetMapfv( GLenum target, GLenum query, GLfloat *v );
void glGetMapiv( GLenum target, GLenum query, GLint *v );
void glEvalCoord1d( GLdouble u );
void glEvalCoord1f( GLfloat u );
void glEvalCoord1dv( in GLdouble *u );
void glEvalCoord1fv( in GLfloat *u );
void glEvalCoord2d( GLdouble u, GLdouble v );
void glEvalCoord2f( GLfloat u, GLfloat v );
void glEvalCoord2dv( in GLdouble *u );
void glEvalCoord2fv( in GLfloat *u );
void glMapGrid1d( GLint un, GLdouble u1, GLdouble u2 );
void glMapGrid1f( GLint un, GLfloat u1, GLfloat u2 );
void glMapGrid2d( GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2 );
void glMapGrid2f( GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2 );
void glEvalPoint1( GLint i );
void glEvalPoint2( GLint i, GLint j );
void glEvalMesh1( GLenum mode, GLint i1, GLint i2 );
void glEvalMesh2( GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2 );

/* Fog */
void glFogf( GLenum pname, GLfloat param );
void glFogi( GLenum pname, GLint param );
void glFogfv( GLenum pname, in GLfloat *params );
void glFogiv( GLenum pname, in GLint *params );

/* Selection and Feedback */
void glFeedbackBuffer( GLsizei size, GLenum type, GLfloat *buffer );
void glPassThrough( GLfloat token );
void glSelectBuffer( GLsizei size, GLuint *buffer );
void glInitNames();
void glLoadName( GLuint name );
void glPushName( GLuint name );
void glPopName();


/* OpenGL 1.1 functions */
/* texture objects */
void glGenTextures( GLsizei n, GLuint *textures );
void glDeleteTextures( GLsizei n, in GLuint *textures );
void glBindTexture( GLenum target, GLuint texture );
void glPrioritizeTextures( GLsizei n, in GLuint *textures, in GLclampf *priorities );
GLboolean glAreTexturesResident( GLsizei n, in GLuint *textures, GLboolean *residences );
GLboolean glIsTexture( GLuint texture );
/* texture mapping */
void glTexSubImage1D( GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, GLvoid *pixels );
void glTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels );
void glCopyTexImage1D( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border );
void glCopyTexImage2D( GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border );
void glCopyTexSubImage1D( GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width );
void glCopyTexSubImage2D( GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height );
/* vertex arrays */
void glVertexPointer( GLint size, GLenum type, GLsizei stride, in GLvoid *ptr );
void glNormalPointer( GLenum type, GLsizei stride, in GLvoid *ptr );
void glColorPointer( GLint size, GLenum type, GLsizei stride, in GLvoid *ptr );
void glIndexPointer( GLenum type, GLsizei stride, in GLvoid *ptr );
void glTexCoordPointer( GLint size, GLenum type, GLsizei stride, in GLvoid *ptr );
void glEdgeFlagPointer( GLsizei stride, in GLvoid *ptr );
void glGetPointerv( GLenum pname, GLvoid **params );
void glArrayElement( GLint i );
void glDrawArrays( GLenum mode, GLint first, GLsizei count );
void glDrawElements( GLenum mode, GLsizei count, GLenum type, in GLvoid *indices );
void glInterleavedArrays( GLenum format, GLsizei stride, in GLvoid *pointer );



/************************************************************************
 * OpenGL 1.2 -> 1.4 function pointer types
 * Note: Proper OpenGL version checking must be performed to use these.
 ************************************************************************/

/* OpenGL 1.2 */
typedef void (* PFNGLBLENDCOLORPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
typedef void (* PFNGLBLENDEQUATIONPROC) (GLenum mode);
typedef void (* PFNGLDRAWRANGEELEMENTSPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLvoid *indices);
typedef void (* PFNGLCOLORTABLEPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, GLvoid *table);
typedef void (* PFNGLCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLCOPYCOLORTABLEPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (* PFNGLGETCOLORTABLEPROC) (GLenum target, GLenum format, GLenum type, GLvoid *table);
typedef void (* PFNGLGETCOLORTABLEPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCOLORTABLEPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLCOLORSUBTABLEPROC) (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, GLvoid *data);
typedef void (* PFNGLCOPYCOLORSUBTABLEPROC) (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);
typedef void (* PFNGLCONVOLUTIONFILTER1DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, GLvoid *image);
typedef void (* PFNGLCONVOLUTIONFILTER2DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *image);
typedef void (* PFNGLCONVOLUTIONPARAMETERFPROC) (GLenum target, GLenum pname, GLfloat params);
typedef void (* PFNGLCONVOLUTIONPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLCONVOLUTIONPARAMETERIPROC) (GLenum target, GLenum pname, GLint params);
typedef void (* PFNGLCONVOLUTIONPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLCOPYCONVOLUTIONFILTER1DPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (* PFNGLCOPYCONVOLUTIONFILTER2DPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (* PFNGLGETCONVOLUTIONFILTERPROC) (GLenum target, GLenum format, GLenum type, GLvoid *image);
typedef void (* PFNGLGETCONVOLUTIONPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCONVOLUTIONPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETSEPARABLEFILTERPROC) (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span);
typedef void (* PFNGLSEPARABLEFILTER2DPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *row, GLvoid *column);
typedef void (* PFNGLGETHISTOGRAMPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (* PFNGLGETHISTOGRAMPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETHISTOGRAMPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETMINMAXPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (* PFNGLGETMINMAXPARAMETERFVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETMINMAXPARAMETERIVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLHISTOGRAMPROC) (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
typedef void (* PFNGLMINMAXPROC) (GLenum target, GLenum internalformat, GLboolean sink);
typedef void (* PFNGLRESETHISTOGRAMPROC) (GLenum target);
typedef void (* PFNGLRESETMINMAXPROC) (GLenum target);
typedef void (* PFNGLTEXIMAGE3DPROC) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, GLvoid *pixels);
typedef void (* PFNGLTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLvoid *pixels);
typedef void (* PFNGLCOPYTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);


/* OpenGL 1.3 */
typedef void (* PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (* PFNGLCLIENTACTIVETEXTUREPROC) (GLenum texture);
typedef void (* PFNGLMULTITEXCOORD1DPROC) (GLenum target, GLdouble s);
typedef void (* PFNGLMULTITEXCOORD1DVPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD1FPROC) (GLenum target, GLfloat s);
typedef void (* PFNGLMULTITEXCOORD1FVPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD1IPROC) (GLenum target, GLint s);
typedef void (* PFNGLMULTITEXCOORD1IVPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD1SPROC) (GLenum target, GLshort s);
typedef void (* PFNGLMULTITEXCOORD1SVPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLMULTITEXCOORD2DPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (* PFNGLMULTITEXCOORD2DVPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD2FPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (* PFNGLMULTITEXCOORD2FVPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD2IPROC) (GLenum target, GLint s, GLint t);
typedef void (* PFNGLMULTITEXCOORD2IVPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD2SPROC) (GLenum target, GLshort s, GLshort t);
typedef void (* PFNGLMULTITEXCOORD2SVPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLMULTITEXCOORD3DPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (* PFNGLMULTITEXCOORD3DVPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD3FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (* PFNGLMULTITEXCOORD3FVPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD3IPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (* PFNGLMULTITEXCOORD3IVPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD3SPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (* PFNGLMULTITEXCOORD3SVPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLMULTITEXCOORD4DPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (* PFNGLMULTITEXCOORD4DVPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD4FPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (* PFNGLMULTITEXCOORD4FVPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD4IPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (* PFNGLMULTITEXCOORD4IVPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD4SPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (* PFNGLMULTITEXCOORD4SVPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLLOADTRANSPOSEMATRIXFPROC) (GLfloat *m);
typedef void (* PFNGLLOADTRANSPOSEMATRIXDPROC) (GLdouble *m);
typedef void (* PFNGLMULTTRANSPOSEMATRIXFPROC) (GLfloat *m);
typedef void (* PFNGLMULTTRANSPOSEMATRIXDPROC) (GLdouble *m);
typedef void (* PFNGLSAMPLECOVERAGEPROC) (GLclampf value, GLboolean invert);
typedef void (* PFNGLCOMPRESSEDTEXIMAGE3DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXIMAGE2DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXIMAGE1DPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLGETCOMPRESSEDTEXIMAGEPROC) (GLenum target, GLint level, void *img);

/* OpenGL 1.4 */
typedef void (* PFNGLFOGCOORDFPROC) (GLfloat coord);
typedef void (* PFNGLFOGCOORDFVPROC) (GLfloat *coord);
typedef void (* PFNGLFOGCOORDDPROC) (GLdouble coord);
typedef void (* PFNGLFOGCOORDDVPROC) (GLdouble *coord);
typedef void (* PFNGLFOGCOORDPOINTERPROC) (GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (* PFNGLMULTIDRAWARRAYSPROC) (GLenum mode, GLint *first, GLsizei *count, GLsizei primcount);
typedef void (* PFNGLMULTIDRAWELEMENTSPROC) (GLenum mode, GLsizei *count, GLenum type, GLvoid **indices, GLsizei primcount);
typedef void (* PFNGLPOINTPARAMETERFPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLPOINTPARAMETERFVPROC) (GLenum pname, GLfloat *params);
typedef void (* PFNGLSECONDARYCOLOR3BPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (* PFNGLSECONDARYCOLOR3BVPROC) (GLbyte *v);
typedef void (* PFNGLSECONDARYCOLOR3DPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (* PFNGLSECONDARYCOLOR3DVPROC) (GLdouble *v);
typedef void (* PFNGLSECONDARYCOLOR3FPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (* PFNGLSECONDARYCOLOR3FVPROC) (GLfloat *v);
typedef void (* PFNGLSECONDARYCOLOR3IPROC) (GLint red, GLint green, GLint blue);
typedef void (* PFNGLSECONDARYCOLOR3IVPROC) (GLint *v);
typedef void (* PFNGLSECONDARYCOLOR3SPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (* PFNGLSECONDARYCOLOR3SVPROC) (GLshort *v);
typedef void (* PFNGLSECONDARYCOLOR3UBPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (* PFNGLSECONDARYCOLOR3UBVPROC) (GLubyte *v);
typedef void (* PFNGLSECONDARYCOLOR3UIPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (* PFNGLSECONDARYCOLOR3UIVPROC) (GLuint *v);
typedef void (* PFNGLSECONDARYCOLOR3USPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (* PFNGLSECONDARYCOLOR3USVPROC) (GLushort *v);
typedef void (* PFNGLSECONDARYCOLORPOINTERPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (* PFNGLBLENDFUNCSEPARATEPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (* PFNGLWINDOWPOS2DPROC) (GLdouble x, GLdouble y);
typedef void (* PFNGLWINDOWPOS2FPROC) (GLfloat x, GLfloat y);
typedef void (* PFNGLWINDOWPOS2IPROC) (GLint x, GLint y);
typedef void (* PFNGLWINDOWPOS2SPROC) (GLshort x, GLshort y);
typedef void (* PFNGLWINDOWPOS2DVPROC) (GLdouble *p);
typedef void (* PFNGLWINDOWPOS2FVPROC) (GLfloat *p);
typedef void (* PFNGLWINDOWPOS2IVPROC) (GLint *p);
typedef void (* PFNGLWINDOWPOS2SVPROC) (GLshort *p);
typedef void (* PFNGLWINDOWPOS3DPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (* PFNGLWINDOWPOS3FPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLWINDOWPOS3IPROC) (GLint x, GLint y, GLint z);
typedef void (* PFNGLWINDOWPOS3SPROC) (GLshort x, GLshort y, GLshort z);
typedef void (* PFNGLWINDOWPOS3DVPROC) (GLdouble *p);
typedef void (* PFNGLWINDOWPOS3FVPROC) (GLfloat *p);
typedef void (* PFNGLWINDOWPOS3IVPROC) (GLint *p);
typedef void (* PFNGLWINDOWPOS3SVPROC) (GLshort *p);



/************************************************************************
 *
 * Extension Constants
 *
 * Note: Proper OpenGL extension checking must be performed to use these.
 *
 ************************************************************************/

/*-----------------------------------------------------------------------
 * ARB
 *----------------------------------------------------------------------*/

const uint GL_ARB_depth_texture = 1;
const uint GL_DEPTH_COMPONENT16_ARB          = 0x81A5;
const uint GL_DEPTH_COMPONENT24_ARB          = 0x81A6;
const uint GL_DEPTH_COMPONENT32_ARB          = 0x81A7;
const uint GL_TEXTURE_DEPTH_SIZE_ARB         = 0x884A;
const uint GL_DEPTH_TEXTURE_MODE_ARB         = 0x884B;

const uint GL_ARB_fragment_program = 1;
const uint GL_FRAGMENT_PROGRAM_ARB                     = 0x8804;
const uint GL_PROGRAM_ALU_INSTRUCTIONS_ARB             = 0x8805;
const uint GL_PROGRAM_TEX_INSTRUCTIONS_ARB             = 0x8806;
const uint GL_PROGRAM_TEX_INDIRECTIONS_ARB             = 0x8807;
const uint GL_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB      = 0x8808;
const uint GL_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB      = 0x8809;
const uint GL_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB      = 0x880A;
const uint GL_MAX_PROGRAM_ALU_INSTRUCTIONS_ARB         = 0x880B;
const uint GL_MAX_PROGRAM_TEX_INSTRUCTIONS_ARB         = 0x880C;
const uint GL_MAX_PROGRAM_TEX_INDIRECTIONS_ARB         = 0x880D;
const uint GL_MAX_PROGRAM_NATIVE_ALU_INSTRUCTIONS_ARB  = 0x880E;
const uint GL_MAX_PROGRAM_NATIVE_TEX_INSTRUCTIONS_ARB  = 0x880F;
const uint GL_MAX_PROGRAM_NATIVE_TEX_INDIRECTIONS_ARB  = 0x8810;
const uint GL_MAX_TEXTURE_COORDS_ARB                   = 0x8871;
const uint GL_MAX_TEXTURE_IMAGE_UNITS_ARB              = 0x8872;

/* OpenGL 1.2 imaging subset */
const uint GL_ARB_imaging = 1;
const uint GL_CONSTANT_COLOR                 = 0x8001;
const uint GL_ONE_MINUS_CONSTANT_COLOR       = 0x8002;
const uint GL_CONSTANT_ALPHA                 = 0x8003;
const uint GL_ONE_MINUS_CONSTANT_ALPHA       = 0x8004;
const uint GL_BLEND_COLOR                    = 0x8005;
const uint GL_FUNC_ADD                       = 0x8006;
const uint GL_MIN                            = 0x8007;
const uint GL_MAX                            = 0x8008;
const uint GL_BLEND_EQUATION                 = 0x8009;
const uint GL_FUNC_SUBTRACT                  = 0x800A;
const uint GL_FUNC_REVERSE_SUBTRACT          = 0x800B;
const uint GL_CONVOLUTION_1D                 = 0x8010;
const uint GL_CONVOLUTION_2D                 = 0x8011;
const uint GL_SEPARABLE_2D                   = 0x8012;
const uint GL_CONVOLUTION_BORDER_MODE        = 0x8013;
const uint GL_CONVOLUTION_FILTER_SCALE       = 0x8014;
const uint GL_CONVOLUTION_FILTER_BIAS        = 0x8015;
const uint GL_REDUCE                         = 0x8016;
const uint GL_CONVOLUTION_FORMAT             = 0x8017;
const uint GL_CONVOLUTION_WIDTH              = 0x8018;
const uint GL_CONVOLUTION_HEIGHT             = 0x8019;
const uint GL_MAX_CONVOLUTION_WIDTH          = 0x801A;
const uint GL_MAX_CONVOLUTION_HEIGHT         = 0x801B;
const uint GL_POST_CONVOLUTION_RED_SCALE     = 0x801C;
const uint GL_POST_CONVOLUTION_GREEN_SCALE   = 0x801D;
const uint GL_POST_CONVOLUTION_BLUE_SCALE    = 0x801E;
const uint GL_POST_CONVOLUTION_ALPHA_SCALE   = 0x801F;
const uint GL_POST_CONVOLUTION_RED_BIAS      = 0x8020;
const uint GL_POST_CONVOLUTION_GREEN_BIAS    = 0x8021;
const uint GL_POST_CONVOLUTION_BLUE_BIAS     = 0x8022;
const uint GL_POST_CONVOLUTION_ALPHA_BIAS    = 0x8023;
const uint GL_HISTOGRAM                      = 0x8024;
const uint GL_PROXY_HISTOGRAM                = 0x8025;
const uint GL_HISTOGRAM_WIDTH                = 0x8026;
const uint GL_HISTOGRAM_FORMAT               = 0x8027;
const uint GL_HISTOGRAM_RED_SIZE             = 0x8028;
const uint GL_HISTOGRAM_GREEN_SIZE           = 0x8029;
const uint GL_HISTOGRAM_BLUE_SIZE            = 0x802A;
const uint GL_HISTOGRAM_ALPHA_SIZE           = 0x802B;
const uint GL_HISTOGRAM_LUMINANCE_SIZE       = 0x802C;
const uint GL_HISTOGRAM_SINK                 = 0x802D;
const uint GL_MINMAX                         = 0x802E;
const uint GL_MINMAX_FORMAT                  = 0x802F;
const uint GL_MINMAX_SINK                    = 0x8030;
const uint GL_TABLE_TOO_LARGE                = 0x8031;
const uint GL_COLOR_MATRIX                   = 0x80B1;
const uint GL_COLOR_MATRIX_STACK_DEPTH       = 0x80B2;
const uint GL_MAX_COLOR_MATRIX_STACK_DEPTH   = 0x80B3;
const uint GL_POST_COLOR_MATRIX_RED_SCALE    = 0x80B4;
const uint GL_POST_COLOR_MATRIX_GREEN_SCALE  = 0x80B5;
const uint GL_POST_COLOR_MATRIX_BLUE_SCALE   = 0x80B6;
const uint GL_POST_COLOR_MATRIX_ALPHA_SCALE  = 0x80B7;
const uint GL_POST_COLOR_MATRIX_RED_BIAS     = 0x80B8;
const uint GL_POST_COLOR_MATRIX_GREEN_BIAS   = 0x80B9;
const uint GL_POST_COLOR_MATRIX_BLUE_BIAS    = 0x80BA;
const uint GL_POST_COLOR_MATIX_ALPHA_BIAS    = 0x80BB;
const uint GL_COLOR_TABLE                    = 0x80D0;
const uint GL_POST_CONVOLUTION_COLOR_TABLE   = 0x80D1;
const uint GL_POST_COLOR_MATRIX_COLOR_TABLE  = 0x80D2;
const uint GL_PROXY_COLOR_TABLE              = 0x80D3;
const uint GL_PROXY_POST_CONVOLUTION_COLOR_TABLE = 0x80D4;
const uint GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE = 0x80D5;
const uint GL_COLOR_TABLE_SCALE              = 0x80D6;
const uint GL_COLOR_TABLE_BIAS               = 0x80D7;
const uint GL_COLOR_TABLE_FORMAT             = 0x80D8;
const uint GL_COLOR_TABLE_WIDTH              = 0x80D9;
const uint GL_COLOR_TABLE_RED_SIZE           = 0x80DA;
const uint GL_COLOR_TABLE_GREEN_SIZE         = 0x80DB;
const uint GL_COLOR_TABLE_BLUE_SIZE          = 0x80DC;
const uint GL_COLOR_TABLE_ALPHA_SIZE         = 0x80DD;
const uint GL_COLOR_TABLE_LUMINANCE_SIZE     = 0x80DE;
const uint GL_COLOR_TABLE_INTENSITY_SIZE     = 0x80DF;
const uint GL_IGNORE_BORDER                  = 0x8150;
const uint GL_CONSTANT_BORDER                = 0x8151;
const uint GL_WRAP_BORDER                    = 0x8152;
const uint GL_REPLICATE_BORDER               = 0x8153;
const uint GL_CONVOLUTION_BORDER_COLOR       = 0x8154;

const uint GL_ARB_matrix_palette = 1;
const uint GL_MATRIX_PALETTE_ARB             = 0x8840;
const uint GL_MAX_MATRIX_PALETTE_STACK_DEPTH_ARB = 0x8841;
const uint GL_MAX_PALETTE_MATRICES_ARB       = 0x8842;
const uint GL_CURRENT_PALETTE_MATRIX_ARB     = 0x8843;
const uint GL_MATRIX_INDEX_ARRAY_ARB         = 0x8844;
const uint GL_CURRENT_MATRIX_INDEX_ARB       = 0x8845;
const uint GL_MATRIX_INDEX_ARRAY_SIZE_ARB    = 0x8846;
const uint GL_MATRIX_INDEX_ARRAY_TYPE_ARB    = 0x8847;
const uint GL_MATRIX_INDEX_ARRAY_STRIDE_ARB  = 0x8848;
const uint GL_MATRIX_INDEX_ARRAY_POINTER_ARB = 0x8849;

const uint GL_ARB_multisample = 1;
const uint GL_MULTISAMPLE_ARB                = 0x809D;
const uint GL_SAMPLE_ALPHA_TO_COVERAGE_ARB   = 0x809E;
const uint GL_SAMPLE_ALPHA_TO_ONE_ARB        = 0x809F;
const uint GL_SAMPLE_COVERAGE_ARB            = 0x80A0;
const uint GL_SAMPLE_BUFFERS_ARB             = 0x80A8;
const uint GL_SAMPLES_ARB                    = 0x80A9;
const uint GL_SAMPLE_COVERAGE_VALUE_ARB      = 0x80AA;
const uint GL_SAMPLE_COVERAGE_INVERT_ARB     = 0x80AB;
const uint GL_MULTISAMPLE_BIT_ARB            = 0x20000000;

const uint GL_ARB_multitexture = 1;
const uint GL_TEXTURE0_ARB                   = 0x84C0;
const uint GL_TEXTURE1_ARB                   = 0x84C1;
const uint GL_TEXTURE2_ARB                   = 0x84C2;
const uint GL_TEXTURE3_ARB                   = 0x84C3;
const uint GL_TEXTURE4_ARB                   = 0x84C4;
const uint GL_TEXTURE5_ARB                   = 0x84C5;
const uint GL_TEXTURE6_ARB                   = 0x84C6;
const uint GL_TEXTURE7_ARB                   = 0x84C7;
const uint GL_TEXTURE8_ARB                   = 0x84C8;
const uint GL_TEXTURE9_ARB                   = 0x84C9;
const uint GL_TEXTURE10_ARB                  = 0x84CA;
const uint GL_TEXTURE11_ARB                  = 0x84CB;
const uint GL_TEXTURE12_ARB                  = 0x84CC;
const uint GL_TEXTURE13_ARB                  = 0x84CD;
const uint GL_TEXTURE14_ARB                  = 0x84CE;
const uint GL_TEXTURE15_ARB                  = 0x84CF;
const uint GL_TEXTURE16_ARB                  = 0x84D0;
const uint GL_TEXTURE17_ARB                  = 0x84D1;
const uint GL_TEXTURE18_ARB                  = 0x84D2;
const uint GL_TEXTURE19_ARB                  = 0x84D3;
const uint GL_TEXTURE20_ARB                  = 0x84D4;
const uint GL_TEXTURE21_ARB                  = 0x84D5;
const uint GL_TEXTURE22_ARB                  = 0x84D6;
const uint GL_TEXTURE23_ARB                  = 0x84D7;
const uint GL_TEXTURE24_ARB                  = 0x84D8;
const uint GL_TEXTURE25_ARB                  = 0x84D9;
const uint GL_TEXTURE26_ARB                  = 0x84DA;
const uint GL_TEXTURE27_ARB                  = 0x84DB;
const uint GL_TEXTURE28_ARB                  = 0x84DC;
const uint GL_TEXTURE29_ARB                  = 0x84DD;
const uint GL_TEXTURE30_ARB                  = 0x84DE;
const uint GL_TEXTURE31_ARB                  = 0x84DF;
const uint GL_ACTIVE_TEXTURE_ARB             = 0x84E0;
const uint GL_CLIENT_ACTIVE_TEXTURE_ARB      = 0x84E1;
const uint GL_MAX_TEXTURE_UNITS_ARB          = 0x84E2;

const uint GL_ARB_point_parameters = 1;
const uint GL_POINT_SIZE_MIN_ARB             = 0x8126;
const uint GL_POINT_SIZE_MIN_EXT             = 0x8126;
const uint GL_POINT_SIZE_MIN_SGIS            = 0x8126;
const uint GL_POINT_SIZE_MAX_ARB             = 0x8127;
const uint GL_POINT_SIZE_MAX_EXT             = 0x8127;
const uint GL_POINT_SIZE_MAX_SGIS            = 0x8127;
const uint GL_POINT_FADE_THRESHOLD_SIZE_ARB  = 0x8128;
const uint GL_POINT_FADE_THRESHOLD_SIZE_EXT  = 0x8128;
const uint GL_POINT_FADE_THRESHOLD_SIZE_SGIS = 0x8128;
const uint GL_POINT_DISTANCE_ATTENUATION_ARB = 0x8129;
const uint GL_DISTANCE_ATTENUATION_EXT       = 0x8129;
const uint GL_DISTANCE_ATTENUATION_SGIS      = 0x8129;

const uint GL_ARB_shadow = 1;
const uint GL_TEXTURE_COMPARE_MODE_ARB       = 0x884C;
const uint GL_TEXTURE_COMPARE_FUNC_ARB       = 0x884D;
const uint GL_COMPARE_R_TO_TEXTURE_ARB       = 0x884E;

const uint GL_ARB_shadow_ambient = 1;
const uint GL_TEXTURE_COMPARE_FAIL_VALUE_ARB = 0x80BF;

const uint GL_ARB_texture_border_clamp = 1;
const uint GL_CLAMP_TO_BORDER_ARB            = 0x812D;

const uint GL_ARB_texture_compression = 1;
const uint GL_COMPRESSED_ALPHA_ARB           = 0x84E9;
const uint GL_COMPRESSED_LUMINANCE_ARB       = 0x84EA;
const uint GL_COMPRESSED_LUMINANCE_ALPHA_ARB = 0x84EB;
const uint GL_COMPRESSED_INTENSITY_ARB       = 0x84EC;
const uint GL_COMPRESSED_RGB_ARB             = 0x84ED;
const uint GL_COMPRESSED_RGBA_ARB            = 0x84EE;
const uint GL_TEXTURE_COMPRESSION_HINT_ARB   = 0x84EF;
const uint GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB = 0x86A0;
const uint GL_TEXTURE_COMPRESSED_ARB         = 0x86A1;
const uint GL_NUM_COMPRESSED_TEXTURE_FORMATS_ARB = 0x86A2;
const uint GL_COMPRESSED_TEXTURE_FORMATS_ARB = 0x86A3;

const uint GL_ARB_texture_cube_map = 1;
const uint GL_NORMAL_MAP_ARB                 = 0x8511;
const uint GL_REFLECTION_MAP_ARB             = 0x8512;
const uint GL_TEXTURE_CUBE_MAP_ARB           = 0x8513;
const uint GL_TEXTURE_BINDING_CUBE_MAP_ARB   = 0x8514;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB = 0x8515;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB = 0x8516;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB = 0x8517;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB = 0x8518;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB = 0x8519;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB = 0x851A;
const uint GL_PROXY_TEXTURE_CUBE_MAP_ARB     = 0x851B;
const uint GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB  = 0x851C;

const uint GL_ARB_texture_env_add = 1;

const uint GL_ARB_texture_env_combine = 1;
const uint GL_COMBINE_ARB                    = 0x8570;
const uint GL_COMBINE_RGB_ARB                = 0x8571;
const uint GL_COMBINE_ALPHA_ARB              = 0x8572;
const uint GL_SOURCE0_RGB_ARB                = 0x8580;
const uint GL_SOURCE1_RGB_ARB                = 0x8581;
const uint GL_SOURCE2_RGB_ARB                = 0x8582;
const uint GL_SOURCE0_ALPHA_ARB              = 0x8588;
const uint GL_SOURCE1_ALPHA_ARB              = 0x8589;
const uint GL_SOURCE2_ALPHA_ARB              = 0x858A;
const uint GL_OPERAND0_RGB_ARB               = 0x8590;
const uint GL_OPERAND1_RGB_ARB               = 0x8591;
const uint GL_OPERAND2_RGB_ARB               = 0x8592;
const uint GL_OPERAND0_ALPHA_ARB             = 0x8598;
const uint GL_OPERAND1_ALPHA_ARB             = 0x8599;
const uint GL_OPERAND2_ALPHA_ARB             = 0x859A;
const uint GL_RGB_SCALE_ARB                  = 0x8573;
const uint GL_ADD_SIGNED_ARB                 = 0x8574;
const uint GL_INTERPOLATE_ARB                = 0x8575;
const uint GL_SUBTRACT_ARB                   = 0x84E7;
const uint GL_CONSTANT_ARB                   = 0x8576;
const uint GL_PRIMARY_COLOR_ARB              = 0x8577;
const uint GL_PREVIOUS_ARB                   = 0x8578;

const uint GL_ARB_texture_env_crossbar = 1;

const uint GL_ARB_texture_env_dot3 = 1;
const uint GL_DOT3_RGB_ARB                   = 0x86AE;
const uint GL_DOT3_RGBA_ARB                  = 0x86AF;

const uint GL_ARB_texture_mirrored_repeat = 1;
const uint GL_MIRRORED_REPEAT_ARB            = 0x8370;

const uint GL_ARB_transpose_matrix = 1;
const uint GL_TRANSPOSE_MODELVIEW_MATRIX_ARB = 0x84E3;
const uint GL_TRANSPOSE_PROJECTION_MATRIX_ARB = 0x84E4;
const uint GL_TRANSPOSE_TEXTURE_MATRIX_ARB   = 0x84E5;
const uint GL_TRANSPOSE_COLOR_MATRIX_ARB     = 0x84E6;

const uint GL_ARB_vertex_blend = 1;
const uint GL_MAX_VERTEX_UNITS_ARB           = 0x86A4;
const uint GL_ACTIVE_VERTEX_UNITS_ARB        = 0x86A5;
const uint GL_WEIGHT_SUM_UNITY_ARB           = 0x86A6;
const uint GL_VERTEX_BLEND_ARB               = 0x86A7;
const uint GL_CURRENT_WEIGHT_ARB             = 0x86A8;
const uint GL_WEIGHT_ARRAY_TYPE_ARB          = 0x86A9;
const uint GL_WEIGHT_ARRAY_STRIDE_ARB        = 0x86AA;
const uint GL_WEIGHT_ARRAY_SIZE_ARB          = 0x86AB;
const uint GL_WEIGHT_ARRAY_POINTER_ARB       = 0x86AC;
const uint GL_WEIGHT_ARRAY_ARB               = 0x86AD;
const uint GL_MODELVIEW0_ARB                 = 0x1700;
const uint GL_MODELVIEW1_ARB                 = 0x850A;
const uint GL_MODELVIEW2_ARB                 = 0x8722;
const uint GL_MODELVIEW3_ARB                 = 0x8723;
const uint GL_MODELVIEW4_ARB                 = 0x8724;
const uint GL_MODELVIEW5_ARB                 = 0x8725;
const uint GL_MODELVIEW6_ARB                 = 0x8726;
const uint GL_MODELVIEW7_ARB                 = 0x8727;
const uint GL_MODELVIEW8_ARB                 = 0x8728;
const uint GL_MODELVIEW9_ARB                 = 0x8729;
const uint GL_MODELVIEW10_ARB                = 0x872A;
const uint GL_MODELVIEW11_ARB                = 0x872B;
const uint GL_MODELVIEW12_ARB                = 0x872C;
const uint GL_MODELVIEW13_ARB                = 0x872D;
const uint GL_MODELVIEW14_ARB                = 0x872E;
const uint GL_MODELVIEW15_ARB                = 0x872F;
const uint GL_MODELVIEW16_ARB                = 0x8730;
const uint GL_MODELVIEW17_ARB                = 0x8731;
const uint GL_MODELVIEW18_ARB                = 0x8732;
const uint GL_MODELVIEW19_ARB                = 0x8733;
const uint GL_MODELVIEW20_ARB                = 0x8734;
const uint GL_MODELVIEW21_ARB                = 0x8735;
const uint GL_MODELVIEW22_ARB                = 0x8736;
const uint GL_MODELVIEW23_ARB                = 0x8737;
const uint GL_MODELVIEW24_ARB                = 0x8738;
const uint GL_MODELVIEW25_ARB                = 0x8739;
const uint GL_MODELVIEW26_ARB                = 0x873A;
const uint GL_MODELVIEW27_ARB                = 0x873B;
const uint GL_MODELVIEW28_ARB                = 0x873C;
const uint GL_MODELVIEW29_ARB                = 0x873D;
const uint GL_MODELVIEW30_ARB                = 0x873E;
const uint GL_MODELVIEW31_ARB                = 0x873F;

const uint GL_ARB_vertex_buffer_object = 1;
const uint GL_ARRAY_BUFFER_ARB               = 0x8892;
const uint GL_ELEMENT_ARRAY_BUFFER_ARB       = 0x8893;
const uint GL_ARRAY_BUFFER_BINDING_ARB       = 0x8894;
const uint GL_ELEMENT_ARRAY_BUFFER_BINDING_ARB = 0x8895;
const uint GL_VERTEX_ARRAY_BUFFER_BINDING_ARB = 0x8896;
const uint GL_NORMAL_ARRAY_BUFFER_BINDING_ARB = 0x8897;
const uint GL_COLOR_ARRAY_BUFFER_BINDING_ARB = 0x8898;
const uint GL_INDEX_ARRAY_BUFFER_BINDING_ARB = 0x8899;
const uint GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING_ARB = 0x889A;
const uint GL_EDGE_FLAG_ARRAY_BUFFER_BINDING_ARB = 0x889B;
const uint GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING_ARB = 0x889C;
const uint GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING_ARB = 0x889D;
const uint GL_WEIGHT_ARRAY_BUFFER_BINDING_ARB = 0x889E;
const uint GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING_ARB = 0x889F;
const uint GL_STREAM_DRAW_ARB                = 0x88E0;
const uint GL_STREAM_READ_ARB                = 0x88E1;
const uint GL_STREAM_COPY_ARB                = 0x88E2;
const uint GL_STATIC_DRAW_ARB                = 0x88E4;
const uint GL_STATIC_READ_ARB                = 0x88E5;
const uint GL_STATIC_COPY_ARB                = 0x88E6;
const uint GL_DYNAMIC_DRAW_ARB               = 0x88E8;
const uint GL_DYNAMIC_READ_ARB               = 0x88E9;
const uint GL_DYNAMIC_COPY_ARB               = 0x88EA;
const uint GL_READ_ONLY_ARB                  = 0x88B8;
const uint GL_WRITE_ONLY_ARB                 = 0x88B9;
const uint GL_READ_WRITE_ARB                 = 0x88BA;
const uint GL_BUFFER_SIZE_ARB                = 0x8764;
const uint GL_BUFFER_USAGE_ARB               = 0x8765;
const uint GL_BUFFER_ACCESS_ARB              = 0x88BB;
const uint GL_BUFFER_MAPPED_ARB              = 0x88BC;
const uint GL_BUFFER_MAP_POINTER_ARB         = 0x88BD;

const uint GL_ARB_vertex_program = 1;
const uint GL_VERTEX_PROGRAM_ARB                       = 0x8620;
const uint GL_VERTEX_PROGRAM_POINT_SIZE_ARB            = 0x8642;
const uint GL_VERTEX_PROGRAM_TWO_SIDE_ARB              = 0x8643;
const uint GL_COLOR_SUM_ARB                            = 0x8458;
const uint GL_PROGRAM_FORMAT_ASCII_ARB                 = 0x8875;
const uint GL_VERTEX_ATTRIB_ARRAY_ENABLED_ARB          = 0x8622;
const uint GL_VERTEX_ATTRIB_ARRAY_SIZE_ARB             = 0x8623;
const uint GL_VERTEX_ATTRIB_ARRAY_STRIDE_ARB           = 0x8624;
const uint GL_VERTEX_ATTRIB_ARRAY_TYPE_ARB             = 0x8625;
const uint GL_VERTEX_ATTRIB_ARRAY_NORMALIZED_ARB       = 0x886A;
const uint GL_CURRENT_VERTEX_ATTRIB_ARB                = 0x8626;
const uint GL_VERTEX_ATTRIB_ARRAY_POINTER_ARB          = 0x8645;
const uint GL_PROGRAM_LENGTH_ARB                       = 0x8627;
const uint GL_PROGRAM_FORMAT_ARB                       = 0x8876;
const uint GL_PROGRAM_BINDING_ARB                      = 0x8677;
const uint GL_PROGRAM_INSTRUCTIONS_ARB                 = 0x88A0;
const uint GL_MAX_PROGRAM_INSTRUCTIONS_ARB             = 0x88A1;
const uint GL_PROGRAM_NATIVE_INSTRUCTIONS_ARB          = 0x88A2;
const uint GL_MAX_PROGRAM_NATIVE_INSTRUCTIONS_ARB      = 0x88A3;
const uint GL_PROGRAM_TEMPORARIES_ARB                  = 0x88A4;
const uint GL_MAX_PROGRAM_TEMPORARIES_ARB              = 0x88A5;
const uint GL_PROGRAM_NATIVE_TEMPORARIES_ARB           = 0x88A6;
const uint GL_MAX_PROGRAM_NATIVE_TEMPORARIES_ARB       = 0x88A7;
const uint GL_PROGRAM_PARAMETERS_ARB                   = 0x88A8;
const uint GL_MAX_PROGRAM_PARAMETERS_ARB               = 0x88A9;
const uint GL_PROGRAM_NATIVE_PARAMETERS_ARB            = 0x88AA;
const uint GL_MAX_PROGRAM_NATIVE_PARAMETERS_ARB        = 0x88AB;
const uint GL_PROGRAM_ATTRIBS_ARB                      = 0x88AC;
const uint GL_MAX_PROGRAM_ATTRIBS_ARB                  = 0x88AD;
const uint GL_PROGRAM_NATIVE_ATTRIBS_ARB               = 0x88AE;
const uint GL_MAX_PROGRAM_NATIVE_ATTRIBS_ARB           = 0x88AF;
const uint GL_PROGRAM_ADDRESS_REGISTERS_ARB            = 0x88B0;
const uint GL_MAX_PROGRAM_ADDRESS_REGISTERS_ARB        = 0x88B1;
const uint GL_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB     = 0x88B2;
const uint GL_MAX_PROGRAM_NATIVE_ADDRESS_REGISTERS_ARB = 0x88B3;
const uint GL_MAX_PROGRAM_LOCAL_PARAMETERS_ARB         = 0x88B4;
const uint GL_MAX_PROGRAM_ENV_PARAMETERS_ARB           = 0x88B5;
const uint GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB          = 0x88B6;
const uint GL_PROGRAM_STRING_ARB                       = 0x8628;
const uint GL_PROGRAM_ERROR_POSITION_ARB               = 0x864B;
const uint GL_CURRENT_MATRIX_ARB                       = 0x8641;
const uint GL_TRANSPOSE_CURRENT_MATRIX_ARB             = 0x88B7;
const uint GL_CURRENT_MATRIX_STACK_DEPTH_ARB           = 0x8640;
const uint GL_MAX_VERTEX_ATTRIBS_ARB                   = 0x8869;
const uint GL_MAX_PROGRAM_MATRICES_ARB                 = 0x862F;
const uint GL_MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB       = 0x862E;
const uint GL_PROGRAM_ERROR_STRING_ARB                 = 0x8874;
const uint GL_MATRIX0_ARB                              = 0x88C0;
const uint GL_MATRIX1_ARB                              = 0x88C1;
const uint GL_MATRIX2_ARB                              = 0x88C2;
const uint GL_MATRIX3_ARB                              = 0x88C3;
const uint GL_MATRIX4_ARB                              = 0x88C4;
const uint GL_MATRIX5_ARB                              = 0x88C5;
const uint GL_MATRIX6_ARB                              = 0x88C6;
const uint GL_MATRIX7_ARB                              = 0x88C7;
const uint GL_MATRIX8_ARB                              = 0x88C8;
const uint GL_MATRIX9_ARB                              = 0x88C9;
const uint GL_MATRIX10_ARB                             = 0x88CA;
const uint GL_MATRIX11_ARB                             = 0x88CB;
const uint GL_MATRIX12_ARB                             = 0x88CC;
const uint GL_MATRIX13_ARB                             = 0x88CD;
const uint GL_MATRIX14_ARB                             = 0x88CE;
const uint GL_MATRIX15_ARB                             = 0x88CF;
const uint GL_MATRIX16_ARB                             = 0x88D0;
const uint GL_MATRIX17_ARB                             = 0x88D1;
const uint GL_MATRIX18_ARB                             = 0x88D2;
const uint GL_MATRIX19_ARB                             = 0x88D3;
const uint GL_MATRIX20_ARB                             = 0x88D4;
const uint GL_MATRIX21_ARB                             = 0x88D5;
const uint GL_MATRIX22_ARB                             = 0x88D6;
const uint GL_MATRIX23_ARB                             = 0x88D7;
const uint GL_MATRIX24_ARB                             = 0x88D8;
const uint GL_MATRIX25_ARB                             = 0x88D9;
const uint GL_MATRIX26_ARB                             = 0x88DA;
const uint GL_MATRIX27_ARB                             = 0x88DB;
const uint GL_MATRIX28_ARB                             = 0x88DC;
const uint GL_MATRIX29_ARB                             = 0x88DD;
const uint GL_MATRIX30_ARB                             = 0x88DE;
const uint GL_MATRIX31_ARB                             = 0x88DF;

const uint GL_ARB_window_pos = 1;



/*-----------------------------------------------------------------------
 * EXT
 *----------------------------------------------------------------------*/

const uint GL_EXT_422_pixels = 1;
const uint GL_422_EXT                        = 0x80CC;
const uint GL_422_REV_EXT                    = 0x80CD;
const uint GL_422_AVERAGE_EXT                = 0x80CE;
const uint GL_422_REV_AVERAGE_EXT            = 0x80CF;

const uint GL_EXT_abgr = 1;
const uint GL_ABGR_EXT                       = 0x8000;

const uint GL_EXT_bgra = 1;
const uint GL_BGR_EXT                        = 0x80E0;
const uint GL_BGRA_EXT                       = 0x80E1;

const uint GL_EXT_blend_color = 1;
const uint GL_CONSTANT_COLOR_EXT             = 0x8001;
const uint GL_ONE_MINUS_CONSTANT_COLOR_EXT   = 0x8002;
const uint GL_CONSTANT_ALPHA_EXT             = 0x8003;
const uint GL_ONE_MINUS_CONSTANT_ALPHA_EXT   = 0x8004;
const uint GL_BLEND_COLOR_EXT                = 0x8005;

const uint GL_EXT_blend_func_separate = 1;
const uint GL_BLEND_DST_RGB_EXT              = 0x80C8;
const uint GL_BLEND_SRC_RGB_EXT              = 0x80C9;
const uint GL_BLEND_DST_ALPHA_EXT            = 0x80CA;
const uint GL_BLEND_SRC_ALPHA_EXT            = 0x80CB;

const uint GL_EXT_blend_logic_op = 1;

const uint GL_EXT_blend_minmax = 1;
const uint GL_FUNC_ADD_EXT                   = 0x8006;
const uint GL_MIN_EXT                        = 0x8007;
const uint GL_MAX_EXT                        = 0x8008;
const uint GL_BLEND_EQUATION_EXT             = 0x8009;

const uint GL_EXT_blend_subtract = 1;
const uint GL_FUNC_SUBTRACT_EXT              = 0x800A;
const uint GL_FUNC_REVERSE_SUBTRACT_EXT      = 0x800B;

const uint GL_EXT_clip_volume_hint = 1;
const uint GL_CLIP_VOLUME_CLIPPING_HINT_EXT  = 0x80F0;

const uint GL_EXT_cmyka = 1;
const uint GL_CMYK_EXT                       = 0x800C;
const uint GL_CMYKA_EXT                      = 0x800D;
const uint GL_PACK_CMYK_HINT_EXT             = 0x800E;
const uint GL_UNPACK_CMYK_HINT_EXT           = 0x800F;

const uint GL_EXT_color_subtable = 1;

const uint GL_EXT_compiled_vertex_array = 1;
const uint GL_ARRAY_ELEMENT_LOCK_FIRST_EXT   = 0x81A8;
const uint GL_ARRAY_ELEMENT_LOCK_COUNT_EXT   = 0x81A9;

const uint GL_EXT_convolution = 1;
const uint GL_CONVOLUTION_1D_EXT             = 0x8010;
const uint GL_CONVOLUTION_2D_EXT             = 0x8011;
const uint GL_SEPARABLE_2D_EXT               = 0x8012;
const uint GL_CONVOLUTION_BORDER_MODE_EXT    = 0x8013;
const uint GL_CONVOLUTION_FILTER_SCALE_EXT   = 0x8014;
const uint GL_CONVOLUTION_FILTER_BIAS_EXT    = 0x8015;
const uint GL_REDUCE_EXT                     = 0x8016;
const uint GL_CONVOLUTION_FORMAT_EXT         = 0x8017;
const uint GL_CONVOLUTION_WIDTH_EXT          = 0x8018;
const uint GL_CONVOLUTION_HEIGHT_EXT         = 0x8019;
const uint GL_MAX_CONVOLUTION_WIDTH_EXT      = 0x801A;
const uint GL_MAX_CONVOLUTION_HEIGHT_EXT     = 0x801B;
const uint GL_POST_CONVOLUTION_RED_SCALE_EXT = 0x801C;
const uint GL_POST_CONVOLUTION_GREEN_SCALE_EXT = 0x801D;
const uint GL_POST_CONVOLUTION_BLUE_SCALE_EXT = 0x801E;
const uint GL_POST_CONVOLUTION_ALPHA_SCALE_EXT = 0x801F;
const uint GL_POST_CONVOLUTION_RED_BIAS_EXT  = 0x8020;
const uint GL_POST_CONVOLUTION_GREEN_BIAS_EXT = 0x8021;
const uint GL_POST_CONVOLUTION_BLUE_BIAS_EXT = 0x8022;
const uint GL_POST_CONVOLUTION_ALPHA_BIAS_EXT = 0x8023;

const uint GL_EXT_coordinate_frame = 1;
const uint GL_TANGENT_ARRAY_EXT              = 0x8439;
const uint GL_BINORMAL_ARRAY_EXT             = 0x843A;
const uint GL_CURRENT_TANGENT_EXT            = 0x843B;
const uint GL_CURRENT_BINORMAL_EXT           = 0x843C;
const uint GL_TANGENT_ARRAY_TYPE_EXT         = 0x843E;
const uint GL_TANGENT_ARRAY_STRIDE_EXT       = 0x843F;
const uint GL_BINORMAL_ARRAY_TYPE_EXT        = 0x8440;
const uint GL_BINORMAL_ARRAY_STRIDE_EXT      = 0x8441;
const uint GL_TANGENT_ARRAY_POINTER_EXT      = 0x8442;
const uint GL_BINORMAL_ARRAY_POINTER_EXT     = 0x8443;
const uint GL_MAP1_TANGENT_EXT               = 0x8444;
const uint GL_MAP2_TANGENT_EXT               = 0x8445;
const uint GL_MAP1_BINORMAL_EXT              = 0x8446;
const uint GL_MAP2_BINORMAL_EXT              = 0x8447;

const uint GL_EXT_copy_texture = 1;

const uint GL_EXT_cull_vertex = 1;
const uint GL_CULL_VERTEX_EXT                = 0x81AA;
const uint GL_CULL_VERTEX_EYE_POSITION_EXT   = 0x81AB;
const uint GL_CULL_VERTEX_OBJECT_POSITION_EXT = 0x81AC;

const uint GL_EXT_draw_range_elements = 1;
const uint GL_MAX_ELEMENTS_VERTICES_EXT      = 0x80E8;
const uint GL_MAX_ELEMENTS_INDICES_EXT       = 0x80E9;

const uint GL_EXT_fog_coord = 1;
const uint GL_FOG_COORDINATE_SOURCE_EXT      = 0x8450;
const uint GL_FOG_COORDINATE_EXT             = 0x8451;
const uint GL_FRAGMENT_DEPTH_EXT             = 0x8452;
const uint GL_CURRENT_FOG_COORDINATE_EXT     = 0x8453;
const uint GL_FOG_COORDINATE_ARRAY_TYPE_EXT  = 0x8454;
const uint GL_FOG_COORDINATE_ARRAY_STRIDE_EXT = 0x8455;
const uint GL_FOG_COORDINATE_ARRAY_POINTER_EXT = 0x8456;
const uint GL_FOG_COORDINATE_ARRAY_EXT       = 0x8457;

const uint GL_EXT_histogram = 1;
const uint GL_HISTOGRAM_EXT                  = 0x8024;
const uint GL_PROXY_HISTOGRAM_EXT            = 0x8025;
const uint GL_HISTOGRAM_WIDTH_EXT            = 0x8026;
const uint GL_HISTOGRAM_FORMAT_EXT           = 0x8027;
const uint GL_HISTOGRAM_RED_SIZE_EXT         = 0x8028;
const uint GL_HISTOGRAM_GREEN_SIZE_EXT       = 0x8029;
const uint GL_HISTOGRAM_BLUE_SIZE_EXT        = 0x802A;
const uint GL_HISTOGRAM_ALPHA_SIZE_EXT       = 0x802B;
const uint GL_HISTOGRAM_LUMINANCE_SIZE_EXT   = 0x802C;
const uint GL_HISTOGRAM_SINK_EXT             = 0x802D;
const uint GL_MINMAX_EXT                     = 0x802E;
const uint GL_MINMAX_FORMAT_EXT              = 0x802F;
const uint GL_MINMAX_SINK_EXT                = 0x8030;
const uint GL_TABLE_TOO_LARGE_EXT            = 0x8031;

const uint GL_EXT_index_array_formats = 1;
const uint GL_IUI_V2F_EXT                    = 0x81AD;
const uint GL_IUI_V3F_EXT                    = 0x81AE;
const uint GL_IUI_N3F_V2F_EXT                = 0x81AF;
const uint GL_IUI_N3F_V3F_EXT                = 0x81B0;
const uint GL_T2F_IUI_V2F_EXT                = 0x81B1;
const uint GL_T2F_IUI_V3F_EXT                = 0x81B2;
const uint GL_T2F_IUI_N3F_V2F_EXT            = 0x81B3;
const uint GL_T2F_IUI_N3F_V3F_EXT            = 0x81B4;

const uint GL_EXT_index_func = 1;
const uint GL_INDEX_TEST_EXT                 = 0x81B5;
const uint GL_INDEX_TEST_FUNC_EXT            = 0x81B6;
const uint GL_INDEX_TEST_REF_EXT             = 0x81B7;

const uint GL_EXT_index_material = 1;
const uint GL_INDEX_MATERIAL_EXT             = 0x81B8;
const uint GL_INDEX_MATERIAL_PARAMETER_EXT   = 0x81B9;
const uint GL_INDEX_MATERIAL_FACE_EXT        = 0x81BA;

const uint GL_EXT_index_texture = 1;

const uint GL_EXT_light_texture = 1;
const uint GL_FRAGMENT_MATERIAL_EXT          = 0x8349;
const uint GL_FRAGMENT_NORMAL_EXT            = 0x834A;
const uint GL_FRAGMENT_COLOR_EXT             = 0x834C;
const uint GL_ATTENUATION_EXT                = 0x834D;
const uint GL_SHADOW_ATTENUATION_EXT         = 0x834E;
const uint GL_TEXTURE_APPLICATION_MODE_EXT   = 0x834F;
const uint GL_TEXTURE_LIGHT_EXT              = 0x8350;
const uint GL_TEXTURE_MATERIAL_FACE_EXT      = 0x8351;
const uint GL_TEXTURE_MATERIAL_PARAMETER_EXT = 0x8352;
/* reuse GL_FRAGMENT_DEPTH_EXT */

const uint GL_EXT_misc_attribute = 1;

const uint GL_EXT_multi_draw_arrays = 1;

const uint GL_EXT_multisample = 1;
const uint GL_MULTISAMPLE_EXT                = 0x809D;
const uint GL_SAMPLE_ALPHA_TO_MASK_EXT       = 0x809E;
const uint GL_SAMPLE_ALPHA_TO_ONE_EXT        = 0x809F;
const uint GL_SAMPLE_MASK_EXT                = 0x80A0;
const uint GL_1PASS_EXT                      = 0x80A1;
const uint GL_2PASS_0_EXT                    = 0x80A2;
const uint GL_2PASS_1_EXT                    = 0x80A3;
const uint GL_4PASS_0_EXT                    = 0x80A4;
const uint GL_4PASS_1_EXT                    = 0x80A5;
const uint GL_4PASS_2_EXT                    = 0x80A6;
const uint GL_4PASS_3_EXT                    = 0x80A7;
const uint GL_SAMPLE_BUFFERS_EXT             = 0x80A8;
const uint GL_SAMPLES_EXT                    = 0x80A9;
const uint GL_SAMPLE_MASK_VALUE_EXT          = 0x80AA;
const uint GL_SAMPLE_MASK_INVERT_EXT         = 0x80AB;
const uint GL_SAMPLE_PATTERN_EXT             = 0x80AC;
const uint GL_MULTISAMPLE_BIT_EXT            = 0x20000000;

const uint GL_EXT_packed_pixels = 1;
const uint GL_UNSIGNED_BYTE_3_3_2_EXT        = 0x8032;
const uint GL_UNSIGNED_SHORT_4_4_4_4_EXT     = 0x8033;
const uint GL_UNSIGNED_SHORT_5_5_5_1_EXT     = 0x8034;
const uint GL_UNSIGNED_INT_8_8_8_8_EXT       = 0x8035;
const uint GL_UNSIGNED_INT_10_10_10_2_EXT    = 0x8036;

const uint GL_EXT_paletted_texture = 1;
const uint GL_COLOR_INDEX1_EXT               = 0x80E2;
const uint GL_COLOR_INDEX2_EXT               = 0x80E3;
const uint GL_COLOR_INDEX4_EXT               = 0x80E4;
const uint GL_COLOR_INDEX8_EXT               = 0x80E5;
const uint GL_COLOR_INDEX12_EXT              = 0x80E6;
const uint GL_COLOR_INDEX16_EXT              = 0x80E7;
const uint GL_TEXTURE_INDEX_SIZE_EXT         = 0x80ED;

const uint GL_EXT_pixel_transform = 1;
const uint GL_PIXEL_TRANSFORM_2D_EXT         = 0x8330;
const uint GL_PIXEL_MAG_FILTER_EXT           = 0x8331;
const uint GL_PIXEL_MIN_FILTER_EXT           = 0x8332;
const uint GL_PIXEL_CUBIC_WEIGHT_EXT         = 0x8333;
const uint GL_CUBIC_EXT                      = 0x8334;
const uint GL_AVERAGE_EXT                    = 0x8335;
const uint GL_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT = 0x8336;
const uint GL_MAX_PIXEL_TRANSFORM_2D_STACK_DEPTH_EXT = 0x8337;
const uint GL_PIXEL_TRANSFORM_2D_MATRIX_EXT  = 0x8338;

const uint GL_EXT_pixel_transform_color_table = 1;

const uint GL_EXT_polygon_offset = 1;
const uint GL_POLYGON_OFFSET_EXT             = 0x8037;
const uint GL_POLYGON_OFFSET_FACTOR_EXT      = 0x8038;
const uint GL_POLYGON_OFFSET_BIAS_EXT        = 0x8039;

const uint GL_EXT_rescale_normal = 1;
const uint GL_RESCALE_NORMAL_EXT             = 0x803A;

const uint GL_EXT_secondary_color = 1;
const uint GL_COLOR_SUM_EXT                  = 0x8458;
const uint GL_CURRENT_SECONDARY_COLOR_EXT    = 0x8459;
const uint GL_SECONDARY_COLOR_ARRAY_SIZE_EXT = 0x845A;
const uint GL_SECONDARY_COLOR_ARRAY_TYPE_EXT = 0x845B;
const uint GL_SECONDARY_COLOR_ARRAY_STRIDE_EXT = 0x845C;
const uint GL_SECONDARY_COLOR_ARRAY_POINTER_EXT = 0x845D;
const uint GL_SECONDARY_COLOR_ARRAY_EXT      = 0x845E;

const uint GL_EXT_separate_specular_color = 1;
const uint GL_LIGHT_MODEL_COLOR_CONTROL_EXT  = 0x81F8;
const uint GL_SINGLE_COLOR_EXT               = 0x81F9;
const uint GL_SEPARATE_SPECULAR_COLOR_EXT    = 0x81FA;

const uint GL_EXT_shadow_funcs = 1;

const uint GL_EXT_shared_texture_palette = 1;
const uint GL_SHARED_TEXTURE_PALETTE_EXT     = 0x81FB;

const uint GL_EXT_stencil_two_side = 1;
const uint GL_STENCIL_TEST_TWO_SIDE_EXT      = 0x8910;
const uint GL_ACTIVE_STENCIL_FACE_EXT        = 0x8911;

const uint GL_EXT_stencil_wrap = 1;
const uint GL_INCR_WRAP_EXT                  = 0x8507;
const uint GL_DECR_WRAP_EXT                  = 0x8508;

const uint GL_EXT_subtexture = 1;

const uint GL_EXT_texture = 1;
const uint GL_ALPHA4_EXT                     = 0x803B;
const uint GL_ALPHA8_EXT                     = 0x803C;
const uint GL_ALPHA12_EXT                    = 0x803D;
const uint GL_ALPHA16_EXT                    = 0x803E;
const uint GL_LUMINANCE4_EXT                 = 0x803F;
const uint GL_LUMINANCE8_EXT                 = 0x8040;
const uint GL_LUMINANCE12_EXT                = 0x8041;
const uint GL_LUMINANCE16_EXT                = 0x8042;
const uint GL_LUMINANCE4_ALPHA4_EXT          = 0x8043;
const uint GL_LUMINANCE6_ALPHA2_EXT          = 0x8044;
const uint GL_LUMINANCE8_ALPHA8_EXT          = 0x8045;
const uint GL_LUMINANCE12_ALPHA4_EXT         = 0x8046;
const uint GL_LUMINANCE12_ALPHA12_EXT        = 0x8047;
const uint GL_LUMINANCE16_ALPHA16_EXT        = 0x8048;
const uint GL_INTENSITY_EXT                  = 0x8049;
const uint GL_INTENSITY4_EXT                 = 0x804A;
const uint GL_INTENSITY8_EXT                 = 0x804B;
const uint GL_INTENSITY12_EXT                = 0x804C;
const uint GL_INTENSITY16_EXT                = 0x804D;
const uint GL_RGB2_EXT                       = 0x804E;
const uint GL_RGB4_EXT                       = 0x804F;
const uint GL_RGB5_EXT                       = 0x8050;
const uint GL_RGB8_EXT                       = 0x8051;
const uint GL_RGB10_EXT                      = 0x8052;
const uint GL_RGB12_EXT                      = 0x8053;
const uint GL_RGB16_EXT                      = 0x8054;
const uint GL_RGBA2_EXT                      = 0x8055;
const uint GL_RGBA4_EXT                      = 0x8056;
const uint GL_RGB5_A1_EXT                    = 0x8057;
const uint GL_RGBA8_EXT                      = 0x8058;
const uint GL_RGB10_A2_EXT                   = 0x8059;
const uint GL_RGBA12_EXT                     = 0x805A;
const uint GL_RGBA16_EXT                     = 0x805B;
const uint GL_TEXTURE_RED_SIZE_EXT           = 0x805C;
const uint GL_TEXTURE_GREEN_SIZE_EXT         = 0x805D;
const uint GL_TEXTURE_BLUE_SIZE_EXT          = 0x805E;
const uint GL_TEXTURE_ALPHA_SIZE_EXT         = 0x805F;
const uint GL_TEXTURE_LUMINANCE_SIZE_EXT     = 0x8060;
const uint GL_TEXTURE_INTENSITY_SIZE_EXT     = 0x8061;
const uint GL_REPLACE_EXT                    = 0x8062;
const uint GL_PROXY_TEXTURE_1D_EXT           = 0x8063;
const uint GL_PROXY_TEXTURE_2D_EXT           = 0x8064;
const uint GL_TEXTURE_TOO_LARGE_EXT          = 0x8065;

const uint GL_EXT_texture_compression_s3tc = 1;
const uint GL_COMPRESSED_RGB_S3TC_DXT1_EXT   = 0x83F0;
const uint GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  = 0x83F1;
const uint GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  = 0x83F2;
const uint GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  = 0x83F3;

const uint GL_EXT_texture_cube_map = 1;
const uint GL_NORMAL_MAP_EXT                 = 0x8511;
const uint GL_REFLECTION_MAP_EXT             = 0x8512;
const uint GL_TEXTURE_CUBE_MAP_EXT           = 0x8513;
const uint GL_TEXTURE_BINDING_CUBE_MAP_EXT   = 0x8514;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_X_EXT = 0x8515;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_X_EXT = 0x8516;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_Y_EXT = 0x8517;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_EXT = 0x8518;
const uint GL_TEXTURE_CUBE_MAP_POSITIVE_Z_EXT = 0x8519;
const uint GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_EXT = 0x851A;
const uint GL_PROXY_TEXTURE_CUBE_MAP_EXT     = 0x851B;
const uint GL_MAX_CUBE_MAP_TEXTURE_SIZE_EXT  = 0x851C;

const uint GL_EXT_texture_env_add = 1;

const uint GL_EXT_texture_env_combine = 1;
const uint GL_COMBINE_EXT                    = 0x8570;
const uint GL_COMBINE_RGB_EXT                = 0x8571;
const uint GL_COMBINE_ALPHA_EXT              = 0x8572;
const uint GL_RGB_SCALE_EXT                  = 0x8573;
const uint GL_ADD_SIGNED_EXT                 = 0x8574;
const uint GL_INTERPOLATE_EXT                = 0x8575;
const uint GL_CONSTANT_EXT                   = 0x8576;
const uint GL_PRIMARY_COLOR_EXT              = 0x8577;
const uint GL_PREVIOUS_EXT                   = 0x8578;
const uint GL_SOURCE0_RGB_EXT                = 0x8580;
const uint GL_SOURCE1_RGB_EXT                = 0x8581;
const uint GL_SOURCE2_RGB_EXT                = 0x8582;
const uint GL_SOURCE0_ALPHA_EXT              = 0x8588;
const uint GL_SOURCE1_ALPHA_EXT              = 0x8589;
const uint GL_SOURCE2_ALPHA_EXT              = 0x858A;
const uint GL_OPERAND0_RGB_EXT               = 0x8590;
const uint GL_OPERAND1_RGB_EXT               = 0x8591;
const uint GL_OPERAND2_RGB_EXT               = 0x8592;
const uint GL_OPERAND0_ALPHA_EXT             = 0x8598;
const uint GL_OPERAND1_ALPHA_EXT             = 0x8599;
const uint GL_OPERAND2_ALPHA_EXT             = 0x859A;

const uint GL_EXT_texture_env_dot3 = 1;
const uint GL_DOT3_RGB_EXT                   = 0x8740;
const uint GL_DOT3_RGBA_EXT                  = 0x8741;

const uint GL_EXT_texture_filter_anisotropic = 1;
const uint GL_TEXTURE_MAX_ANISOTROPY_EXT     = 0x84FE;
const uint GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

const uint GL_EXT_texture_lod_bias = 1;
const uint GL_MAX_TEXTURE_LOD_BIAS_EXT       = 0x84FD;
const uint GL_TEXTURE_FILTER_CONTROL_EXT     = 0x8500;
const uint GL_TEXTURE_LOD_BIAS_EXT           = 0x8501;

const uint GL_EXT_texture_object = 1;
const uint GL_TEXTURE_PRIORITY_EXT           = 0x8066;
const uint GL_TEXTURE_RESIDENT_EXT           = 0x8067;
const uint GL_TEXTURE_1D_BINDING_EXT         = 0x8068;
const uint GL_TEXTURE_2D_BINDING_EXT         = 0x8069;
const uint GL_TEXTURE_3D_BINDING_EXT         = 0x806A;

const uint GL_EXT_texture_perturb_normal = 1;
const uint GL_PERTURB_EXT                    = 0x85AE;
const uint GL_TEXTURE_NORMAL_EXT             = 0x85AF;

const uint GL_EXT_texture3D = 1;
const uint GL_PACK_SKIP_IMAGES_EXT           = 0x806B;
const uint GL_PACK_IMAGE_HEIGHT_EXT          = 0x806C;
const uint GL_UNPACK_SKIP_IMAGES_EXT         = 0x806D;
const uint GL_UNPACK_IMAGE_HEIGHT_EXT        = 0x806E;
const uint GL_TEXTURE_3D_EXT                 = 0x806F;
const uint GL_PROXY_TEXTURE_3D_EXT           = 0x8070;
const uint GL_TEXTURE_DEPTH_EXT              = 0x8071;
const uint GL_TEXTURE_WRAP_R_EXT             = 0x8072;
const uint GL_MAX_3D_TEXTURE_SIZE_EXT        = 0x8073;

const uint GL_EXT_vertex_array = 1;
const uint GL_VERTEX_ARRAY_EXT               = 0x8074;
const uint GL_NORMAL_ARRAY_EXT               = 0x8075;
const uint GL_COLOR_ARRAY_EXT                = 0x8076;
const uint GL_INDEX_ARRAY_EXT                = 0x8077;
const uint GL_TEXTURE_COORD_ARRAY_EXT        = 0x8078;
const uint GL_EDGE_FLAG_ARRAY_EXT            = 0x8079;
const uint GL_VERTEX_ARRAY_SIZE_EXT          = 0x807A;
const uint GL_VERTEX_ARRAY_TYPE_EXT          = 0x807B;
const uint GL_VERTEX_ARRAY_STRIDE_EXT        = 0x807C;
const uint GL_VERTEX_ARRAY_COUNT_EXT         = 0x807D;
const uint GL_NORMAL_ARRAY_TYPE_EXT          = 0x807E;
const uint GL_NORMAL_ARRAY_STRIDE_EXT        = 0x807F;
const uint GL_NORMAL_ARRAY_COUNT_EXT         = 0x8080;
const uint GL_COLOR_ARRAY_SIZE_EXT           = 0x8081;
const uint GL_COLOR_ARRAY_TYPE_EXT           = 0x8082;
const uint GL_COLOR_ARRAY_STRIDE_EXT         = 0x8083;
const uint GL_COLOR_ARRAY_COUNT_EXT          = 0x8084;
const uint GL_INDEX_ARRAY_TYPE_EXT           = 0x8085;
const uint GL_INDEX_ARRAY_STRIDE_EXT         = 0x8086;
const uint GL_INDEX_ARRAY_COUNT_EXT          = 0x8087;
const uint GL_TEXTURE_COORD_ARRAY_SIZE_EXT   = 0x8088;
const uint GL_TEXTURE_COORD_ARRAY_TYPE_EXT   = 0x8089;
const uint GL_TEXTURE_COORD_ARRAY_STRIDE_EXT = 0x808A;
const uint GL_TEXTURE_COORD_ARRAY_COUNT_EXT  = 0x808B;
const uint GL_EDGE_FLAG_ARRAY_STRIDE_EXT     = 0x808C;
const uint GL_EDGE_FLAG_ARRAY_COUNT_EXT      = 0x808D;
const uint GL_VERTEX_ARRAY_POINTER_EXT       = 0x808E;
const uint GL_NORMAL_ARRAY_POINTER_EXT       = 0x808F;
const uint GL_COLOR_ARRAY_POINTER_EXT        = 0x8090;
const uint GL_INDEX_ARRAY_POINTER_EXT        = 0x8091;
const uint GL_TEXTURE_COORD_ARRAY_POINTER_EXT = 0x8092;
const uint GL_EDGE_FLAG_ARRAY_POINTER_EXT    = 0x8093;

const uint GL_EXT_vertex_shader = 1;
const uint GL_VERTEX_SHADER_EXT              = 0x8780;
const uint GL_VERTEX_SHADER_BINDING_EXT      = 0x8781;
const uint GL_OP_INDEX_EXT                   = 0x8782;
const uint GL_OP_NEGATE_EXT                  = 0x8783;
const uint GL_OP_DOT3_EXT                    = 0x8784;
const uint GL_OP_DOT4_EXT                    = 0x8785;
const uint GL_OP_MUL_EXT                     = 0x8786;
const uint GL_OP_ADD_EXT                     = 0x8787;
const uint GL_OP_MADD_EXT                    = 0x8788;
const uint GL_OP_FRAC_EXT                    = 0x8789;
const uint GL_OP_MAX_EXT                     = 0x878A;
const uint GL_OP_MIN_EXT                     = 0x878B;
const uint GL_OP_SET_GE_EXT                  = 0x878C;
const uint GL_OP_SET_LT_EXT                  = 0x878D;
const uint GL_OP_CLAMP_EXT                   = 0x878E;
const uint GL_OP_FLOOR_EXT                   = 0x878F;
const uint GL_OP_ROUND_EXT                   = 0x8790;
const uint GL_OP_EXP_BASE_2_EXT              = 0x8791;
const uint GL_OP_LOG_BASE_2_EXT              = 0x8792;
const uint GL_OP_POWER_EXT                   = 0x8793;
const uint GL_OP_RECIP_EXT                   = 0x8794;
const uint GL_OP_RECIP_SQRT_EXT              = 0x8795;
const uint GL_OP_SUB_EXT                     = 0x8796;
const uint GL_OP_CROSS_PRODUCT_EXT           = 0x8797;
const uint GL_OP_MULTIPLY_MATRIX_EXT         = 0x8798;
const uint GL_OP_MOV_EXT                     = 0x8799;
const uint GL_OUTPUT_VERTEX_EXT              = 0x879A;
const uint GL_OUTPUT_COLOR0_EXT              = 0x879B;
const uint GL_OUTPUT_COLOR1_EXT              = 0x879C;
const uint GL_OUTPUT_TEXTURE_COORD0_EXT      = 0x879D;
const uint GL_OUTPUT_TEXTURE_COORD1_EXT      = 0x879E;
const uint GL_OUTPUT_TEXTURE_COORD2_EXT      = 0x879F;
const uint GL_OUTPUT_TEXTURE_COORD3_EXT      = 0x87A0;
const uint GL_OUTPUT_TEXTURE_COORD4_EXT      = 0x87A1;
const uint GL_OUTPUT_TEXTURE_COORD5_EXT      = 0x87A2;
const uint GL_OUTPUT_TEXTURE_COORD6_EXT      = 0x87A3;
const uint GL_OUTPUT_TEXTURE_COORD7_EXT      = 0x87A4;
const uint GL_OUTPUT_TEXTURE_COORD8_EXT      = 0x87A5;
const uint GL_OUTPUT_TEXTURE_COORD9_EXT      = 0x87A6;
const uint GL_OUTPUT_TEXTURE_COORD10_EXT     = 0x87A7;
const uint GL_OUTPUT_TEXTURE_COORD11_EXT     = 0x87A8;
const uint GL_OUTPUT_TEXTURE_COORD12_EXT     = 0x87A9;
const uint GL_OUTPUT_TEXTURE_COORD13_EXT     = 0x87AA;
const uint GL_OUTPUT_TEXTURE_COORD14_EXT     = 0x87AB;
const uint GL_OUTPUT_TEXTURE_COORD15_EXT     = 0x87AC;
const uint GL_OUTPUT_TEXTURE_COORD16_EXT     = 0x87AD;
const uint GL_OUTPUT_TEXTURE_COORD17_EXT     = 0x87AE;
const uint GL_OUTPUT_TEXTURE_COORD18_EXT     = 0x87AF;
const uint GL_OUTPUT_TEXTURE_COORD19_EXT     = 0x87B0;
const uint GL_OUTPUT_TEXTURE_COORD20_EXT     = 0x87B1;
const uint GL_OUTPUT_TEXTURE_COORD21_EXT     = 0x87B2;
const uint GL_OUTPUT_TEXTURE_COORD22_EXT     = 0x87B3;
const uint GL_OUTPUT_TEXTURE_COORD23_EXT     = 0x87B4;
const uint GL_OUTPUT_TEXTURE_COORD24_EXT     = 0x87B5;
const uint GL_OUTPUT_TEXTURE_COORD25_EXT     = 0x87B6;
const uint GL_OUTPUT_TEXTURE_COORD26_EXT     = 0x87B7;
const uint GL_OUTPUT_TEXTURE_COORD27_EXT     = 0x87B8;
const uint GL_OUTPUT_TEXTURE_COORD28_EXT     = 0x87B9;
const uint GL_OUTPUT_TEXTURE_COORD29_EXT     = 0x87BA;
const uint GL_OUTPUT_TEXTURE_COORD30_EXT     = 0x87BB;
const uint GL_OUTPUT_TEXTURE_COORD31_EXT     = 0x87BC;
const uint GL_OUTPUT_FOG_EXT                 = 0x87BD;
const uint GL_SCALAR_EXT                     = 0x87BE;
const uint GL_VECTOR_EXT                     = 0x87BF;
const uint GL_MATRIX_EXT                     = 0x87C0;
const uint GL_VARIANT_EXT                    = 0x87C1;
const uint GL_INVARIANT_EXT                  = 0x87C2;
const uint GL_LOCAL_CONSTANT_EXT             = 0x87C3;
const uint GL_LOCAL_EXT                      = 0x87C4;
const uint GL_MAX_VERTEX_SHADER_INSTRUCTIONS_EXT = 0x87C5;
const uint GL_MAX_VERTEX_SHADER_VARIANTS_EXT = 0x87C6;
const uint GL_MAX_VERTEX_SHADER_INVARIANTS_EXT = 0x87C7;
const uint GL_MAX_VERTEX_SHADER_LOCAL_CONSTANTS_EXT = 0x87C8;
const uint GL_MAX_VERTEX_SHADER_LOCALS_EXT   = 0x87C9;
const uint GL_MAX_OPTIMIZED_VERTEX_SHADER_INSTRUCTIONS_EXT = 0x87CA;
const uint GL_MAX_OPTIMIZED_VERTEX_SHADER_VARIANTS_EXT = 0x87CB;
const uint GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCAL_CONSTANTS_EXT = 0x87CC;
const uint GL_MAX_OPTIMIZED_VERTEX_SHADER_INARIANTS_EXT = 0x87CD;
const uint GL_MAX_OPTIMIZED_VERTEX_SHADER_LOCALS_EXT = 0x87CE;
const uint GL_VERTEX_SHADER_INSTRUCTIONS_EXT = 0x87CF;
const uint GL_VERTEX_SHADER_VARIANTS_EXT     = 0x87D0;
const uint GL_VERTEX_SHADER_INVARIANTS_EXT   = 0x87D1;
const uint GL_VERTEX_SHADER_LOCAL_CONSTANTS_EXT = 0x87D2;
const uint GL_VERTEX_SHADER_LOCALS_EXT       = 0x87D3;
const uint GL_VERTEX_SHADER_OPTIMIZED_EXT    = 0x87D4;
const uint GL_X_EXT                          = 0x87D5;
const uint GL_Y_EXT                          = 0x87D6;
const uint GL_Z_EXT                          = 0x87D7;
const uint GL_W_EXT                          = 0x87D8;
const uint GL_NEGATIVE_X_EXT                 = 0x87D9;
const uint GL_NEGATIVE_Y_EXT                 = 0x87DA;
const uint GL_NEGATIVE_Z_EXT                 = 0x87DB;
const uint GL_NEGATIVE_W_EXT                 = 0x87DC;
const uint GL_ZERO_EXT                       = 0x87DD;
const uint GL_ONE_EXT                        = 0x87DE;
const uint GL_NEGATIVE_ONE_EXT               = 0x87DF;
const uint GL_NORMALIZED_RANGE_EXT           = 0x87E0;
const uint GL_FULL_RANGE_EXT                 = 0x87E1;
const uint GL_CURRENT_VERTEX_EXT             = 0x87E2;
const uint GL_MVP_MATRIX_EXT                 = 0x87E3;
const uint GL_VARIANT_VALUE_EXT              = 0x87E4;
const uint GL_VARIANT_DATATYPE_EXT           = 0x87E5;
const uint GL_VARIANT_ARRAY_STRIDE_EXT       = 0x87E6;
const uint GL_VARIANT_ARRAY_TYPE_EXT         = 0x87E7;
const uint GL_VARIANT_ARRAY_EXT              = 0x87E8;
const uint GL_VARIANT_ARRAY_POINTER_EXT      = 0x87E9;
const uint GL_INVARIANT_VALUE_EXT            = 0x87EA;
const uint GL_INVARIANT_DATATYPE_EXT         = 0x87EB;
const uint GL_LOCAL_CONSTANT_VALUE_EXT       = 0x87EC;
const uint GL_LOCAL_CONSTANT_DATATYPE_EXT    = 0x87ED;

const uint GL_EXT_vertex_weighting = 1;
const uint GL_MODELVIEW0_STACK_DEPTH_EXT      = GL_MODELVIEW_STACK_DEPTH;
const uint GL_MODELVIEW1_STACK_DEPTH_EXT      = 0x8502;
const uint GL_MODELVIEW0_MATRIX_EXT           = GL_MODELVIEW_MATRIX;
const uint GL_MODELVIEW1_MATRIX_EXT           = 0x8506;
const uint GL_VERTEX_WEIGHTING_EXT            = 0x8509;
const uint GL_MODELVIEW0_EXT                  = GL_MODELVIEW;
const uint GL_MODELVIEW1_EXT                  = 0x850A;
const uint GL_CURRENT_VERTEX_WEIGHT_EXT       = 0x850B;
const uint GL_VERTEX_WEIGHT_ARRAY_EXT         = 0x850C;
const uint GL_VERTEX_WEIGHT_ARRAY_SIZE_EXT    = 0x850D;
const uint GL_VERTEX_WEIGHT_ARRAY_TYPE_EXT    = 0x850E;
const uint GL_VERTEX_WEIGHT_ARRAY_STRIDE_EXT  = 0x850F;
const uint GL_VERTEX_WEIGHT_ARRAY_POINTER_EXT = 0x8510;



/*-----------------------------------------------------------------------
 * 3DFX
 *----------------------------------------------------------------------*/

const uint GL_3DFX_multisample = 1;
const uint GL_MULTISAMPLE_3DFX               = 0x86B2;
const uint GL_SAMPLE_BUFFERS_3DFX            = 0x86B3;
const uint GL_SAMPLES_3DFX                   = 0x86B4;
const uint GL_MULTISAMPLE_BIT_3DFX           = 0x20000000;

const uint GL_3DFX_tbuffer = 1;

const uint GL_3DFX_texture_compression_FXT1 = 1;
const uint GL_COMPRESSED_RGB_FXT1_3DFX       = 0x86B0;
const uint GL_COMPRESSED_RGBA_FXT1_3DFX      = 0x86B1;



/*-----------------------------------------------------------------------
 * APPLE
 *----------------------------------------------------------------------*/

const uint GL_APPLE_specular_vector = 1;
const uint GL_LIGHT_MODEL_SPECULAR_VECTOR_APPLE = 0x85B0;

const uint GL_APPLE_transform_hint = 1;
const uint GL_TRANSFORM_HINT_APPLE           = 0x85B1;



/*-----------------------------------------------------------------------
 * ATI
 *----------------------------------------------------------------------*/

const uint GL_ATI_element_array = 1;
const uint GL_ELEMENT_ARRAY_ATI              = 0x8768;
const uint GL_ELEMENT_ARRAY_TYPE_ATI         = 0x8769;
const uint GL_ELEMENT_ARRAY_POINTER_ATI      = 0x876A;

const uint GL_ATI_envmap_bumpmap = 1;
const uint GL_BUMP_ROT_MATRIX_ATI            = 0x8775;
const uint GL_BUMP_ROT_MATRIX_SIZE_ATI       = 0x8776;
const uint GL_BUMP_NUM_TEX_UNITS_ATI         = 0x8777;
const uint GL_BUMP_TEX_UNITS_ATI             = 0x8778;
const uint GL_DUDV_ATI                       = 0x8779;
const uint GL_DU8DV8_ATI                     = 0x877A;
const uint GL_BUMP_ENVMAP_ATI                = 0x877B;
const uint GL_BUMP_TARGET_ATI                = 0x877C;

const uint GL_ATI_fragment_shader = 1;
const uint GL_FRAGMENT_SHADER_ATI            = 0x8920;
const uint GL_REG_0_ATI                      = 0x8921;
const uint GL_REG_1_ATI                      = 0x8922;
const uint GL_REG_2_ATI                      = 0x8923;
const uint GL_REG_3_ATI                      = 0x8924;
const uint GL_REG_4_ATI                      = 0x8925;
const uint GL_REG_5_ATI                      = 0x8926;
const uint GL_REG_6_ATI                      = 0x8927;
const uint GL_REG_7_ATI                      = 0x8928;
const uint GL_REG_8_ATI                      = 0x8929;
const uint GL_REG_9_ATI                      = 0x892A;
const uint GL_REG_10_ATI                     = 0x892B;
const uint GL_REG_11_ATI                     = 0x892C;
const uint GL_REG_12_ATI                     = 0x892D;
const uint GL_REG_13_ATI                     = 0x892E;
const uint GL_REG_14_ATI                     = 0x892F;
const uint GL_REG_15_ATI                     = 0x8930;
const uint GL_REG_16_ATI                     = 0x8931;
const uint GL_REG_17_ATI                     = 0x8932;
const uint GL_REG_18_ATI                     = 0x8933;
const uint GL_REG_19_ATI                     = 0x8934;
const uint GL_REG_20_ATI                     = 0x8935;
const uint GL_REG_21_ATI                     = 0x8936;
const uint GL_REG_22_ATI                     = 0x8937;
const uint GL_REG_23_ATI                     = 0x8938;
const uint GL_REG_24_ATI                     = 0x8939;
const uint GL_REG_25_ATI                     = 0x893A;
const uint GL_REG_26_ATI                     = 0x893B;
const uint GL_REG_27_ATI                     = 0x893C;
const uint GL_REG_28_ATI                     = 0x893D;
const uint GL_REG_29_ATI                     = 0x893E;
const uint GL_REG_30_ATI                     = 0x893F;
const uint GL_REG_31_ATI                     = 0x8940;
const uint GL_CON_0_ATI                      = 0x8941;
const uint GL_CON_1_ATI                      = 0x8942;
const uint GL_CON_2_ATI                      = 0x8943;
const uint GL_CON_3_ATI                      = 0x8944;
const uint GL_CON_4_ATI                      = 0x8945;
const uint GL_CON_5_ATI                      = 0x8946;
const uint GL_CON_6_ATI                      = 0x8947;
const uint GL_CON_7_ATI                      = 0x8948;
const uint GL_CON_8_ATI                      = 0x8949;
const uint GL_CON_9_ATI                      = 0x894A;
const uint GL_CON_10_ATI                     = 0x894B;
const uint GL_CON_11_ATI                     = 0x894C;
const uint GL_CON_12_ATI                     = 0x894D;
const uint GL_CON_13_ATI                     = 0x894E;
const uint GL_CON_14_ATI                     = 0x894F;
const uint GL_CON_15_ATI                     = 0x8950;
const uint GL_CON_16_ATI                     = 0x8951;
const uint GL_CON_17_ATI                     = 0x8952;
const uint GL_CON_18_ATI                     = 0x8953;
const uint GL_CON_19_ATI                     = 0x8954;
const uint GL_CON_20_ATI                     = 0x8955;
const uint GL_CON_21_ATI                     = 0x8956;
const uint GL_CON_22_ATI                     = 0x8957;
const uint GL_CON_23_ATI                     = 0x8958;
const uint GL_CON_24_ATI                     = 0x8959;
const uint GL_CON_25_ATI                     = 0x895A;
const uint GL_CON_26_ATI                     = 0x895B;
const uint GL_CON_27_ATI                     = 0x895C;
const uint GL_CON_28_ATI                     = 0x895D;
const uint GL_CON_29_ATI                     = 0x895E;
const uint GL_CON_30_ATI                     = 0x895F;
const uint GL_CON_31_ATI                     = 0x8960;
const uint GL_MOV_ATI                        = 0x8961;
const uint GL_ADD_ATI                        = 0x8963;
const uint GL_MUL_ATI                        = 0x8964;
const uint GL_SUB_ATI                        = 0x8965;
const uint GL_DOT3_ATI                       = 0x8966;
const uint GL_DOT4_ATI                       = 0x8967;
const uint GL_MAD_ATI                        = 0x8968;
const uint GL_LERP_ATI                       = 0x8969;
const uint GL_CND_ATI                        = 0x896A;
const uint GL_CND0_ATI                       = 0x896B;
const uint GL_DOT2_ADD_ATI                   = 0x896C;
const uint GL_SECONDARY_INTERPOLATOR_ATI     = 0x896D;
const uint GL_NUM_FRAGMENT_REGISTERS_ATI     = 0x896E;
const uint GL_NUM_FRAGMENT_CONSTANTS_ATI     = 0x896F;
const uint GL_NUM_PASSES_ATI                 = 0x8970;
const uint GL_NUM_INSTRUCTIONS_PER_PASS_ATI  = 0x8971;
const uint GL_NUM_INSTRUCTIONS_TOTAL_ATI     = 0x8972;
const uint GL_NUM_INPUT_INTERPOLATOR_COMPONENTS_ATI = 0x8973;
const uint GL_NUM_LOOPBACK_COMPONENTS_ATI    = 0x8974;
const uint GL_COLOR_ALPHA_PAIRING_ATI        = 0x8975;
const uint GL_SWIZZLE_STR_ATI                = 0x8976;
const uint GL_SWIZZLE_STQ_ATI                = 0x8977;
const uint GL_SWIZZLE_STR_DR_ATI             = 0x8978;
const uint GL_SWIZZLE_STQ_DQ_ATI             = 0x8979;
const uint GL_SWIZZLE_STRQ_ATI               = 0x897A;
const uint GL_SWIZZLE_STRQ_DQ_ATI            = 0x897B;
const uint GL_RED_BIT_ATI                    = 0x00000001;
const uint GL_GREEN_BIT_ATI                  = 0x00000002;
const uint GL_BLUE_BIT_ATI                   = 0x00000004;
const uint GL_2X_BIT_ATI                     = 0x00000001;
const uint GL_4X_BIT_ATI                     = 0x00000002;
const uint GL_8X_BIT_ATI                     = 0x00000004;
const uint GL_HALF_BIT_ATI                   = 0x00000008;
const uint GL_QUARTER_BIT_ATI                = 0x00000010;
const uint GL_EIGHTH_BIT_ATI                 = 0x00000020;
const uint GL_SATURATE_BIT_ATI               = 0x00000040;
const uint GL_COMP_BIT_ATI                   = 0x00000002;
const uint GL_NEGATE_BIT_ATI                 = 0x00000004;
const uint GL_BIAS_BIT_ATI                   = 0x00000008;

const uint GL_ATI_pn_triangles = 1;
const uint GL_PN_TRIANGLES_ATI                       = 0x87F0;
const uint GL_MAX_PN_TRIANGLES_TESSELATION_LEVEL_ATI = 0x87F1;
const uint GL_PN_TRIANGLES_POINT_MODE_ATI            = 0x87F2;
const uint GL_PN_TRIANGLES_NORMAL_MODE_ATI           = 0x87F3;
const uint GL_PN_TRIANGLES_TESSELATION_LEVEL_ATI     = 0x87F4;
const uint GL_PN_TRIANGLES_POINT_MODE_LINEAR_ATI     = 0x87F5;
const uint GL_PN_TRIANGLES_POINT_MODE_CUBIC_ATI      = 0x87F6;
const uint GL_PN_TRIANGLES_NORMAL_MODE_LINEAR_ATI    = 0x87F7;
const uint GL_PN_TRIANGLES_NORMAL_MODE_QUADRATIC_ATI = 0x87F8;

const uint GL_ATI_texture_mirror_once = 1;
const uint GL_MIRROR_CLAMP_ATI               = 0x8742;
const uint GL_MIRROR_CLAMP_TO_EDGE_ATI       = 0x8743;

const uint GL_ATI_vertex_array_object = 1;
const uint GL_STATIC_ATI                     = 0x8760;
const uint GL_DYNAMIC_ATI                    = 0x8761;
const uint GL_PRESERVE_ATI                   = 0x8762;
const uint GL_DISCARD_ATI                    = 0x8763;
const uint GL_OBJECT_BUFFER_SIZE_ATI         = 0x8764;
const uint GL_OBJECT_BUFFER_USAGE_ATI        = 0x8765;
const uint GL_ARRAY_OBJECT_BUFFER_ATI        = 0x8766;
const uint GL_ARRAY_OBJECT_OFFSET_ATI        = 0x8767;

const uint GL_ATI_vertex_streams = 1;
const uint GL_MAX_VERTEX_STREAMS_ATI         = 0x876B;
const uint GL_VERTEX_STREAM0_ATI             = 0x876C;
const uint GL_VERTEX_STREAM1_ATI             = 0x876D;
const uint GL_VERTEX_STREAM2_ATI             = 0x876E;
const uint GL_VERTEX_STREAM3_ATI             = 0x876F;
const uint GL_VERTEX_STREAM4_ATI             = 0x8770;
const uint GL_VERTEX_STREAM5_ATI             = 0x8771;
const uint GL_VERTEX_STREAM6_ATI             = 0x8772;
const uint GL_VERTEX_STREAM7_ATI             = 0x8773;
const uint GL_VERTEX_SOURCE_ATI              = 0x8774;



/*-----------------------------------------------------------------------
 * HP (Hewlett Packard)
 *----------------------------------------------------------------------*/

const uint GL_HP_convolution_border_modes = 1;
const uint GL_IGNORE_BORDER_HP               = 0x8150;
const uint GL_CONSTANT_BORDER_HP             = 0x8151;
const uint GL_REPLICATE_BORDER_HP            = 0x8153;
const uint GL_CONVOLUTION_BORDER_COLOR_HP    = 0x8154;

const uint GL_HP_image_transform = 1;
const uint GL_IMAGE_SCALE_X_HP               = 0x8155;
const uint GL_IMAGE_SCALE_Y_HP               = 0x8156;
const uint GL_IMAGE_TRANSLATE_X_HP           = 0x8157;
const uint GL_IMAGE_TRANSLATE_Y_HP           = 0x8158;
const uint GL_IMAGE_ROTATE_ANGLE_HP          = 0x8159;
const uint GL_IMAGE_ROTATE_ORIGIN_X_HP       = 0x815A;
const uint GL_IMAGE_ROTATE_ORIGIN_Y_HP       = 0x815B;
const uint GL_IMAGE_MAG_FILTER_HP            = 0x815C;
const uint GL_IMAGE_MIN_FILTER_HP            = 0x815D;
const uint GL_IMAGE_CUBIC_WEIGHT_HP          = 0x815E;
const uint GL_CUBIC_HP                       = 0x815F;
const uint GL_AVERAGE_HP                     = 0x8160;
const uint GL_IMAGE_TRANSFORM_2D_HP          = 0x8161;
const uint GL_POST_IMAGE_TRANSFORM_COLOR_TABLE_HP = 0x8162;
const uint GL_PROXY_POST_IMAGE_TRANSFORM_COLOR_TABLE_HP = 0x8163;

const uint GL_HP_occlusion_test = 1;
const uint GL_OCCLUSION_TEST_HP              = 0x8165;
const uint GL_OCCLUSION_TEST_RESULT_HP       = 0x8166;

const uint GL_HP_texture_lighting = 1;
const uint GL_TEXTURE_LIGHTING_MODE_HP       = 0x8167;
const uint GL_TEXTURE_POST_SPECULAR_HP       = 0x8168;
const uint GL_TEXTURE_PRE_SPECULAR_HP        = 0x8169;



/*-----------------------------------------------------------------------
 * IBM
 *----------------------------------------------------------------------*/

const uint GL_IBM_cull_vertex = 1;
const uint GL_CULL_VERTEX_IBM                = 103050;

const uint GL_IBM_multimode_draw_arrays = 1;

const uint GL_IBM_rasterpos_clip = 1;
const uint GL_RASTER_POSITION_UNCLIPPED_IBM  = 0x19262;

const uint GL_IBM_texture_mirrored_repeat = 1;
const uint GL_MIRRORED_REPEAT_IBM            = 0x8370;

const uint GL_IBM_vertex_array_lists = 1;
const uint GL_VERTEX_ARRAY_LIST_IBM                 = 103070;
const uint GL_NORMAL_ARRAY_LIST_IBM                 = 103071;
const uint GL_COLOR_ARRAY_LIST_IBM                  = 103072;
const uint GL_INDEX_ARRAY_LIST_IBM                  = 103073;
const uint GL_TEXTURE_COORD_ARRAY_LIST_IBM          = 103074;
const uint GL_EDGE_FLAG_ARRAY_LIST_IBM              = 103075;
const uint GL_FOG_COORDINATE_ARRAY_LIST_IBM         = 103076;
const uint GL_SECONDARY_COLOR_ARRAY_LIST_IBM        = 103077;
const uint GL_VERTEX_ARRAY_LIST_STRIDE_IBM          = 103080;
const uint GL_NORMAL_ARRAY_LIST_STRIDE_IBM          = 103081;
const uint GL_COLOR_ARRAY_LIST_STRIDE_IBM           = 103082;
const uint GL_INDEX_ARRAY_LIST_STRIDE_IBM           = 103083;
const uint GL_TEXTURE_COORD_ARRAY_LIST_STRIDE_IBM   = 103084;
const uint GL_EDGE_FLAG_ARRAY_LIST_STRIDE_IBM       = 103085;
const uint GL_FOG_COORDINATE_ARRAY_LIST_STRIDE_IBM  = 103086;
const uint GL_SECONDARY_COLOR_ARRAY_LIST_STRIDE_IBM = 103087;



/*-----------------------------------------------------------------------
 * INGR
 *----------------------------------------------------------------------*/

const uint GL_INGR_color_clamp = 1;
const uint GL_RED_MIN_CLAMP_INGR             = 0x8560;
const uint GL_GREEN_MIN_CLAMP_INGR           = 0x8561;
const uint GL_BLUE_MIN_CLAMP_INGR            = 0x8562;
const uint GL_ALPHA_MIN_CLAMP_INGR           = 0x8563;
const uint GL_RED_MAX_CLAMP_INGR             = 0x8564;
const uint GL_GREEN_MAX_CLAMP_INGR           = 0x8565;
const uint GL_BLUE_MAX_CLAMP_INGR            = 0x8566;
const uint GL_ALPHA_MAX_CLAMP_INGR           = 0x8567;

const uint GL_INGR_interlace_read = 1;
const uint GL_INTERLACE_READ_INGR            = 0x8568;

const uint GL_INGR_palette_buffer = 1;



/*-----------------------------------------------------------------------
 * INTEL
 *----------------------------------------------------------------------*/

const uint GL_INTEL_parallel_arrays = 1;
const uint GL_PARALLEL_ARRAYS_INTEL          = 0x83F4;
const uint GL_VERTEX_ARRAY_PARALLEL_POINTERS_INTEL = 0x83F5;
const uint GL_NORMAL_ARRAY_PARALLEL_POINTERS_INTEL = 0x83F6;
const uint GL_COLOR_ARRAY_PARALLEL_POINTERS_INTEL = 0x83F7;
const uint GL_TEXTURE_COORD_ARRAY_PARALLEL_POINTERS_INTEL = 0x83F8;

const uint GL_INTEL_texture_scissor = 1;



/*-----------------------------------------------------------------------
 * MESA (Brian Pauls Mesa3D)
 *----------------------------------------------------------------------*/

const uint GL_MESA_resize_buffers = 1;

const uint GL_MESA_window_pos = 1;



/*-----------------------------------------------------------------------
 * NV (nVidia)
 *----------------------------------------------------------------------*/

const uint GL_NV_blend_square = 1;

const uint GL_NV_copy_depth_to_color = 1;
const uint GL_DEPTH_STENCIL_TO_RGBA_NV       = 0x886E;
const uint GL_DEPTH_STENCIL_TO_BGRA_NV       = 0x886F;

const uint GL_NV_depth_clamp = 1;
const uint GL_DEPTH_CLAMP_NV                 = 0x864F;

const uint GL_NV_element_array = 1;
const uint GL_ELEMENT_ARRAY_TYPE_NV          = 0x8769;
const uint GL_ELEMENT_ARRAY_POINTER_NV       = 0x876A;

const uint GL_NV_evaluators = 1;
const uint GL_EVAL_2D_NV                     = 0x86C0;
const uint GL_EVAL_TRIANGULAR_2D_NV          = 0x86C1;
const uint GL_MAP_TESSELLATION_NV            = 0x86C2;
const uint GL_MAP_ATTRIB_U_ORDER_NV          = 0x86C3;
const uint GL_MAP_ATTRIB_V_ORDER_NV          = 0x86C4;
const uint GL_EVAL_FRACTIONAL_TESSELLATION_NV = 0x86C5;
const uint GL_EVAL_VERTEX_ATTRIB0_NV         = 0x86C6;
const uint GL_EVAL_VERTEX_ATTRIB1_NV         = 0x86C7;
const uint GL_EVAL_VERTEX_ATTRIB2_NV         = 0x86C8;
const uint GL_EVAL_VERTEX_ATTRIB3_NV         = 0x86C9;
const uint GL_EVAL_VERTEX_ATTRIB4_NV         = 0x86CA;
const uint GL_EVAL_VERTEX_ATTRIB5_NV         = 0x86CB;
const uint GL_EVAL_VERTEX_ATTRIB6_NV         = 0x86CC;
const uint GL_EVAL_VERTEX_ATTRIB7_NV         = 0x86CD;
const uint GL_EVAL_VERTEX_ATTRIB8_NV         = 0x86CE;
const uint GL_EVAL_VERTEX_ATTRIB9_NV         = 0x86CF;
const uint GL_EVAL_VERTEX_ATTRIB10_NV        = 0x86D0;
const uint GL_EVAL_VERTEX_ATTRIB11_NV        = 0x86D1;
const uint GL_EVAL_VERTEX_ATTRIB12_NV        = 0x86D2;
const uint GL_EVAL_VERTEX_ATTRIB13_NV        = 0x86D3;
const uint GL_EVAL_VERTEX_ATTRIB14_NV        = 0x86D4;
const uint GL_EVAL_VERTEX_ATTRIB15_NV        = 0x86D5;
const uint GL_MAX_MAP_TESSELLATION_NV        = 0x86D6;
const uint GL_MAX_RATIONAL_EVAL_ORDER_NV     = 0x86D7;

const uint GL_NV_fence = 1;
const uint GL_ALL_COMPLETED_NV               = 0x84F2;
const uint GL_FENCE_STATUS_NV                = 0x84F3;
const uint GL_FENCE_CONDITION_NV             = 0x84F4;

const uint GL_NV_float_buffer = 1;
const uint GL_FLOAT_R_NV                     = 0x8880;
const uint GL_FLOAT_RG_NV                    = 0x8881;
const uint GL_FLOAT_RGB_NV                   = 0x8882;
const uint GL_FLOAT_RGBA_NV                  = 0x8883;
const uint GL_FLOAT_R16_NV                   = 0x8884;
const uint GL_FLOAT_R32_NV                   = 0x8885;
const uint GL_FLOAT_RG16_NV                  = 0x8886;
const uint GL_FLOAT_RG32_NV                  = 0x8887;
const uint GL_FLOAT_RGB16_NV                 = 0x8888;
const uint GL_FLOAT_RGB32_NV                 = 0x8889;
const uint GL_FLOAT_RGBA16_NV                = 0x888A;
const uint GL_FLOAT_RGBA32_NV                = 0x888B;
const uint GL_TEXTURE_FLOAT_COMPONENTS_NV    = 0x888C;
const uint GL_FLOAT_CLEAR_COLOR_VALUE_NV     = 0x888D;
const uint GL_FLOAT_RGBA_MODE_NV             = 0x888E;

const uint GL_NV_fog_distance = 1;
const uint GL_FOG_DISTANCE_MODE_NV           = 0x855A;
const uint GL_EYE_RADIAL_NV                  = 0x855B;
const uint GL_EYE_PLANE_ABSOLUTE_NV          = 0x855C;
/* reuse GL_EYE_PLANE */

const uint GL_NV_fragment_program = 1;
const uint GL_FRAGMENT_PROGRAM_NV            = 0x8870;
const uint GL_MAX_TEXTURE_COORDS_NV          = 0x8871;
const uint GL_MAX_TEXTURE_IMAGE_UNITS_NV     = 0x8872;
const uint GL_FRAGMENT_PROGRAM_BINDING_NV    = 0x8873;
const uint GL_PROGRAM_ERROR_STRING_NV        = 0x8874;
const uint GL_MAX_FRAGMENT_PROGRAM_LOCAL_PARAMETERS_NV = 0x8868;

const uint GL_NV_half_float = 1;
const uint GL_HALF_FLOAT_NV                  = 0x140B;

const uint GL_NV_light_max_exponent = 1;
const uint GL_MAX_SHININESS_NV               = 0x8504;
const uint GL_MAX_SPOT_EXPONENT_NV           = 0x8505;

const uint GL_NV_multisample_filter_hint = 1;
const uint GL_MULTISAMPLE_FILTER_HINT_NV     = 0x8534;

const uint GL_NV_occlusion_query = 1;
const uint GL_PIXEL_COUNTER_BITS_NV          = 0x8864;
const uint GL_CURRENT_OCCLUSION_QUERY_ID_NV  = 0x8865;
const uint GL_PIXEL_COUNT_NV                 = 0x8866;
const uint GL_PIXEL_COUNT_AVAILABLE_NV       = 0x8867;

const uint GL_NV_packed_depth_stencil = 1;
const uint GL_DEPTH_STENCIL_NV               = 0x84F9;
const uint GL_UNSIGNED_INT_24_8_NV           = 0x84FA;

const uint GL_NV_pixel_data_range = 1;
const uint GL_WRITE_PIXEL_DATA_RANGE_NV      = 0x8878;
const uint GL_READ_PIXEL_DATA_RANGE_NV       = 0x8879;
const uint GL_WRITE_PIXEL_DATA_RANGE_LENGTH_NV = 0x887A;
const uint GL_READ_PIXEL_DATA_RANGE_LENGTH_NV = 0x887B;
const uint GL_WRITE_PIXEL_DATA_RANGE_POINTER_NV = 0x887C;
const uint GL_READ_PIXEL_DATA_RANGE_POINTER_NV = 0x887D;

const uint GL_NV_point_sprite = 1;
const uint GL_POINT_SPRITE_NV                = 0x8861;
const uint GL_COORD_REPLACE_NV               = 0x8862;
const uint GL_POINT_SPRITE_R_MODE_NV         = 0x8863;

const uint GL_NV_primitive_restart = 1;
const uint GL_PRIMITIVE_RESTART_NV           = 0x8558;
const uint GL_PRIMITIVE_RESTART_INDEX_NV     = 0x8559;

const uint GL_NV_register_combiners = 1;
const uint GL_REGISTER_COMBINERS_NV          = 0x8522;
const uint GL_VARIABLE_A_NV                  = 0x8523;
const uint GL_VARIABLE_B_NV                  = 0x8524;
const uint GL_VARIABLE_C_NV                  = 0x8525;
const uint GL_VARIABLE_D_NV                  = 0x8526;
const uint GL_VARIABLE_E_NV                  = 0x8527;
const uint GL_VARIABLE_F_NV                  = 0x8528;
const uint GL_VARIABLE_G_NV                  = 0x8529;
const uint GL_CONSTANT_COLOR0_NV             = 0x852A;
const uint GL_CONSTANT_COLOR1_NV             = 0x852B;
const uint GL_PRIMARY_COLOR_NV               = 0x852C;
const uint GL_SECONDARY_COLOR_NV             = 0x852D;
const uint GL_SPARE0_NV                      = 0x852E;
const uint GL_SPARE1_NV                      = 0x852F;
const uint GL_DISCARD_NV                     = 0x8530;
const uint GL_E_TIMES_F_NV                   = 0x8531;
const uint GL_SPARE0_PLUS_SECONDARY_COLOR_NV = 0x8532;
const uint GL_UNSIGNED_IDENTITY_NV           = 0x8536;
const uint GL_UNSIGNED_INVERT_NV             = 0x8537;
const uint GL_EXPAND_NORMAL_NV               = 0x8538;
const uint GL_EXPAND_NEGATE_NV               = 0x8539;
const uint GL_HALF_BIAS_NORMAL_NV            = 0x853A;
const uint GL_HALF_BIAS_NEGATE_NV            = 0x853B;
const uint GL_SIGNED_IDENTITY_NV             = 0x853C;
const uint GL_SIGNED_NEGATE_NV               = 0x853D;
const uint GL_SCALE_BY_TWO_NV                = 0x853E;
const uint GL_SCALE_BY_FOUR_NV               = 0x853F;
const uint GL_SCALE_BY_ONE_HALF_NV           = 0x8540;
const uint GL_BIAS_BY_NEGATIVE_ONE_HALF_NV   = 0x8541;
const uint GL_COMBINER_INPUT_NV              = 0x8542;
const uint GL_COMBINER_MAPPING_NV            = 0x8543;
const uint GL_COMBINER_COMPONENT_USAGE_NV    = 0x8544;
const uint GL_COMBINER_AB_DOT_PRODUCT_NV     = 0x8545;
const uint GL_COMBINER_CD_DOT_PRODUCT_NV     = 0x8546;
const uint GL_COMBINER_MUX_SUM_NV            = 0x8547;
const uint GL_COMBINER_SCALE_NV              = 0x8548;
const uint GL_COMBINER_BIAS_NV               = 0x8549;
const uint GL_COMBINER_AB_OUTPUT_NV          = 0x854A;
const uint GL_COMBINER_CD_OUTPUT_NV          = 0x854B;
const uint GL_COMBINER_SUM_OUTPUT_NV         = 0x854C;
const uint GL_MAX_GENERAL_COMBINERS_NV       = 0x854D;
const uint GL_NUM_GENERAL_COMBINERS_NV       = 0x854E;
const uint GL_COLOR_SUM_CLAMP_NV             = 0x854F;
const uint GL_COMBINER0_NV                   = 0x8550;
const uint GL_COMBINER1_NV                   = 0x8551;
const uint GL_COMBINER2_NV                   = 0x8552;
const uint GL_COMBINER3_NV                   = 0x8553;
const uint GL_COMBINER4_NV                   = 0x8554;
const uint GL_COMBINER5_NV                   = 0x8555;
const uint GL_COMBINER6_NV                   = 0x8556;
const uint GL_COMBINER7_NV                   = 0x8557;
/* reuse GL_TEXTURE0_ARB */
/* reuse GL_TEXTURE1_ARB */
/* reuse GL_ZERO */
/* reuse GL_NONE */
/* reuse GL_FOG */

const uint GL_NV_register_combiners2 = 1;
const uint GL_PER_STAGE_CONSTANTS_NV         = 0x8535;

const uint GL_NV_stencil_two_side = 1;
const uint GL_STENCIL_TEST_TWO_SIDE_NV       = 0x8910;
const uint GL_ACTIVE_STENCIL_FACE_NV         = 0x8911;

const uint GL_NV_texgen_emboss = 1;
const uint GL_EMBOSS_LIGHT_NV                = 0x855D;
const uint GL_EMBOSS_CONSTANT_NV             = 0x855E;
const uint GL_EMBOSS_MAP_NV                  = 0x855F;

const uint GL_NV_texgen_reflection = 1;
const uint GL_NORMAL_MAP_NV                  = 0x8511;
const uint GL_REFLECTION_MAP_NV              = 0x8512;

const uint GL_NV_texture_compression_vtc = 1;

const uint GL_NV_texture_env_combine4 = 1;
const uint GL_COMBINE4_NV                    = 0x8503;
const uint GL_SOURCE3_RGB_NV                 = 0x8583;
const uint GL_SOURCE3_ALPHA_NV               = 0x858B;
const uint GL_OPERAND3_RGB_NV                = 0x8593;
const uint GL_OPERAND3_ALPHA_NV              = 0x859B;

const uint GL_NV_texture_rectangle = 1;
const uint GL_TEXTURE_RECTANGLE_NV           = 0x84F5;
const uint GL_TEXTURE_BINDING_RECTANGLE_NV   = 0x84F6;
const uint GL_PROXY_TEXTURE_RECTANGLE_NV     = 0x84F7;
const uint GL_MAX_RECTANGLE_TEXTURE_SIZE_NV  = 0x84F8;

const uint GL_NV_texture_shader = 1;
const uint GL_OFFSET_TEXTURE_RECTANGLE_NV    = 0x864C;
const uint GL_OFFSET_TEXTURE_RECTANGLE_SCALE_NV = 0x864D;
const uint GL_DOT_PRODUCT_TEXTURE_RECTANGLE_NV = 0x864E;
const uint GL_RGBA_UNSIGNED_DOT_PRODUCT_MAPPING_NV = 0x86D9;
const uint GL_UNSIGNED_INT_S8_S8_8_8_NV      = 0x86DA;
const uint GL_UNSIGNED_INT_8_8_S8_S8_REV_NV  = 0x86DB;
const uint GL_DSDT_MAG_INTENSITY_NV          = 0x86DC;
const uint GL_SHADER_CONSISTENT_NV           = 0x86DD;
const uint GL_TEXTURE_SHADER_NV              = 0x86DE;
const uint GL_SHADER_OPERATION_NV            = 0x86DF;
const uint GL_CULL_MODES_NV                  = 0x86E0;
const uint GL_OFFSET_TEXTURE_MATRIX_NV       = 0x86E1;
const uint GL_OFFSET_TEXTURE_SCALE_NV        = 0x86E2;
const uint GL_OFFSET_TEXTURE_BIAS_NV         = 0x86E3;
const uint GL_OFFSET_TEXTURE_2D_MATRIX_NV    = GL_OFFSET_TEXTURE_MATRIX_NV;
const uint GL_OFFSET_TEXTURE_2D_SCALE_NV     = GL_OFFSET_TEXTURE_SCALE_NV;
const uint GL_OFFSET_TEXTURE_2D_BIAS_NV      = GL_OFFSET_TEXTURE_BIAS_NV;
const uint GL_PREVIOUS_TEXTURE_INPUT_NV      = 0x86E4;
const uint GL_CONST_EYE_NV                   = 0x86E5;
const uint GL_PASS_THROUGH_NV                = 0x86E6;
const uint GL_CULL_FRAGMENT_NV               = 0x86E7;
const uint GL_OFFSET_TEXTURE_2D_NV           = 0x86E8;
const uint GL_DEPENDENT_AR_TEXTURE_2D_NV     = 0x86E9;
const uint GL_DEPENDENT_GB_TEXTURE_2D_NV     = 0x86EA;
const uint GL_DOT_PRODUCT_NV                 = 0x86EC;
const uint GL_DOT_PRODUCT_DEPTH_REPLACE_NV   = 0x86ED;
const uint GL_DOT_PRODUCT_TEXTURE_2D_NV      = 0x86EE;
const uint GL_DOT_PRODUCT_TEXTURE_3D_NV      = 0x86EF;
const uint GL_DOT_PRODUCT_TEXTURE_CUBE_MAP_NV = 0x86F0;
const uint GL_DOT_PRODUCT_DIFFUSE_CUBE_MAP_NV = 0x86F1;
const uint GL_DOT_PRODUCT_REFLECT_CUBE_MAP_NV = 0x86F2;
const uint GL_DOT_PRODUCT_CONST_EYE_REFLECT_CUBE_MAP_NV = 0x86F3;
const uint GL_HILO_NV                        = 0x86F4;
const uint GL_DSDT_NV                        = 0x86F5;
const uint GL_DSDT_MAG_NV                    = 0x86F6;
const uint GL_DSDT_MAG_VIB_NV                = 0x86F7;
const uint GL_HILO16_NV                      = 0x86F8;
const uint GL_SIGNED_HILO_NV                 = 0x86F9;
const uint GL_SIGNED_HILO16_NV               = 0x86FA;
const uint GL_SIGNED_RGBA_NV                 = 0x86FB;
const uint GL_SIGNED_RGBA8_NV                = 0x86FC;
const uint GL_SIGNED_RGB_NV                  = 0x86FE;
const uint GL_SIGNED_RGB8_NV                 = 0x86FF;
const uint GL_SIGNED_LUMINANCE_NV            = 0x8701;
const uint GL_SIGNED_LUMINANCE8_NV           = 0x8702;
const uint GL_SIGNED_LUMINANCE_ALPHA_NV      = 0x8703;
const uint GL_SIGNED_LUMINANCE8_ALPHA8_NV    = 0x8704;
const uint GL_SIGNED_ALPHA_NV                = 0x8705;
const uint GL_SIGNED_ALPHA8_NV               = 0x8706;
const uint GL_SIGNED_INTENSITY_NV            = 0x8707;
const uint GL_SIGNED_INTENSITY8_NV           = 0x8708;
const uint GL_DSDT8_NV                       = 0x8709;
const uint GL_DSDT8_MAG8_NV                  = 0x870A;
const uint GL_DSDT8_MAG8_INTENSITY8_NV       = 0x870B;
const uint GL_SIGNED_RGB_UNSIGNED_ALPHA_NV   = 0x870C;
const uint GL_SIGNED_RGB8_UNSIGNED_ALPHA8_NV = 0x870D;
const uint GL_HI_SCALE_NV                    = 0x870E;
const uint GL_LO_SCALE_NV                    = 0x870F;
const uint GL_DS_SCALE_NV                    = 0x8710;
const uint GL_DT_SCALE_NV                    = 0x8711;
const uint GL_MAGNITUDE_SCALE_NV             = 0x8712;
const uint GL_VIBRANCE_SCALE_NV              = 0x8713;
const uint GL_HI_BIAS_NV                     = 0x8714;
const uint GL_LO_BIAS_NV                     = 0x8715;
const uint GL_DS_BIAS_NV                     = 0x8716;
const uint GL_DT_BIAS_NV                     = 0x8717;
const uint GL_MAGNITUDE_BIAS_NV              = 0x8718;
const uint GL_VIBRANCE_BIAS_NV               = 0x8719;
const uint GL_TEXTURE_BORDER_VALUES_NV       = 0x871A;
const uint GL_TEXTURE_HI_SIZE_NV             = 0x871B;
const uint GL_TEXTURE_LO_SIZE_NV             = 0x871C;
const uint GL_TEXTURE_DS_SIZE_NV             = 0x871D;
const uint GL_TEXTURE_DT_SIZE_NV             = 0x871E;
const uint GL_TEXTURE_MAG_SIZE_NV            = 0x871F;

const uint GL_NV_texture_shader2 = 1;

const uint GL_NV_texture_shader3 = 1;
const uint GL_OFFSET_PROJECTIVE_TEXTURE_2D_NV = 0x8850;
const uint GL_OFFSET_PROJECTIVE_TEXTURE_2D_SCALE_NV = 0x8851;
const uint GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_NV = 0x8852;
const uint GL_OFFSET_PROJECTIVE_TEXTURE_RECTANGLE_SCALE_NV = 0x8853;
const uint GL_OFFSET_HILO_TEXTURE_2D_NV      = 0x8854;
const uint GL_OFFSET_HILO_TEXTURE_RECTANGLE_NV = 0x8855;
const uint GL_OFFSET_HILO_PROJECTIVE_TEXTURE_2D_NV = 0x8856;
const uint GL_OFFSET_HILO_PROJECTIVE_TEXTURE_RECTANGLE_NV = 0x8857;
const uint GL_DEPENDENT_HILO_TEXTURE_2D_NV   = 0x8858;
const uint GL_DEPENDENT_RGB_TEXTURE_3D_NV    = 0x8859;
const uint GL_DEPENDENT_RGB_TEXTURE_CUBE_MAP_NV = 0x885A;
const uint GL_DOT_PRODUCT_PASS_THROUGH_NV    = 0x885B;
const uint GL_DOT_PRODUCT_TEXTURE_1D_NV      = 0x885C;
const uint GL_DOT_PRODUCT_AFFINE_DEPTH_REPLACE_NV = 0x885D;
const uint GL_HILO8_NV                       = 0x885E;
const uint GL_SIGNED_HILO8_NV                = 0x885F;
const uint GL_FORCE_BLUE_TO_ONE_NV           = 0x8860;

const uint GL_NV_vertex_array_range = 1;
const uint GL_VERTEX_ARRAY_RANGE_NV          = 0x851D;
const uint GL_VERTEX_ARRAY_RANGE_LENGTH_NV   = 0x851E;
const uint GL_VERTEX_ARRAY_RANGE_VALID_NV    = 0x851F;
const uint GL_MAX_VERTEX_ARRAY_RANGE_ELEMENT_NV = 0x8520;
const uint GL_VERTEX_ARRAY_RANGE_POINTER_NV  = 0x8521;

const uint GL_NV_vertex_array_range2 = 1;
const uint GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV = 0x8533;

const uint GL_NV_vertex_program = 1;
const uint GL_VERTEX_PROGRAM_NV              = 0x8620;
const uint GL_VERTEX_STATE_PROGRAM_NV        = 0x8621;
const uint GL_ATTRIB_ARRAY_SIZE_NV           = 0x8623;
const uint GL_ATTRIB_ARRAY_STRIDE_NV         = 0x8624;
const uint GL_ATTRIB_ARRAY_TYPE_NV           = 0x8625;
const uint GL_CURRENT_ATTRIB_NV              = 0x8626;
const uint GL_PROGRAM_LENGTH_NV              = 0x8627;
const uint GL_PROGRAM_STRING_NV              = 0x8628;
const uint GL_MODELVIEW_PROJECTION_NV        = 0x8629;
const uint GL_IDENTITY_NV                    = 0x862A;
const uint GL_INVERSE_NV                     = 0x862B;
const uint GL_TRANSPOSE_NV                   = 0x862C;
const uint GL_INVERSE_TRANSPOSE_NV           = 0x862D;
const uint GL_MAX_TRACK_MATRIX_STACK_DEPTH_NV = 0x862E;
const uint GL_MAX_TRACK_MATRICES_NV          = 0x862F;
const uint GL_MATRIX0_NV                     = 0x8630;
const uint GL_MATRIX1_NV                     = 0x8631;
const uint GL_MATRIX2_NV                     = 0x8632;
const uint GL_MATRIX3_NV                     = 0x8633;
const uint GL_MATRIX4_NV                     = 0x8634;
const uint GL_MATRIX5_NV                     = 0x8635;
const uint GL_MATRIX6_NV                     = 0x8636;
const uint GL_MATRIX7_NV                     = 0x8637;
const uint GL_CURRENT_MATRIX_STACK_DEPTH_NV  = 0x8640;
const uint GL_CURRENT_MATRIX_NV              = 0x8641;
const uint GL_VERTEX_PROGRAM_POINT_SIZE_NV   = 0x8642;
const uint GL_VERTEX_PROGRAM_TWO_SIDE_NV     = 0x8643;
const uint GL_PROGRAM_PARAMETER_NV           = 0x8644;
const uint GL_ATTRIB_ARRAY_POINTER_NV        = 0x8645;
const uint GL_PROGRAM_TARGET_NV              = 0x8646;
const uint GL_PROGRAM_RESIDENT_NV            = 0x8647;
const uint GL_TRACK_MATRIX_NV                = 0x8648;
const uint GL_TRACK_MATRIX_TRANSFORM_NV      = 0x8649;
const uint GL_VERTEX_PROGRAM_BINDING_NV      = 0x864A;
const uint GL_PROGRAM_ERROR_POSITION_NV      = 0x864B;
const uint GL_VERTEX_ATTRIB_ARRAY0_NV        = 0x8650;
const uint GL_VERTEX_ATTRIB_ARRAY1_NV        = 0x8651;
const uint GL_VERTEX_ATTRIB_ARRAY2_NV        = 0x8652;
const uint GL_VERTEX_ATTRIB_ARRAY3_NV        = 0x8653;
const uint GL_VERTEX_ATTRIB_ARRAY4_NV        = 0x8654;
const uint GL_VERTEX_ATTRIB_ARRAY5_NV        = 0x8655;
const uint GL_VERTEX_ATTRIB_ARRAY6_NV        = 0x8656;
const uint GL_VERTEX_ATTRIB_ARRAY7_NV        = 0x8657;
const uint GL_VERTEX_ATTRIB_ARRAY8_NV        = 0x8658;
const uint GL_VERTEX_ATTRIB_ARRAY9_NV        = 0x8659;
const uint GL_VERTEX_ATTRIB_ARRAY10_NV       = 0x865A;
const uint GL_VERTEX_ATTRIB_ARRAY11_NV       = 0x865B;
const uint GL_VERTEX_ATTRIB_ARRAY12_NV       = 0x865C;
const uint GL_VERTEX_ATTRIB_ARRAY13_NV       = 0x865D;
const uint GL_VERTEX_ATTRIB_ARRAY14_NV       = 0x865E;
const uint GL_VERTEX_ATTRIB_ARRAY15_NV       = 0x865F;
const uint GL_MAP1_VERTEX_ATTRIB0_4_NV       = 0x8660;
const uint GL_MAP1_VERTEX_ATTRIB1_4_NV       = 0x8661;
const uint GL_MAP1_VERTEX_ATTRIB2_4_NV       = 0x8662;
const uint GL_MAP1_VERTEX_ATTRIB3_4_NV       = 0x8663;
const uint GL_MAP1_VERTEX_ATTRIB4_4_NV       = 0x8664;
const uint GL_MAP1_VERTEX_ATTRIB5_4_NV       = 0x8665;
const uint GL_MAP1_VERTEX_ATTRIB6_4_NV       = 0x8666;
const uint GL_MAP1_VERTEX_ATTRIB7_4_NV       = 0x8667;
const uint GL_MAP1_VERTEX_ATTRIB8_4_NV       = 0x8668;
const uint GL_MAP1_VERTEX_ATTRIB9_4_NV       = 0x8669;
const uint GL_MAP1_VERTEX_ATTRIB10_4_NV      = 0x866A;
const uint GL_MAP1_VERTEX_ATTRIB11_4_NV      = 0x866B;
const uint GL_MAP1_VERTEX_ATTRIB12_4_NV      = 0x866C;
const uint GL_MAP1_VERTEX_ATTRIB13_4_NV      = 0x866D;
const uint GL_MAP1_VERTEX_ATTRIB14_4_NV      = 0x866E;
const uint GL_MAP1_VERTEX_ATTRIB15_4_NV      = 0x866F;
const uint GL_MAP2_VERTEX_ATTRIB0_4_NV       = 0x8670;
const uint GL_MAP2_VERTEX_ATTRIB1_4_NV       = 0x8671;
const uint GL_MAP2_VERTEX_ATTRIB2_4_NV       = 0x8672;
const uint GL_MAP2_VERTEX_ATTRIB3_4_NV       = 0x8673;
const uint GL_MAP2_VERTEX_ATTRIB4_4_NV       = 0x8674;
const uint GL_MAP2_VERTEX_ATTRIB5_4_NV       = 0x8675;
const uint GL_MAP2_VERTEX_ATTRIB6_4_NV       = 0x8676;
const uint GL_MAP2_VERTEX_ATTRIB7_4_NV       = 0x8677;
const uint GL_MAP2_VERTEX_ATTRIB8_4_NV       = 0x8678;
const uint GL_MAP2_VERTEX_ATTRIB9_4_NV       = 0x8679;
const uint GL_MAP2_VERTEX_ATTRIB10_4_NV      = 0x867A;
const uint GL_MAP2_VERTEX_ATTRIB11_4_NV      = 0x867B;
const uint GL_MAP2_VERTEX_ATTRIB12_4_NV      = 0x867C;
const uint GL_MAP2_VERTEX_ATTRIB13_4_NV      = 0x867D;
const uint GL_MAP2_VERTEX_ATTRIB14_4_NV      = 0x867E;
const uint GL_MAP2_VERTEX_ATTRIB15_4_NV      = 0x867F;

const uint GL_NV_vertex_program1_1 = 1;

const uint GL_NV_vertex_program2 = 1;




/*-----------------------------------------------------------------------
 * OML (OpenML)
 *----------------------------------------------------------------------*/

const uint GL_OML_interlace = 1;
const uint GL_INTERLACE_OML                  = 0x8980;
const uint GL_INTERLACE_READ_OML             = 0x8981;

const uint GL_OML_subsample = 1;
const uint GL_FORMAT_SUBSAMPLE_24_24_OML     = 0x8982;
const uint GL_FORMAT_SUBSAMPLE_244_244_OML   = 0x8983;

const uint GL_OML_resample = 1;
const uint GL_PACK_RESAMPLE_OML              = 0x8984;
const uint GL_UNPACK_RESAMPLE_OML            = 0x8985;
const uint GL_RESAMPLE_REPLICATE_OML         = 0x8986;
const uint GL_RESAMPLE_ZERO_FILL_OML         = 0x8987;
const uint GL_RESAMPLE_AVERAGE_OML           = 0x8988;
const uint GL_RESAMPLE_DECIMATE_OML          = 0x8989;



/*-----------------------------------------------------------------------
 * PGI
 *----------------------------------------------------------------------*/

const uint GL_PGI_misc_hints = 1;
const uint GL_PREFER_DOUBLEBUFFER_HINT_PGI   = 0x1A1F8;
const uint GL_CONSERVE_MEMORY_HINT_PGI       = 0x1A1FD;
const uint GL_RECLAIM_MEMORY_HINT_PGI        = 0x1A1FE;
const uint GL_NATIVE_GRAPHICS_HANDLE_PGI     = 0x1A202;
const uint GL_NATIVE_GRAPHICS_BEGIN_HINT_PGI = 0x1A203;
const uint GL_NATIVE_GRAPHICS_END_HINT_PGI   = 0x1A204;
const uint GL_ALWAYS_FAST_HINT_PGI           = 0x1A20C;
const uint GL_ALWAYS_SOFT_HINT_PGI           = 0x1A20D;
const uint GL_ALLOW_DRAW_OBJ_HINT_PGI        = 0x1A20E;
const uint GL_ALLOW_DRAW_WIN_HINT_PGI        = 0x1A20F;
const uint GL_ALLOW_DRAW_FRG_HINT_PGI        = 0x1A210;
const uint GL_ALLOW_DRAW_MEM_HINT_PGI        = 0x1A211;
const uint GL_STRICT_DEPTHFUNC_HINT_PGI      = 0x1A216;
const uint GL_STRICT_LIGHTING_HINT_PGI       = 0x1A217;
const uint GL_STRICT_SCISSOR_HINT_PGI        = 0x1A218;
const uint GL_FULL_STIPPLE_HINT_PGI          = 0x1A219;
const uint GL_CLIP_NEAR_HINT_PGI             = 0x1A220;
const uint GL_CLIP_FAR_HINT_PGI              = 0x1A221;
const uint GL_WIDE_LINE_HINT_PGI             = 0x1A222;
const uint GL_BACK_NORMALS_HINT_PGI          = 0x1A223;

const uint GL_PGI_vertex_hints = 1;
const uint GL_VERTEX_DATA_HINT_PGI           = 0x1A22A;
const uint GL_VERTEX_CONSISTENT_HINT_PGI     = 0x1A22B;
const uint GL_MATERIAL_SIDE_HINT_PGI         = 0x1A22C;
const uint GL_MAX_VERTEX_HINT_PGI            = 0x1A22D;
const uint GL_COLOR3_BIT_PGI                 = 0x00010000;
const uint GL_COLOR4_BIT_PGI                 = 0x00020000;
const uint GL_EDGEFLAG_BIT_PGI               = 0x00040000;
const uint GL_INDEX_BIT_PGI                  = 0x00080000;
const uint GL_MAT_AMBIENT_BIT_PGI            = 0x00100000;
const uint GL_MAT_AMBIENT_AND_DIFFUSE_BIT_PGI = 0x00200000;
const uint GL_MAT_DIFFUSE_BIT_PGI            = 0x00400000;
const uint GL_MAT_EMISSION_BIT_PGI           = 0x00800000;
const uint GL_MAT_COLOR_INDEXES_BIT_PGI      = 0x01000000;
const uint GL_MAT_SHININESS_BIT_PGI          = 0x02000000;
const uint GL_MAT_SPECULAR_BIT_PGI           = 0x04000000;
const uint GL_NORMAL_BIT_PGI                 = 0x08000000;
const uint GL_TEXCOORD1_BIT_PGI              = 0x10000000;
const uint GL_TEXCOORD2_BIT_PGI              = 0x20000000;
const uint GL_TEXCOORD3_BIT_PGI              = 0x40000000;
const uint GL_TEXCOORD4_BIT_PGI              = 0x80000000;
const uint GL_VERTEX23_BIT_PGI               = 0x00000004;
const uint GL_VERTEX4_BIT_PGI                = 0x00000008;



/*-----------------------------------------------------------------------
 * REND
 *----------------------------------------------------------------------*/

const uint GL_REND_screen_coordinates = 1;
const uint GL_SCREEN_COORDINATES_REND        = 0x8490;
const uint GL_INVERTED_SCREEN_W_REND         = 0x8491;



/*-----------------------------------------------------------------------
 * SGI (Silicon Graphics Inc)
 *----------------------------------------------------------------------*/

const uint GL_SGI_color_matrix = 1;
const uint GL_COLOR_MATRIX_SGI                   = 0x80B1;
const uint GL_COLOR_MATRIX_STACK_DEPTH_SGI       = 0x80B2;
const uint GL_MAX_COLOR_MATRIX_STACK_DEPTH_SGI   = 0x80B3;
const uint GL_POST_COLOR_MATRIX_RED_SCALE_SGI    = 0x80B4;
const uint GL_POST_COLOR_MATRIX_GREEN_SCALE_SGI  = 0x80B5;
const uint GL_POST_COLOR_MATRIX_BLUE_SCALE_SGI   = 0x80B6;
const uint GL_POST_COLOR_MATRIX_ALPHA_SCALE_SGI  = 0x80B7;
const uint GL_POST_COLOR_MATRIX_RED_BIAS_SGI     = 0x80B8;
const uint GL_POST_COLOR_MATRIX_GREEN_BIAS_SGI   = 0x80B9;
const uint GL_POST_COLOR_MATRIX_BLUE_BIAS_SGI    = 0x80BA;
const uint GL_POST_COLOR_MATRIX_ALPHA_BIAS_SGI   = 0x80BB;

const uint GL_SGI_color_table = 1;
const uint GL_COLOR_TABLE_SGI                         = 0x80D0;
const uint GL_POST_CONVOLUTION_COLOR_TABLE_SGI        = 0x80D1;
const uint GL_POST_COLOR_MATRIX_COLOR_TABLE_SGI       = 0x80D2;
const uint GL_PROXY_COLOR_TABLE_SGI                   = 0x80D3;
const uint GL_PROXY_POST_CONVOLUTION_COLOR_TABLE_SGI  = 0x80D4;
const uint GL_PROXY_POST_COLOR_MATRIX_COLOR_TABLE_SGI = 0x80D5;
const uint GL_COLOR_TABLE_SCALE_SGI                   = 0x80D6;
const uint GL_COLOR_TABLE_BIAS_SGI                    = 0x80D7;
const uint GL_COLOR_TABLE_FORMAT_SGI                  = 0x80D8;
const uint GL_COLOR_TABLE_WIDTH_SGI                   = 0x80D9;
const uint GL_COLOR_TABLE_RED_SIZE_SGI                = 0x80DA;
const uint GL_COLOR_TABLE_GREEN_SIZE_SGI              = 0x80DB;
const uint GL_COLOR_TABLE_BLUE_SIZE_SGI               = 0x80DC;
const uint GL_COLOR_TABLE_ALPHA_SIZE_SGI              = 0x80DD;
const uint GL_COLOR_TABLE_LUMINANCE_SIZE_SGI          = 0x80DE;
const uint GL_COLOR_TABLE_INTENSITY_SIZE_SGI          = 0x80DF;

const uint GL_SGI_depth_pass_instrument = 1;
const uint GL_DEPTH_PASS_INSTRUMENT_SGIX          = 0x8310;
const uint GL_DEPTH_PASS_INSTRUMENT_COUNTERS_SGIX = 0x8311;
const uint GL_DEPTH_PASS_INSTRUMENT_MAX_SGIX      = 0x8312;

const uint GL_SGI_texture_color_table = 1;
const uint GL_TEXTURE_COLOR_TABLE_SGI        = 0x80BC;
const uint GL_PROXY_TEXTURE_COLOR_TABLE_SGI  = 0x80BD;

const uint GL_SGIS_detail_texture = 1;
const uint GL_DETAIL_TEXTURE_2D_SGIS          = 0x8095;
const uint GL_DETAIL_TEXTURE_2D_BINDING_SGIS  = 0x8096;
const uint GL_LINEAR_DETAIL_SGIS              = 0x8097;
const uint GL_LINEAR_DETAIL_ALPHA_SGIS        = 0x8098;
const uint GL_LINEAR_DETAIL_COLOR_SGIS        = 0x8099;
const uint GL_DETAIL_TEXTURE_LEVEL_SGIS       = 0x809A;
const uint GL_DETAIL_TEXTURE_MODE_SGIS        = 0x809B;
const uint GL_DETAIL_TEXTURE_FUNC_POINTS_SGIS = 0x809C;

const uint GL_SGIS_fog_function = 1;
const uint GL_FOG_FUNC_SGIS                  = 0x812A;
const uint GL_FOG_FUNC_POINTS_SGIS           = 0x812B;
const uint GL_MAX_FOG_FUNC_POINTS_SGIS       = 0x812C;

const uint GL_SGIS_generate_mipmap = 1;
const uint GL_GENERATE_MIPMAP_SGIS           = 0x8191;
const uint GL_GENERATE_MIPMAP_HINT_SGIS      = 0x8192;

const uint GL_SGIS_multisample = 1;
const uint GL_MULTISAMPLE_SGIS               = 0x809D;
const uint GL_SAMPLE_ALPHA_TO_MASK_SGIS      = 0x809E;
const uint GL_SAMPLE_ALPHA_TO_ONE_SGIS       = 0x809F;
const uint GL_SAMPLE_MASK_SGIS               = 0x80A0;
const uint GL_1PASS_SGIS                     = 0x80A1;
const uint GL_2PASS_0_SGIS                   = 0x80A2;
const uint GL_2PASS_1_SGIS                   = 0x80A3;
const uint GL_4PASS_0_SGIS                   = 0x80A4;
const uint GL_4PASS_1_SGIS                   = 0x80A5;
const uint GL_4PASS_2_SGIS                   = 0x80A6;
const uint GL_4PASS_3_SGIS                   = 0x80A7;
const uint GL_SAMPLE_BUFFERS_SGIS            = 0x80A8;
const uint GL_SAMPLES_SGIS                   = 0x80A9;
const uint GL_SAMPLE_MASK_VALUE_SGIS         = 0x80AA;
const uint GL_SAMPLE_MASK_INVERT_SGIS        = 0x80AB;
const uint GL_SAMPLE_PATTERN_SGIS            = 0x80AC;

const uint GL_SGIS_pixel_texture = 1;
const uint GL_PIXEL_TEXTURE_SGIS               = 0x8353;
const uint GL_PIXEL_FRAGMENT_RGB_SOURCE_SGIS   = 0x8354;
const uint GL_PIXEL_FRAGMENT_ALPHA_SOURCE_SGIS = 0x8355;
const uint GL_PIXEL_GROUP_COLOR_SGIS           = 0x8356;

const uint GL_SGIS_point_line_texgen = 1;
const uint GL_EYE_DISTANCE_TO_POINT_SGIS     = 0x81F0;
const uint GL_OBJECT_DISTANCE_TO_POINT_SGIS  = 0x81F1;
const uint GL_EYE_DISTANCE_TO_LINE_SGIS      = 0x81F2;
const uint GL_OBJECT_DISTANCE_TO_LINE_SGIS   = 0x81F3;
const uint GL_EYE_POINT_SGIS                 = 0x81F4;
const uint GL_OBJECT_POINT_SGIS              = 0x81F5;
const uint GL_EYE_LINE_SGIS                  = 0x81F6;
const uint GL_OBJECT_LINE_SGIS               = 0x81F7;

const uint GL_SGIS_sharpen_texture = 1;
const uint GL_LINEAR_SHARPEN_SGIS              = 0x80AD;
const uint GL_LINEAR_SHARPEN_ALPHA_SGIS        = 0x80AE;
const uint GL_LINEAR_SHARPEN_COLOR_SGIS        = 0x80AF;
const uint GL_SHARPEN_TEXTURE_FUNC_POINTS_SGIS = 0x80B0;

const uint GL_SGIS_texture_border_clamp = 1;
const uint GL_CLAMP_TO_BORDER_SGIS           = 0x812D;

const uint GL_SGIS_texture_color_mask = 1;
const uint GL_TEXTURE_COLOR_WRITEMASK_SGIS   = 0x81EF;

const uint GL_SGIS_texture_edge_clamp = 1;
const uint GL_CLAMP_TO_EDGE_SGIS             = 0x812F;

const uint GL_SGIS_texture_filter4 = 1;
const uint GL_FILTER4_SGIS                   = 0x8146;
const uint GL_TEXTURE_FILTER4_SIZE_SGIS      = 0x8147;

const uint GL_SGIS_texture_lod = 1;
const uint GL_TEXTURE_MIN_LOD_SGIS           = 0x813A;
const uint GL_TEXTURE_MAX_LOD_SGIS           = 0x813B;
const uint GL_TEXTURE_BASE_LEVEL_SGIS        = 0x813C;
const uint GL_TEXTURE_MAX_LEVEL_SGIS         = 0x813D;

const uint GL_SGIS_texture_select = 1;
const uint GL_DUAL_ALPHA4_SGIS               = 0x8110;
const uint GL_DUAL_ALPHA8_SGIS               = 0x8111;
const uint GL_DUAL_ALPHA12_SGIS              = 0x8112;
const uint GL_DUAL_ALPHA16_SGIS              = 0x8113;
const uint GL_DUAL_LUMINANCE4_SGIS           = 0x8114;
const uint GL_DUAL_LUMINANCE8_SGIS           = 0x8115;
const uint GL_DUAL_LUMINANCE12_SGIS          = 0x8116;
const uint GL_DUAL_LUMINANCE16_SGIS          = 0x8117;
const uint GL_DUAL_INTENSITY4_SGIS           = 0x8118;
const uint GL_DUAL_INTENSITY8_SGIS           = 0x8119;
const uint GL_DUAL_INTENSITY12_SGIS          = 0x811A;
const uint GL_DUAL_INTENSITY16_SGIS          = 0x811B;
const uint GL_DUAL_LUMINANCE_ALPHA4_SGIS     = 0x811C;
const uint GL_DUAL_LUMINANCE_ALPHA8_SGIS     = 0x811D;
const uint GL_QUAD_ALPHA4_SGIS               = 0x811E;
const uint GL_QUAD_ALPHA8_SGIS               = 0x811F;
const uint GL_QUAD_LUMINANCE4_SGIS           = 0x8120;
const uint GL_QUAD_LUMINANCE8_SGIS           = 0x8121;
const uint GL_QUAD_INTENSITY4_SGIS           = 0x8122;
const uint GL_QUAD_INTENSITY8_SGIS           = 0x8123;
const uint GL_DUAL_TEXTURE_SELECT_SGIS       = 0x8124;
const uint GL_QUAD_TEXTURE_SELECT_SGIS       = 0x8125;

const uint GL_SGIS_texture4D = 1;
const uint GL_PACK_SKIP_VOLUMES_SGIS         = 0x8130;
const uint GL_PACK_IMAGE_DEPTH_SGIS          = 0x8131;
const uint GL_UNPACK_SKIP_VOLUMES_SGIS       = 0x8132;
const uint GL_UNPACK_IMAGE_DEPTH_SGIS        = 0x8133;
const uint GL_TEXTURE_4D_SGIS                = 0x8134;
const uint GL_PROXY_TEXTURE_4D_SGIS          = 0x8135;
const uint GL_TEXTURE_4DSIZE_SGIS            = 0x8136;
const uint GL_TEXTURE_WRAP_Q_SGIS            = 0x8137;
const uint GL_MAX_4D_TEXTURE_SIZE_SGIS       = 0x8138;
const uint GL_TEXTURE_4D_BINDING_SGIS        = 0x814F;

const uint GL_SGIX_async = 1;
const uint GL_ASYNC_MARKER_SGIX              = 0x8329;

const uint GL_SGIX_async_histogram = 1;
const uint GL_ASYNC_HISTOGRAM_SGIX           = 0x832C;
const uint GL_MAX_ASYNC_HISTOGRAM_SGIX       = 0x832D;

const uint GL_SGIX_async_pixel = 1;
const uint GL_ASYNC_TEX_IMAGE_SGIX           = 0x835C;
const uint GL_ASYNC_DRAW_PIXELS_SGIX         = 0x835D;
const uint GL_ASYNC_READ_PIXELS_SGIX         = 0x835E;
const uint GL_MAX_ASYNC_TEX_IMAGE_SGIX       = 0x835F;
const uint GL_MAX_ASYNC_DRAW_PIXELS_SGIX     = 0x8360;
const uint GL_MAX_ASYNC_READ_PIXELS_SGIX     = 0x8361;

const uint GL_SGIX_blend_alpha_minmax = 1;
const uint GL_ALPHA_MIN_SGIX                 = 0x8320;
const uint GL_ALPHA_MAX_SGIX                 = 0x8321;

const uint GL_SGIX_calligraphic_fragment = 1;
const uint GL_CALLIGRAPHIC_FRAGMENT_SGIX     = 0x8183;

const uint GL_SGIX_clipmap = 1;
const uint GL_LINEAR_CLIPMAP_LINEAR_SGIX         = 0x8170;
const uint GL_TEXTURE_CLIPMAP_CENTER_SGIX        = 0x8171;
const uint GL_TEXTURE_CLIPMAP_FRAME_SGIX         = 0x8172;
const uint GL_TEXTURE_CLIPMAP_OFFSET_SGIX        = 0x8173;
const uint GL_TEXTURE_CLIPMAP_VIRTUAL_DEPTH_SGIX = 0x8174;
const uint GL_TEXTURE_CLIPMAP_LOD_OFFSET_SGIX    = 0x8175;
const uint GL_TEXTURE_CLIPMAP_DEPTH_SGIX         = 0x8176;
const uint GL_MAX_CLIPMAP_DEPTH_SGIX             = 0x8177;
const uint GL_MAX_CLIPMAP_VIRTUAL_DEPTH_SGIX     = 0x8178;
const uint GL_NEAREST_CLIPMAP_NEAREST_SGIX       = 0x844D;
const uint GL_NEAREST_CLIPMAP_LINEAR_SGIX        = 0x844E;
const uint GL_LINEAR_CLIPMAP_NEAREST_SGIX        = 0x844F;

const uint GL_SGIX_convolution_accuracy = 1;
const uint GL_CONVOLUTION_HINT_SGIX          = 0x8316;

const uint GL_SGIX_depth_texture = 1;
const uint GL_DEPTH_COMPONENT16_SGIX         = 0x81A5;
const uint GL_DEPTH_COMPONENT24_SGIX         = 0x81A6;
const uint GL_DEPTH_COMPONENT32_SGIX         = 0x81A7;

const uint GL_SGIX_flush_raster = 1;

const uint GL_SGIX_fog_offset = 1;
const uint GL_FOG_OFFSET_SGIX                = 0x8198;
const uint GL_FOG_OFFSET_VALUE_SGIX          = 0x8199;

const uint GL_SGIX_fog_scale = 1;
const uint GL_FOG_SCALE_SGIX                 = 0x81FC;
const uint GL_FOG_SCALE_VALUE_SGIX           = 0x81FD;

const uint GL_SGIX_fragment_lighting = 1;
const uint GL_FRAGMENT_LIGHTING_SGIX         = 0x8400;
const uint GL_FRAGMENT_COLOR_MATERIAL_SGIX   = 0x8401;
const uint GL_FRAGMENT_COLOR_MATERIAL_FACE_SGIX = 0x8402;
const uint GL_FRAGMENT_COLOR_MATERIAL_PARAMETER_SGIX = 0x8403;
const uint GL_MAX_FRAGMENT_LIGHTS_SGIX       = 0x8404;
const uint GL_MAX_ACTIVE_LIGHTS_SGIX         = 0x8405;
const uint GL_CURRENT_RASTER_NORMAL_SGIX     = 0x8406;
const uint GL_LIGHT_ENV_MODE_SGIX            = 0x8407;
const uint GL_FRAGMENT_LIGHT_MODEL_LOCAL_VIEWER_SGIX = 0x8408;
const uint GL_FRAGMENT_LIGHT_MODEL_TWO_SIDE_SGIX = 0x8409;
const uint GL_FRAGMENT_LIGHT_MODEL_AMBIENT_SGIX = 0x840A;
const uint GL_FRAGMENT_LIGHT_MODEL_NORMAL_INTERPOLATION_SGIX = 0x840B;
const uint GL_FRAGMENT_LIGHT0_SGIX           = 0x840C;
const uint GL_FRAGMENT_LIGHT1_SGIX           = 0x840D;
const uint GL_FRAGMENT_LIGHT2_SGIX           = 0x840E;
const uint GL_FRAGMENT_LIGHT3_SGIX           = 0x840F;
const uint GL_FRAGMENT_LIGHT4_SGIX           = 0x8410;
const uint GL_FRAGMENT_LIGHT5_SGIX           = 0x8411;
const uint GL_FRAGMENT_LIGHT6_SGIX           = 0x8412;
const uint GL_FRAGMENT_LIGHT7_SGIX           = 0x8413;

const uint GL_SGIX_framezoom = 1;
const uint GL_FRAMEZOOM_SGIX                 = 0x818B;
const uint GL_FRAMEZOOM_FACTOR_SGIX          = 0x818C;
const uint GL_MAX_FRAMEZOOM_FACTOR_SGIX      = 0x818D;

const uint GL_SGIX_impact_pixel_texture = 1;
const uint GL_PIXEL_TEX_GEN_Q_CEILING_SGIX        = 0x8184;
const uint GL_PIXEL_TEX_GEN_Q_ROUND_SGIX          = 0x8185;
const uint GL_PIXEL_TEX_GEN_Q_FLOOR_SGIX          = 0x8186;
const uint GL_PIXEL_TEX_GEN_ALPHA_REPLACE_SGIX    = 0x8187;
const uint GL_PIXEL_TEX_GEN_ALPHA_NO_REPLACE_SGIX = 0x8188;
const uint GL_PIXEL_TEX_GEN_ALPHA_LS_SGIX         = 0x8189;
const uint GL_PIXEL_TEX_GEN_ALPHA_MS_SGIX         = 0x818A;

const uint GL_SGIX_instruments = 1;
const uint GL_INSTRUMENT_BUFFER_POINTER_SGIX = 0x8180;
const uint GL_INSTRUMENT_MEASUREMENTS_SGIX   = 0x8181;

const uint GL_SGIX_interlace = 1;
const uint GL_INTERLACE_SGIX                 = 0x8094;

const uint GL_SGIX_ir_instrument1 = 1;
const uint GL_IR_INSTRUMENT1_SGIX            = 0x817F;

const uint GL_SGIX_list_priority = 1;
const uint GL_LIST_PRIORITY_SGIX             = 0x8182;

const uint GL_SGIX_pixel_texture = 1;
const uint GL_PIXEL_TEX_GEN_SGIX             = 0x8139;
const uint GL_PIXEL_TEX_GEN_MODE_SGIX        = 0x832B;

const uint GL_SGIX_pixel_tiles = 1;
const uint GL_PIXEL_TILE_BEST_ALIGNMENT_SGIX  = 0x813E;
const uint GL_PIXEL_TILE_CACHE_INCREMENT_SGIX = 0x813F;
const uint GL_PIXEL_TILE_WIDTH_SGIX           = 0x8140;
const uint GL_PIXEL_TILE_HEIGHT_SGIX          = 0x8141;
const uint GL_PIXEL_TILE_GRID_WIDTH_SGIX      = 0x8142;
const uint GL_PIXEL_TILE_GRID_HEIGHT_SGIX     = 0x8143;
const uint GL_PIXEL_TILE_GRID_DEPTH_SGIX      = 0x8144;
const uint GL_PIXEL_TILE_CACHE_SIZE_SGIX      = 0x8145;

const uint GL_SGIX_polynomial_ffd = 1;
const uint GL_GEOMETRY_DEFORMATION_SGIX      = 0x8194;
const uint GL_TEXTURE_DEFORMATION_SGIX       = 0x8195;
const uint GL_DEFORMATIONS_MASK_SGIX         = 0x8196;
const uint GL_MAX_DEFORMATION_ORDER_SGIX     = 0x8197;

const uint GL_SGIX_reference_plane = 1;
const uint GL_REFERENCE_PLANE_SGIX           = 0x817D;
const uint GL_REFERENCE_PLANE_EQUATION_SGIX  = 0x817E;

const uint GL_SGIX_resample = 1;
const uint GL_PACK_RESAMPLE_SGIX             = 0x842C;
const uint GL_UNPACK_RESAMPLE_SGIX           = 0x842D;
const uint GL_RESAMPLE_REPLICATE_SGIX        = 0x842E;
const uint GL_RESAMPLE_ZERO_FILL_SGIX        = 0x842F;
const uint GL_RESAMPLE_DECIMATE_SGIX         = 0x8430;

const uint GL_SGIX_scalebias_hint = 1;
const uint GL_SCALEBIAS_HINT_SGIX            = 0x8322;

const uint GL_SGIX_shadow = 1;
const uint GL_TEXTURE_COMPARE_SGIX           = 0x819A;
const uint GL_TEXTURE_COMPARE_OPERATOR_SGIX  = 0x819B;
const uint GL_TEXTURE_LEQUAL_R_SGIX          = 0x819C;
const uint GL_TEXTURE_GEQUAL_R_SGIX          = 0x819D;

const uint GL_SGIX_shadow_ambient = 1;
const uint GL_SHADOW_AMBIENT_SGIX            = 0x80BF;

const uint GL_SGIX_sprite = 1;
const uint GL_SPRITE_SGIX                    = 0x8148;
const uint GL_SPRITE_MODE_SGIX               = 0x8149;
const uint GL_SPRITE_AXIS_SGIX               = 0x814A;
const uint GL_SPRITE_TRANSLATION_SGIX        = 0x814B;
const uint GL_SPRITE_AXIAL_SGIX              = 0x814C;
const uint GL_SPRITE_OBJECT_ALIGNED_SGIX     = 0x814D;
const uint GL_SPRITE_EYE_ALIGNED_SGIX        = 0x814E;

const uint GL_SGIX_subsample = 1;
const uint GL_PACK_SUBSAMPLE_RATE_SGIX       = 0x85A0;
const uint GL_UNPACK_SUBSAMPLE_RATE_SGIX     = 0x85A1;
const uint GL_PIXEL_SUBSAMPLE_4444_SGIX      = 0x85A2;
const uint GL_PIXEL_SUBSAMPLE_2424_SGIX      = 0x85A3;
const uint GL_PIXEL_SUBSAMPLE_4242_SGIX      = 0x85A4;

const uint GL_SGIX_tag_sample_buffer = 1;

const uint GL_SGIX_texture_add_env = 1;
const uint GL_TEXTURE_ENV_BIAS_SGIX          = 0x80BE;

const uint GL_SGIX_texture_coordinate_clamp = 1;
const uint GL_TEXTURE_MAX_CLAMP_S_SGIX       = 0x8369;
const uint GL_TEXTURE_MAX_CLAMP_T_SGIX       = 0x836A;
const uint GL_TEXTURE_MAX_CLAMP_R_SGIX       = 0x836B;

const uint GL_SGIX_texture_lod_bias = 1;
const uint GL_TEXTURE_LOD_BIAS_S_SGIX        = 0x818E;
const uint GL_TEXTURE_LOD_BIAS_T_SGIX        = 0x818F;
const uint GL_TEXTURE_LOD_BIAS_R_SGIX        = 0x8190;

const uint GL_SGIX_texture_multi_buffer = 1;
const uint GL_TEXTURE_MULTI_BUFFER_HINT_SGIX = 0x812E;

const uint GL_SGIX_texture_scale_bias = 1;
const uint GL_POST_TEXTURE_FILTER_BIAS_SGIX        = 0x8179;
const uint GL_POST_TEXTURE_FILTER_SCALE_SGIX       = 0x817A;
const uint GL_POST_TEXTURE_FILTER_BIAS_RANGE_SGIX  = 0x817B;
const uint GL_POST_TEXTURE_FILTER_SCALE_RANGE_SGIX = 0x817C;

const uint GL_SGIX_vertex_preclip = 1;
const uint GL_VERTEX_PRECLIP_SGIX            = 0x83EE;
const uint GL_VERTEX_PRECLIP_HINT_SGIX       = 0x83EF;

const uint GL_SGIX_ycrcb = 1;
const uint GL_YCRCB_422_SGIX                 = 0x81BB;
const uint GL_YCRCB_444_SGIX                 = 0x81BC;

const uint GL_SGIX_ycrcb_subsample = 1;

const uint GL_SGIX_ycrcba = 1;
const uint GL_YCRCB_SGIX                     = 0x8318;
const uint GL_YCRCBA_SGIX                    = 0x8319;



/*-----------------------------------------------------------------------
 * SUN
 *----------------------------------------------------------------------*/

const uint GL_SUN_convolution_border_modes = 1;
const uint GL_WRAP_BORDER_SUN                = 0x81D4;

const uint GL_SUN_global_alpha = 1;
const uint GL_GLOBAL_ALPHA_SUN               = 0x81D9;
const uint GL_GLOBAL_ALPHA_FACTOR_SUN        = 0x81DA;

const uint GL_SUN_mesh_array = 1;
const uint GL_QUAD_MESH_SUN                  = 0x8614;
const uint GL_TRIANGLE_MESH_SUN              = 0x8615;

const uint GL_SUN_slice_accum = 1;
const uint GL_SLICE_ACCUM_SUN                = 0x85CC;

const uint GL_SUN_triangle_list = 1;
const uint GL_RESTART_SUN                        = 0x0001;
const uint GL_REPLACE_MIDDLE_SUN                 = 0x0002;
const uint GL_REPLACE_OLDEST_SUN                 = 0x0003;
const uint GL_TRIANGLE_LIST_SUN                  = 0x81D7;
const uint GL_REPLACEMENT_CODE_SUN               = 0x81D8;
const uint GL_REPLACEMENT_CODE_ARRAY_SUN         = 0x85C0;
const uint GL_REPLACEMENT_CODE_ARRAY_TYPE_SUN    = 0x85C1;
const uint GL_REPLACEMENT_CODE_ARRAY_STRIDE_SUN  = 0x85C2;
const uint GL_REPLACEMENT_CODE_ARRAY_POINTER_SUN = 0x85C3;
const uint GL_R1UI_V3F_SUN                       = 0x85C4;
const uint GL_R1UI_C4UB_V3F_SUN                  = 0x85C5;
const uint GL_R1UI_C3F_V3F_SUN                   = 0x85C6;
const uint GL_R1UI_N3F_V3F_SUN                   = 0x85C7;
const uint GL_R1UI_C4F_N3F_V3F_SUN               = 0x85C8;
const uint GL_R1UI_T2F_V3F_SUN                   = 0x85C9;
const uint GL_R1UI_T2F_N3F_V3F_SUN               = 0x85CA;
const uint GL_R1UI_T2F_C4F_N3F_V3F_SUN           = 0x85CB;

const uint GL_SUN_vertex = 1;

const uint GL_SUNX_constant_data = 1;
const uint GL_UNPACK_CONSTANT_DATA_SUNX      = 0x81D5;
const uint GL_TEXTURE_CONSTANT_DATA_SUNX     = 0x81D6;



/*-----------------------------------------------------------------------
 * WIN (Microsoft)
 *----------------------------------------------------------------------*/

const uint GL_WIN_phong_shading = 1;
const uint GL_PHONG_WIN                      = 0x80EA;
const uint GL_PHONG_HINT_WIN                 = 0x80EB;

const uint GL_WIN_specular_fog = 1;
const uint GL_FOG_SPECULAR_TEXTURE_WIN       = 0x80EC;



/************************************************************************
 *
 * Extension function pointer types
 *
 * Note: Proper OpenGL extension checking must be performed to use these.
 *
 ************************************************************************/

/* GL_ARB_multitexture */
typedef void (* PFNGLACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (* PFNGLCLIENTACTIVETEXTUREARBPROC) (GLenum texture);
typedef void (* PFNGLMULTITEXCOORD1DARBPROC) (GLenum target, GLdouble s);
typedef void (* PFNGLMULTITEXCOORD1DVARBPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD1FARBPROC) (GLenum target, GLfloat s);
typedef void (* PFNGLMULTITEXCOORD1FVARBPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD1IARBPROC) (GLenum target, GLint s);
typedef void (* PFNGLMULTITEXCOORD1IVARBPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD1SARBPROC) (GLenum target, GLshort s);
typedef void (* PFNGLMULTITEXCOORD1SVARBPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLMULTITEXCOORD2DARBPROC) (GLenum target, GLdouble s, GLdouble t);
typedef void (* PFNGLMULTITEXCOORD2DVARBPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD2FARBPROC) (GLenum target, GLfloat s, GLfloat t);
typedef void (* PFNGLMULTITEXCOORD2FVARBPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD2IARBPROC) (GLenum target, GLint s, GLint t);
typedef void (* PFNGLMULTITEXCOORD2IVARBPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD2SARBPROC) (GLenum target, GLshort s, GLshort t);
typedef void (* PFNGLMULTITEXCOORD2SVARBPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLMULTITEXCOORD3DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r);
typedef void (* PFNGLMULTITEXCOORD3DVARBPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD3FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r);
typedef void (* PFNGLMULTITEXCOORD3FVARBPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD3IARBPROC) (GLenum target, GLint s, GLint t, GLint r);
typedef void (* PFNGLMULTITEXCOORD3IVARBPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD3SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r);
typedef void (* PFNGLMULTITEXCOORD3SVARBPROC) (GLenum target, GLshort *v);
typedef void (* PFNGLMULTITEXCOORD4DARBPROC) (GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
typedef void (* PFNGLMULTITEXCOORD4DVARBPROC) (GLenum target, GLdouble *v);
typedef void (* PFNGLMULTITEXCOORD4FARBPROC) (GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
typedef void (* PFNGLMULTITEXCOORD4FVARBPROC) (GLenum target, GLfloat *v);
typedef void (* PFNGLMULTITEXCOORD4IARBPROC) (GLenum target, GLint s, GLint t, GLint r, GLint q);
typedef void (* PFNGLMULTITEXCOORD4IVARBPROC) (GLenum target, GLint *v);
typedef void (* PFNGLMULTITEXCOORD4SARBPROC) (GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
typedef void (* PFNGLMULTITEXCOORD4SVARBPROC) (GLenum target, GLshort *v);

/* GL_ARB_transpose_matrix */
typedef void (* PFNGLLOADTRANSPOSEMATRIXFARBPROC) (GLfloat *m);
typedef void (* PFNGLLOADTRANSPOSEMATRIXDARBPROC) (GLdouble *m);
typedef void (* PFNGLMULTTRANSPOSEMATRIXFARBPROC) (GLfloat *m);
typedef void (* PFNGLMULTTRANSPOSEMATRIXDARBPROC) (GLdouble *m);

/* GL_ARB_multisample */
typedef void (* PFNGLSAMPLECOVERAGEARBPROC) (GLclampf value, GLboolean invert);

/* GL_ARB_texture_compression */
typedef void (* PFNGLCOMPRESSEDTEXIMAGE3DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXIMAGE2DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXIMAGE1DARBPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXSUBIMAGE3DARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXSUBIMAGE2DARBPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLCOMPRESSEDTEXSUBIMAGE1DARBPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, GLvoid *data);
typedef void (* PFNGLGETCOMPRESSEDTEXIMAGEARBPROC) (GLenum target, GLint level, void *img);

/* GL_ARB_point_parameters */
typedef void (* PFNGLPOINTPARAMETERFARBPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLPOINTPARAMETERFVARBPROC) (GLenum pname, GLfloat *params);

/* GL_ARB_vertex_blend */
typedef void (* PFNGLWEIGHTBVARBPROC) (GLint size, GLbyte *weights);
typedef void (* PFNGLWEIGHTSVARBPROC) (GLint size, GLshort *weights);
typedef void (* PFNGLWEIGHTIVARBPROC) (GLint size, GLint *weights);
typedef void (* PFNGLWEIGHTFVARBPROC) (GLint size, GLfloat *weights);
typedef void (* PFNGLWEIGHTDVARBPROC) (GLint size, GLdouble *weights);
typedef void (* PFNGLWEIGHTUBVARBPROC) (GLint size, GLubyte *weights);
typedef void (* PFNGLWEIGHTUSVARBPROC) (GLint size, GLushort *weights);
typedef void (* PFNGLWEIGHTUIVARBPROC) (GLint size, GLuint *weights);
typedef void (* PFNGLWEIGHTPOINTERARBPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (* PFNGLVERTEXBLENDARBPROC) (GLint count);

/* GL_ARB_matrix_palette */
typedef void (* PFNGLCURRENTPALETTEMATRIXARBPROC) (GLint index);
typedef void (* PFNGLMATRIXINDEXUBVARBPROC) (GLint size, GLubyte *indices);
typedef void (* PFNGLMATRIXINDEXUSVARBPROC) (GLint size, GLushort *indices);
typedef void (* PFNGLMATRIXINDEXUIVARBPROC) (GLint size, GLuint *indices);
typedef void (* PFNGLMATRIXINDEXPOINTERARBPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);

/* GL_ARB_window_pos */
typedef void (* PFNGLWINDOWPOS2DARBPROC) (GLdouble x, GLdouble y);
typedef void (* PFNGLWINDOWPOS2DVARBPROC) (GLdouble *v);
typedef void (* PFNGLWINDOWPOS2FARBPROC) (GLfloat x, GLfloat y);
typedef void (* PFNGLWINDOWPOS2FVARBPROC) (GLfloat *v);
typedef void (* PFNGLWINDOWPOS2IARBPROC) (GLint x, GLint y);
typedef void (* PFNGLWINDOWPOS2IVARBPROC) (GLint *v);
typedef void (* PFNGLWINDOWPOS2SARBPROC) (GLshort x, GLshort y);
typedef void (* PFNGLWINDOWPOS2SVARBPROC) (GLshort *v);
typedef void (* PFNGLWINDOWPOS3DARBPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (* PFNGLWINDOWPOS3DVARBPROC) (GLdouble *v);
typedef void (* PFNGLWINDOWPOS3FARBPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLWINDOWPOS3FVARBPROC) (GLfloat *v);
typedef void (* PFNGLWINDOWPOS3IARBPROC) (GLint x, GLint y, GLint z);
typedef void (* PFNGLWINDOWPOS3IVARBPROC) (GLint *v);
typedef void (* PFNGLWINDOWPOS3SARBPROC) (GLshort x, GLshort y, GLshort z);
typedef void (* PFNGLWINDOWPOS3SVARBPROC) (GLshort *v);

/* GL_ARB_vertex_program */
typedef void (* PFNGLVERTEXATTRIB1SARBPROC) (GLuint index, GLshort x);
typedef void (* PFNGLVERTEXATTRIB1FARBPROC) (GLuint index, GLfloat x);
typedef void (* PFNGLVERTEXATTRIB1DARBPROC) (GLuint index, GLdouble x);
typedef void (* PFNGLVERTEXATTRIB2SARBPROC) (GLuint index, GLshort x, GLshort y);
typedef void (* PFNGLVERTEXATTRIB2FARBPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (* PFNGLVERTEXATTRIB2DARBPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (* PFNGLVERTEXATTRIB3SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (* PFNGLVERTEXATTRIB3FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLVERTEXATTRIB3DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (* PFNGLVERTEXATTRIB4SARBPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (* PFNGLVERTEXATTRIB4FARBPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLVERTEXATTRIB4DARBPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLVERTEXATTRIB4NUBARBPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (* PFNGLVERTEXATTRIB1SVARBPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB1FVARBPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB1DVARBPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB2SVARBPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB2FVARBPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB2DVARBPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB3SVARBPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB3FVARBPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB3DVARBPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB4BVARBPROC) (GLuint index, GLbyte *v);
typedef void (* PFNGLVERTEXATTRIB4SVARBPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB4IVARBPROC) (GLuint index, GLint *v);
typedef void (* PFNGLVERTEXATTRIB4UBVARBPROC) (GLuint index, GLubyte *v);
typedef void (* PFNGLVERTEXATTRIB4USVARBPROC) (GLuint index, GLushort *v);
typedef void (* PFNGLVERTEXATTRIB4UIVARBPROC) (GLuint index, GLuint *v);
typedef void (* PFNGLVERTEXATTRIB4FVARBPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB4DVARBPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB4NBVARBPROC) (GLuint index, GLbyte *v);
typedef void (* PFNGLVERTEXATTRIB4NSVARBPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB4NIVARBPROC) (GLuint index, GLint *v);
typedef void (* PFNGLVERTEXATTRIB4NUBVARBPROC) (GLuint index, GLubyte *v);
typedef void (* PFNGLVERTEXATTRIB4NUSVARBPROC) (GLuint index, GLushort *v);
typedef void (* PFNGLVERTEXATTRIB4NUIVARBPROC) (GLuint index, GLuint *v);
typedef void (* PFNGLVERTEXATTRIBPOINTERARBPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, GLvoid *pointer);
typedef void (* PFNGLENABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (* PFNGLDISABLEVERTEXATTRIBARRAYARBPROC) (GLuint index);
typedef void (* PFNGLPROGRAMSTRINGARBPROC) (GLenum target, GLenum format, GLsizei len, GLvoid *string);
typedef void (* PFNGLBINDPROGRAMARBPROC) (GLenum target, GLuint program);
typedef void (* PFNGLDELETEPROGRAMSARBPROC) (GLsizei n, GLuint *programs);
typedef void (* PFNGLGENPROGRAMSARBPROC) (GLsizei n, GLuint *programs);
typedef void (* PFNGLPROGRAMENVPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLPROGRAMENVPARAMETER4DVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (* PFNGLPROGRAMENVPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLPROGRAMENVPARAMETER4FVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (* PFNGLPROGRAMLOCALPARAMETER4DARBPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLPROGRAMLOCALPARAMETER4DVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (* PFNGLPROGRAMLOCALPARAMETER4FARBPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLPROGRAMLOCALPARAMETER4FVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (* PFNGLGETPROGRAMENVPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (* PFNGLGETPROGRAMENVPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (* PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC) (GLenum target, GLuint index, GLdouble *params);
typedef void (* PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC) (GLenum target, GLuint index, GLfloat *params);
typedef void (* PFNGLGETPROGRAMIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETPROGRAMSTRINGARBPROC) (GLenum target, GLenum pname, GLvoid *string);
typedef void (* PFNGLGETVERTEXATTRIBDVARBPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (* PFNGLGETVERTEXATTRIBFVARBPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETVERTEXATTRIBIVARBPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (* PFNGLGETVERTEXATTRIBPOINTERVARBPROC) (GLuint index, GLenum pname, GLvoid **pointer);
typedef GLboolean (* PFNGLISPROGRAMARBPROC) (GLuint program);

/* GL_ARB_vertex_buffer_object */
typedef int GLsizeiptrARB;
typedef int GLintptrARB;
typedef void (* PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (* PFNGLDELETEBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (* PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef GLboolean (* PFNGLISBUFFERARBPROC) (GLuint buffer);
typedef void (* PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptrARB size, void *data, GLenum usage);
typedef void (* PFNGLBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, void *data);
typedef void (* PFNGLGETBUFFERSUBDATAARBPROC) (GLenum target, GLintptrARB offset, GLsizeiptrARB size, void *data);
typedef void * (* PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean (* PFNGLUNMAPBUFFERARBPROC) (GLenum target);
typedef void (* PFNGLGETBUFFERPARAMETERIVARBPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETBUFFERPOINTERVARBPROC) (GLenum target, GLenum pname, void **params);

/* GL_EXT_blend_color */
typedef void (* PFNGLBLENDCOLOREXTPROC) (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);

/* GL_EXT_polygon_offset */
typedef void (* PFNGLPOLYGONOFFSETEXTPROC) (GLfloat factor, GLfloat bias);

/* GL_EXT_texture3D */
typedef void (* PFNGLTEXIMAGE3DEXTPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, GLvoid *pixels);
typedef void (* PFNGLTEXSUBIMAGE3DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLvoid *pixels);

/* GL_SGIS_texture_filter4 */
typedef void (* PFNGLGETTEXFILTERFUNCSGISPROC) (GLenum target, GLenum filter, GLfloat *weights);
typedef void (* PFNGLTEXFILTERFUNCSGISPROC) (GLenum target, GLenum filter, GLsizei n, GLfloat *weights);

/* GL_EXT_subtexture */
typedef void (* PFNGLTEXSUBIMAGE1DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, GLvoid *pixels);
typedef void (* PFNGLTEXSUBIMAGE2DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);

/* GL_EXT_copy_texture */
typedef void (* PFNGLCOPYTEXIMAGE1DEXTPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
typedef void (* PFNGLCOPYTEXIMAGE2DEXTPROC) (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (* PFNGLCOPYTEXSUBIMAGE1DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
typedef void (* PFNGLCOPYTEXSUBIMAGE2DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (* PFNGLCOPYTEXSUBIMAGE3DEXTPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);

/* GL_EXT_histogram */
typedef void (* PFNGLGETHISTOGRAMEXTPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (* PFNGLGETHISTOGRAMPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETHISTOGRAMPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETMINMAXEXTPROC) (GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid *values);
typedef void (* PFNGLGETMINMAXPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETMINMAXPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLHISTOGRAMEXTPROC) (GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
typedef void (* PFNGLMINMAXEXTPROC) (GLenum target, GLenum internalformat, GLboolean sink);
typedef void (* PFNGLRESETHISTOGRAMEXTPROC) (GLenum target);
typedef void (* PFNGLRESETMINMAXEXTPROC) (GLenum target);

/* GL_EXT_convolution */
typedef void (* PFNGLCONVOLUTIONFILTER1DEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, GLvoid *image);
typedef void (* PFNGLCONVOLUTIONFILTER2DEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *image);
typedef void (* PFNGLCONVOLUTIONPARAMETERFEXTPROC) (GLenum target, GLenum pname, GLfloat params);
typedef void (* PFNGLCONVOLUTIONPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLCONVOLUTIONPARAMETERIEXTPROC) (GLenum target, GLenum pname, GLint params);
typedef void (* PFNGLCONVOLUTIONPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLCOPYCONVOLUTIONFILTER1DEXTPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (* PFNGLCOPYCONVOLUTIONFILTER2DEXTPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (* PFNGLGETCONVOLUTIONFILTEREXTPROC) (GLenum target, GLenum format, GLenum type, GLvoid *image);
typedef void (* PFNGLGETCONVOLUTIONPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCONVOLUTIONPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETSEPARABLEFILTEREXTPROC) (GLenum target, GLenum format, GLenum type, GLvoid *row, GLvoid *column, GLvoid *span);
typedef void (* PFNGLSEPARABLEFILTER2DEXTPROC) (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *row, GLvoid *column);

/* GL_EXT_texture_object */
typedef GLboolean (* PFNGLARETEXTURESRESIDENTEXTPROC) (GLsizei n, GLuint *textures, GLboolean *residences);
typedef void (* PFNGLBINDTEXTUREEXTPROC) (GLenum target, GLuint texture);
typedef void (* PFNGLDELETETEXTURESEXTPROC) (GLsizei n, GLuint *textures);
typedef void (* PFNGLGENTEXTURESEXTPROC) (GLsizei n, GLuint *textures);
typedef GLboolean (* PFNGLISTEXTUREEXTPROC) (GLuint texture);
typedef void (* PFNGLPRIORITIZETEXTURESEXTPROC) (GLsizei n, GLuint *textures, GLclampf *priorities);

/* GL_EXT_vertex_array */
typedef void (* PFNGLARRAYELEMENTEXTPROC) (GLint i);
typedef void (* PFNGLCOLORPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, GLvoid *pointer);
typedef void (* PFNGLDRAWARRAYSEXTPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (* PFNGLEDGEFLAGPOINTEREXTPROC) (GLsizei stride, GLsizei count, GLboolean *pointer);
typedef void (* PFNGLGETPOINTERVEXTPROC) (GLenum pname, GLvoid* *params);
typedef void (* PFNGLINDEXPOINTEREXTPROC) (GLenum type, GLsizei stride, GLsizei count, GLvoid *pointer);
typedef void (* PFNGLNORMALPOINTEREXTPROC) (GLenum type, GLsizei stride, GLsizei count, GLvoid *pointer);
typedef void (* PFNGLTEXCOORDPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, GLvoid *pointer);
typedef void (* PFNGLVERTEXPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLsizei count, GLvoid *pointer);

/* GL_EXT_blend_minmax */
typedef void (* PFNGLBLENDEQUATIONEXTPROC) (GLenum mode);

/* GL_EXT_point_parameters */
typedef void (* PFNGLPOINTPARAMETERFEXTPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLPOINTPARAMETERFVEXTPROC) (GLenum pname, GLfloat *params);

/* GL_EXT_color_subtable */
typedef void (* PFNGLCOLORSUBTABLEEXTPROC) (GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, GLvoid *data);
typedef void (* PFNGLCOPYCOLORSUBTABLEEXTPROC) (GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);

/* GL_EXT_paletted_texture */
typedef void (* PFNGLCOLORTABLEEXTPROC) (GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, GLvoid *table);
typedef void (* PFNGLGETCOLORTABLEEXTPROC) (GLenum target, GLenum format, GLenum type, GLvoid *data);
typedef void (* PFNGLGETCOLORTABLEPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETCOLORTABLEPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat *params);

/* GL_EXT_index_material */
typedef void (* PFNGLINDEXMATERIALEXTPROC) (GLenum face, GLenum mode);

/* GL_EXT_index_func */
typedef void (* PFNGLINDEXFUNCEXTPROC) (GLenum func, GLclampf reference);

/* GL_EXT_compiled_vertex_array */
typedef void (* PFNGLLOCKARRAYSEXTPROC) (GLint first, GLsizei count);
typedef void (* PFNGLUNLOCKARRAYSEXTPROC) ();

/* GL_EXT_cull_vertex */
typedef void (* PFNGLCULLPARAMETERDVEXTPROC) (GLenum pname, GLdouble *params);
typedef void (* PFNGLCULLPARAMETERFVEXTPROC) (GLenum pname, GLfloat *params);

/* GL_EXT_draw_range_elements */
typedef void (* PFNGLDRAWRANGEELEMENTSEXTPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, GLvoid *indices);

/* GL_EXT_light_texture */
typedef void (* PFNGLAPPLYTEXTUREEXTPROC) (GLenum mode);
typedef void (* PFNGLTEXTURELIGHTEXTPROC) (GLenum pname);
typedef void (* PFNGLTEXTUREMATERIALEXTPROC) (GLenum face, GLenum mode);

/* GL_EXT_pixel_transform */
typedef void (* PFNGLPIXELTRANSFORMPARAMETERIEXTPROC) (GLenum target, GLenum pname, GLint param);
typedef void (* PFNGLPIXELTRANSFORMPARAMETERFEXTPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (* PFNGLPIXELTRANSFORMPARAMETERIVEXTPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLPIXELTRANSFORMPARAMETERFVEXTPROC) (GLenum target, GLenum pname, GLfloat *params);

/* GL_EXT_secondary_color */
typedef void (* PFNGLSECONDARYCOLOR3BEXTPROC) (GLbyte red, GLbyte green, GLbyte blue);
typedef void (* PFNGLSECONDARYCOLOR3BVEXTPROC) (GLbyte *v);
typedef void (* PFNGLSECONDARYCOLOR3DEXTPROC) (GLdouble red, GLdouble green, GLdouble blue);
typedef void (* PFNGLSECONDARYCOLOR3DVEXTPROC) (GLdouble *v);
typedef void (* PFNGLSECONDARYCOLOR3FEXTPROC) (GLfloat red, GLfloat green, GLfloat blue);
typedef void (* PFNGLSECONDARYCOLOR3FVEXTPROC) (GLfloat *v);
typedef void (* PFNGLSECONDARYCOLOR3IEXTPROC) (GLint red, GLint green, GLint blue);
typedef void (* PFNGLSECONDARYCOLOR3IVEXTPROC) (GLint *v);
typedef void (* PFNGLSECONDARYCOLOR3SEXTPROC) (GLshort red, GLshort green, GLshort blue);
typedef void (* PFNGLSECONDARYCOLOR3SVEXTPROC) (GLshort *v);
typedef void (* PFNGLSECONDARYCOLOR3UBEXTPROC) (GLubyte red, GLubyte green, GLubyte blue);
typedef void (* PFNGLSECONDARYCOLOR3UBVEXTPROC) (GLubyte *v);
typedef void (* PFNGLSECONDARYCOLOR3UIEXTPROC) (GLuint red, GLuint green, GLuint blue);
typedef void (* PFNGLSECONDARYCOLOR3UIVEXTPROC) (GLuint *v);
typedef void (* PFNGLSECONDARYCOLOR3USEXTPROC) (GLushort red, GLushort green, GLushort blue);
typedef void (* PFNGLSECONDARYCOLOR3USVEXTPROC) (GLushort *v);
typedef void (* PFNGLSECONDARYCOLORPOINTEREXTPROC) (GLint size, GLenum type, GLsizei stride, GLvoid *pointer);

/* GL_EXT_texture_perturb_normal */
typedef void (* PFNGLTEXTURENORMALEXTPROC) (GLenum mode);

/* GL_EXT_multi_draw_arrays */
typedef void (* PFNGLMULTIDRAWARRAYSEXTPROC) (GLenum mode, GLint *first, GLsizei *count, GLsizei primcount);
typedef void (* PFNGLMULTIDRAWELEMENTSEXTPROC) (GLenum mode, GLsizei *count, GLenum type, GLvoid* *indices, GLsizei primcount);

/* GL_EXT_fog_coord */
typedef void (* PFNGLFOGCOORDFEXTPROC) (GLfloat coord);
typedef void (* PFNGLFOGCOORDFVEXTPROC) (GLfloat *coord);
typedef void (* PFNGLFOGCOORDDEXTPROC) (GLdouble coord);
typedef void (* PFNGLFOGCOORDDVEXTPROC) (GLdouble *coord);
typedef void (* PFNGLFOGCOORDPOINTEREXTPROC) (GLenum type, GLsizei stride, GLvoid *pointer);

/* GL_EXT_coordinate_frame */
typedef void (* PFNGLTANGENT3BEXTPROC) (GLbyte tx, GLbyte ty, GLbyte tz);
typedef void (* PFNGLTANGENT3BVEXTPROC) (GLbyte *v);
typedef void (* PFNGLTANGENT3DEXTPROC) (GLdouble tx, GLdouble ty, GLdouble tz);
typedef void (* PFNGLTANGENT3DVEXTPROC) (GLdouble *v);
typedef void (* PFNGLTANGENT3FEXTPROC) (GLfloat tx, GLfloat ty, GLfloat tz);
typedef void (* PFNGLTANGENT3FVEXTPROC) (GLfloat *v);
typedef void (* PFNGLTANGENT3IEXTPROC) (GLint tx, GLint ty, GLint tz);
typedef void (* PFNGLTANGENT3IVEXTPROC) (GLint *v);
typedef void (* PFNGLTANGENT3SEXTPROC) (GLshort tx, GLshort ty, GLshort tz);
typedef void (* PFNGLTANGENT3SVEXTPROC) (GLshort *v);
typedef void (* PFNGLBINORMAL3BEXTPROC) (GLbyte bx, GLbyte by, GLbyte bz);
typedef void (* PFNGLBINORMAL3BVEXTPROC) (GLbyte *v);
typedef void (* PFNGLBINORMAL3DEXTPROC) (GLdouble bx, GLdouble by, GLdouble bz);
typedef void (* PFNGLBINORMAL3DVEXTPROC) (GLdouble *v);
typedef void (* PFNGLBINORMAL3FEXTPROC) (GLfloat bx, GLfloat by, GLfloat bz);
typedef void (* PFNGLBINORMAL3FVEXTPROC) (GLfloat *v);
typedef void (* PFNGLBINORMAL3IEXTPROC) (GLint bx, GLint by, GLint bz);
typedef void (* PFNGLBINORMAL3IVEXTPROC) (GLint *v);
typedef void (* PFNGLBINORMAL3SEXTPROC) (GLshort bx, GLshort by, GLshort bz);
typedef void (* PFNGLBINORMAL3SVEXTPROC) (GLshort *v);
typedef void (* PFNGLTANGENTPOINTEREXTPROC) (GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (* PFNGLBINORMALPOINTEREXTPROC) (GLenum type, GLsizei stride, GLvoid *pointer);

/* GL_EXT_blend_func_separate */
typedef void (* PFNGLBLENDFUNCSEPARATEEXTPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void (* PFNGLBLENDFUNCSEPARATEINGRPROC) (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);

/* GL_EXT_vertex_weighting */
typedef void (* PFNGLVERTEXWEIGHTFEXTPROC) (GLfloat weight);
typedef void (* PFNGLVERTEXWEIGHTFVEXTPROC) (GLfloat *weight);
typedef void (* PFNGLVERTEXWEIGHTPOINTEREXTPROC) (GLsizei size, GLenum type, GLsizei stride, GLvoid *pointer);

/* GL_EXT_multisample */
typedef void (* PFNGLSAMPLEMASKEXTPROC) (GLclampf value, GLboolean invert);
typedef void (* PFNGLSAMPLEPATTERNEXTPROC) (GLenum pattern);

/* GL_EXT_vertex_shader */
typedef void (* PFNGLBEGINVERTEXSHADEREXTPROC) ();
typedef void (* PFNGLENDVERTEXSHADEREXTPROC) ();
typedef void (* PFNGLBINDVERTEXSHADEREXTPROC) (GLuint id);
typedef GLuint (* PFNGLGENVERTEXSHADERSEXTPROC) (GLuint range);
typedef void (* PFNGLDELETEVERTEXSHADEREXTPROC) (GLuint id);
typedef void (* PFNGLSHADEROP1EXTPROC) (GLenum op, GLuint res, GLuint arg1);
typedef void (* PFNGLSHADEROP2EXTPROC) (GLenum op, GLuint res, GLuint arg1, GLuint arg2);
typedef void (* PFNGLSHADEROP3EXTPROC) (GLenum op, GLuint res, GLuint arg1, GLuint arg2, GLuint arg3);
typedef void (* PFNGLSWIZZLEEXTPROC) (GLuint res, GLuint in_, GLenum outX, GLenum outY, GLenum outZ, GLenum outW);
typedef void (* PFNGLWRITEMASKEXTPROC) (GLuint res, GLuint in_, GLenum outX, GLenum outY, GLenum outZ, GLenum outW);
typedef void (* PFNGLINSERTCOMPONENTEXTPROC) (GLuint res, GLuint src, GLuint num);
typedef void (* PFNGLEXTRACTCOMPONENTEXTPROC) (GLuint res, GLuint src, GLuint num);
typedef GLuint (* PFNGLGENSYMBOLSEXTPROC) (GLenum datatype, GLenum storagetype, GLenum range, GLuint components);
typedef void (* PFNGLSETINVARIANTEXTPROC) (GLuint id, GLenum type, void *addr);
typedef void (* PFNGLSETLOCALCONSTANTEXTPROC) (GLuint id, GLenum type, void *addr);
typedef void (* PFNGLVARIANTBVEXTPROC) (GLuint id, GLbyte *addr);
typedef void (* PFNGLVARIANTSVEXTPROC) (GLuint id, GLshort *addr);
typedef void (* PFNGLVARIANTIVEXTPROC) (GLuint id, GLint *addr);
typedef void (* PFNGLVARIANTFVEXTPROC) (GLuint id, GLfloat *addr);
typedef void (* PFNGLVARIANTDVEXTPROC) (GLuint id, GLdouble *addr);
typedef void (* PFNGLVARIANTUBVEXTPROC) (GLuint id, GLubyte *addr);
typedef void (* PFNGLVARIANTUSVEXTPROC) (GLuint id, GLushort *addr);
typedef void (* PFNGLVARIANTUIVEXTPROC) (GLuint id, GLuint *addr);
typedef void (* PFNGLVARIANTPOINTEREXTPROC) (GLuint id, GLenum type, GLuint stride, void *addr);
typedef void (* PFNGLENABLEVARIANTCLIENTSTATEEXTPROC) (GLuint id);
typedef void (* PFNGLDISABLEVARIANTCLIENTSTATEEXTPROC) (GLuint id);
typedef GLuint (* PFNGLBINDLIGHTPARAMETEREXTPROC) (GLenum light, GLenum value);
typedef GLuint (* PFNGLBINDMATERIALPARAMETEREXTPROC) (GLenum face, GLenum value);
typedef GLuint (* PFNGLBINDTEXGENPARAMETEREXTPROC) (GLenum unit, GLenum coord, GLenum value);
typedef GLuint (* PFNGLBINDTEXTUREUNITPARAMETEREXTPROC) (GLenum unit, GLenum value);
typedef GLuint (* PFNGLBINDPARAMETEREXTPROC) (GLenum value);
typedef GLboolean (* PFNGLISVARIANTENABLEDEXTPROC) (GLuint id, GLenum cap);
typedef void (* PFNGLGETVARIANTBOOLEANVEXTPROC) (GLuint id, GLenum value, GLboolean *data);
typedef void (* PFNGLGETVARIANTINTEGERVEXTPROC) (GLuint id, GLenum value, GLint *data);
typedef void (* PFNGLGETVARIANTFLOATVEXTPROC) (GLuint id, GLenum value, GLfloat *data);
typedef void (* PFNGLGETVARIANTPOINTERVEXTPROC) (GLuint id, GLenum value, GLvoid* *data);
typedef void (* PFNGLGETINVARIANTBOOLEANVEXTPROC) (GLuint id, GLenum value, GLboolean *data);
typedef void (* PFNGLGETINVARIANTINTEGERVEXTPROC) (GLuint id, GLenum value, GLint *data);
typedef void (* PFNGLGETINVARIANTFLOATVEXTPROC) (GLuint id, GLenum value, GLfloat *data);
typedef void (* PFNGLGETLOCALCONSTANTBOOLEANVEXTPROC) (GLuint id, GLenum value, GLboolean *data);
typedef void (* PFNGLGETLOCALCONSTANTINTEGERVEXTPROC) (GLuint id, GLenum value, GLint *data);
typedef void (* PFNGLGETLOCALCONSTANTFLOATVEXTPROC) (GLuint id, GLenum value, GLfloat *data);

/* GL_EXT_stencil_two_side */
typedef void (* PFNGLACTIVESTENCILFACEEXTPROC) (GLenum face);

/* GL_SGI_color_table */
typedef void (* PFNGLCOLORTABLESGIPROC) (GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, GLvoid *table);
typedef void (* PFNGLCOLORTABLEPARAMETERFVSGIPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLCOLORTABLEPARAMETERIVSGIPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLCOPYCOLORTABLESGIPROC) (GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
typedef void (* PFNGLGETCOLORTABLESGIPROC) (GLenum target, GLenum format, GLenum type, GLvoid *table);
typedef void (* PFNGLGETCOLORTABLEPARAMETERFVSGIPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCOLORTABLEPARAMETERIVSGIPROC) (GLenum target, GLenum pname, GLint *params);

/* GL_SGIX_pixel_texture */
typedef void (* PFNGLPIXELTEXGENSGIXPROC) (GLenum mode);

/* GL_SGIS_pixel_texture */
typedef void (* PFNGLPIXELTEXGENPARAMETERISGISPROC) (GLenum pname, GLint param);
typedef void (* PFNGLPIXELTEXGENPARAMETERIVSGISPROC) (GLenum pname, GLint *params);
typedef void (* PFNGLPIXELTEXGENPARAMETERFSGISPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLPIXELTEXGENPARAMETERFVSGISPROC) (GLenum pname, GLfloat *params);
typedef void (* PFNGLGETPIXELTEXGENPARAMETERIVSGISPROC) (GLenum pname, GLint *params);
typedef void (* PFNGLGETPIXELTEXGENPARAMETERFVSGISPROC) (GLenum pname, GLfloat *params);

/* GL_SGIS_texture4D */
typedef void (* PFNGLTEXIMAGE4DSGISPROC) (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLsizei size4d, GLint border, GLenum format, GLenum type, GLvoid *pixels);
typedef void (* PFNGLTEXSUBIMAGE4DSGISPROC) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint woffset, GLsizei width, GLsizei height, GLsizei depth, GLsizei size4d, GLenum format, GLenum type, GLvoid *pixels);

/* GL_SGIS_detail_texture */
typedef void (* PFNGLDETAILTEXFUNCSGISPROC) (GLenum target, GLsizei n, GLfloat *points);
typedef void (* PFNGLGETDETAILTEXFUNCSGISPROC) (GLenum target, GLfloat *points);

/* GL_SGIS_sharpen_texture */
typedef void (* PFNGLSHARPENTEXFUNCSGISPROC) (GLenum target, GLsizei n, GLfloat *points);
typedef void (* PFNGLGETSHARPENTEXFUNCSGISPROC) (GLenum target, GLfloat *points);

/* GL_SGIS_multisample */
typedef void (* PFNGLSAMPLEMASKSGISPROC) (GLclampf value, GLboolean invert);
typedef void (* PFNGLSAMPLEPATTERNSGISPROC) (GLenum pattern);

/* GL_SGIX_sprite */
typedef void (* PFNGLSPRITEPARAMETERFSGIXPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLSPRITEPARAMETERFVSGIXPROC) (GLenum pname, GLfloat *params);
typedef void (* PFNGLSPRITEPARAMETERISGIXPROC) (GLenum pname, GLint param);
typedef void (* PFNGLSPRITEPARAMETERIVSGIXPROC) (GLenum pname, GLint *params);

/* GL_SGIS_point_parameters */
typedef void (* PFNGLPOINTPARAMETERFSGISPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLPOINTPARAMETERFVSGISPROC) (GLenum pname, GLfloat *params);

/* GL_SGIX_instruments */
typedef GLint (* PFNGLGETINSTRUMENTSSGIXPROC) ();
typedef void (* PFNGLINSTRUMENTSBUFFERSGIXPROC) (GLsizei size, GLint *buffer);
typedef GLint (* PFNGLPOLLINSTRUMENTSSGIXPROC) (GLint *marker_p);
typedef void (* PFNGLREADINSTRUMENTSSGIXPROC) (GLint marker);
typedef void (* PFNGLSTARTINSTRUMENTSSGIXPROC) ();
typedef void (* PFNGLSTOPINSTRUMENTSSGIXPROC) (GLint marker);

/* GL_SGIX_framezoom */
typedef void (* PFNGLFRAMEZOOMSGIXPROC) (GLint factor);

/* GL_SGIX_tag_sample_buffer */
typedef void (* PFNGLTAGSAMPLEBUFFERSGIXPROC) ();

/* GL_SGIX_polynomial_ffd */
typedef void (* PFNGLDEFORMATIONMAP3DSGIXPROC) (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, GLdouble w1, GLdouble w2, GLint wstride, GLint worder, GLdouble *points);
typedef void (* PFNGLDEFORMATIONMAP3FSGIXPROC) (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, GLfloat w1, GLfloat w2, GLint wstride, GLint worder, GLfloat *points);
typedef void (* PFNGLDEFORMSGIXPROC) (GLbitfield mask);
typedef void (* PFNGLLOADIDENTITYDEFORMATIONMAPSGIXPROC) (GLbitfield mask);

/* GL_SGIX_reference_plane */
typedef void (* PFNGLREFERENCEPLANESGIXPROC) (GLdouble *equation);

/* GL_SGIX_flush_raster */
typedef void (* PFNGLFLUSHRASTERSGIXPROC) ();

/* GL_SGIS_fog_function */
typedef void (* PFNGLFOGFUNCSGISPROC) (GLsizei n, GLfloat *points);
typedef void (* PFNGLGETFOGFUNCSGISPROC) (GLfloat *points);

/* GL_HP_image_transform */
typedef void (* PFNGLIMAGETRANSFORMPARAMETERIHPPROC) (GLenum target, GLenum pname, GLint param);
typedef void (* PFNGLIMAGETRANSFORMPARAMETERFHPPROC) (GLenum target, GLenum pname, GLfloat param);
typedef void (* PFNGLIMAGETRANSFORMPARAMETERIVHPPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLIMAGETRANSFORMPARAMETERFVHPPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETIMAGETRANSFORMPARAMETERIVHPPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETIMAGETRANSFORMPARAMETERFVHPPROC) (GLenum target, GLenum pname, GLfloat *params);

/* GL_PGI_misc_hints */
typedef void (* PFNGLHINTPGIPROC) (GLenum target, GLint mode);

/* GL_SGIX_list_priority */
typedef void (* PFNGLGETLISTPARAMETERFVSGIXPROC) (GLuint list, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETLISTPARAMETERIVSGIXPROC) (GLuint list, GLenum pname, GLint *params);
typedef void (* PFNGLLISTPARAMETERFSGIXPROC) (GLuint list, GLenum pname, GLfloat param);
typedef void (* PFNGLLISTPARAMETERFVSGIXPROC) (GLuint list, GLenum pname, GLfloat *params);
typedef void (* PFNGLLISTPARAMETERISGIXPROC) (GLuint list, GLenum pname, GLint param);
typedef void (* PFNGLLISTPARAMETERIVSGIXPROC) (GLuint list, GLenum pname, GLint *params);

/* GL_SGIX_fragment_lighting */
typedef void (* PFNGLFRAGMENTCOLORMATERIALSGIXPROC) (GLenum face, GLenum mode);
typedef void (* PFNGLFRAGMENTLIGHTFSGIXPROC) (GLenum light, GLenum pname, GLfloat param);
typedef void (* PFNGLFRAGMENTLIGHTFVSGIXPROC) (GLenum light, GLenum pname, GLfloat *params);
typedef void (* PFNGLFRAGMENTLIGHTISGIXPROC) (GLenum light, GLenum pname, GLint param);
typedef void (* PFNGLFRAGMENTLIGHTIVSGIXPROC) (GLenum light, GLenum pname, GLint *params);
typedef void (* PFNGLFRAGMENTLIGHTMODELFSGIXPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLFRAGMENTLIGHTMODELFVSGIXPROC) (GLenum pname, GLfloat *params);
typedef void (* PFNGLFRAGMENTLIGHTMODELISGIXPROC) (GLenum pname, GLint param);
typedef void (* PFNGLFRAGMENTLIGHTMODELIVSGIXPROC) (GLenum pname, GLint *params);
typedef void (* PFNGLFRAGMENTMATERIALFSGIXPROC) (GLenum face, GLenum pname, GLfloat param);
typedef void (* PFNGLFRAGMENTMATERIALFVSGIXPROC) (GLenum face, GLenum pname, GLfloat *params);
typedef void (* PFNGLFRAGMENTMATERIALISGIXPROC) (GLenum face, GLenum pname, GLint param);
typedef void (* PFNGLFRAGMENTMATERIALIVSGIXPROC) (GLenum face, GLenum pname, GLint *params);
typedef void (* PFNGLGETFRAGMENTLIGHTFVSGIXPROC) (GLenum light, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETFRAGMENTLIGHTIVSGIXPROC) (GLenum light, GLenum pname, GLint *params);
typedef void (* PFNGLGETFRAGMENTMATERIALFVSGIXPROC) (GLenum face, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETFRAGMENTMATERIALIVSGIXPROC) (GLenum face, GLenum pname, GLint *params);
typedef void (* PFNGLLIGHTENVISGIXPROC) (GLenum pname, GLint param);

/* GL_SGIX_async */
typedef void (* PFNGLASYNCMARKERSGIXPROC) (GLuint marker);
typedef GLint (* PFNGLFINISHASYNCSGIXPROC) (GLuint *markerp);
typedef GLint (* PFNGLPOLLASYNCSGIXPROC) (GLuint *markerp);
typedef GLuint (* PFNGLGENASYNCMARKERSSGIXPROC) (GLsizei range);
typedef void (* PFNGLDELETEASYNCMARKERSSGIXPROC) (GLuint marker, GLsizei range);
typedef GLboolean (* PFNGLISASYNCMARKERSGIXPROC) (GLuint marker);

/* GL_INTEL_parallel_arrays */
typedef void (* PFNGLVERTEXPOINTERVINTELPROC) (GLint size, GLenum type, GLvoid* *pointer);
typedef void (* PFNGLNORMALPOINTERVINTELPROC) (GLenum type, GLvoid* *pointer);
typedef void (* PFNGLCOLORPOINTERVINTELPROC) (GLint size, GLenum type, GLvoid* *pointer);
typedef void (* PFNGLTEXCOORDPOINTERVINTELPROC) (GLint size, GLenum type, GLvoid* *pointer);

/* GL_SUNX_constant_data */
typedef void (* PFNGLFINISHTEXTURESUNXPROC) ();

/* GL_SUN_global_alpha */
typedef void (* PFNGLGLOBALALPHAFACTORBSUNPROC) (GLbyte factor);
typedef void (* PFNGLGLOBALALPHAFACTORSSUNPROC) (GLshort factor);
typedef void (* PFNGLGLOBALALPHAFACTORISUNPROC) (GLint factor);
typedef void (* PFNGLGLOBALALPHAFACTORFSUNPROC) (GLfloat factor);
typedef void (* PFNGLGLOBALALPHAFACTORDSUNPROC) (GLdouble factor);
typedef void (* PFNGLGLOBALALPHAFACTORUBSUNPROC) (GLubyte factor);
typedef void (* PFNGLGLOBALALPHAFACTORUSSUNPROC) (GLushort factor);
typedef void (* PFNGLGLOBALALPHAFACTORUISUNPROC) (GLuint factor);

/* GL_SUN_triangle_list */
typedef void (* PFNGLREPLACEMENTCODEUISUNPROC) (GLuint code);
typedef void (* PFNGLREPLACEMENTCODEUSSUNPROC) (GLushort code);
typedef void (* PFNGLREPLACEMENTCODEUBSUNPROC) (GLubyte code);
typedef void (* PFNGLREPLACEMENTCODEUIVSUNPROC) (GLuint *code);
typedef void (* PFNGLREPLACEMENTCODEUSVSUNPROC) (GLushort *code);
typedef void (* PFNGLREPLACEMENTCODEUBVSUNPROC) (GLubyte *code);
typedef void (* PFNGLREPLACEMENTCODEPOINTERSUNPROC) (GLenum type, GLsizei stride, GLvoid* *pointer);

/* GL_SUN_vertex */
typedef void (* PFNGLCOLOR4UBVERTEX2FSUNPROC) (GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y);
typedef void (* PFNGLCOLOR4UBVERTEX2FVSUNPROC) (GLubyte *c, GLfloat *v);
typedef void (* PFNGLCOLOR4UBVERTEX3FSUNPROC) (GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLCOLOR4UBVERTEX3FVSUNPROC) (GLubyte *c, GLfloat *v);
typedef void (* PFNGLCOLOR3FVERTEX3FSUNPROC) (GLfloat r, GLfloat g, GLfloat b, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLCOLOR3FVERTEX3FVSUNPROC) (GLfloat *c, GLfloat *v);
typedef void (* PFNGLNORMAL3FVERTEX3FSUNPROC) (GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLNORMAL3FVERTEX3FVSUNPROC) (GLfloat *n, GLfloat *v);
typedef void (* PFNGLCOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLCOLOR4FNORMAL3FVERTEX3FVSUNPROC) (GLfloat *c, GLfloat *n, GLfloat *v);
typedef void (* PFNGLTEXCOORD2FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLTEXCOORD2FVERTEX3FVSUNPROC) (GLfloat *tc, GLfloat *v);
typedef void (* PFNGLTEXCOORD4FVERTEX4FSUNPROC) (GLfloat s, GLfloat t, GLfloat p, GLfloat q, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLTEXCOORD4FVERTEX4FVSUNPROC) (GLfloat *tc, GLfloat *v);
typedef void (* PFNGLTEXCOORD2FCOLOR4UBVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLTEXCOORD2FCOLOR4UBVERTEX3FVSUNPROC) (GLfloat *tc, GLubyte *c, GLfloat *v);
typedef void (* PFNGLTEXCOORD2FCOLOR3FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat r, GLfloat g, GLfloat b, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLTEXCOORD2FCOLOR3FVERTEX3FVSUNPROC) (GLfloat *tc, GLfloat *c, GLfloat *v);
typedef void (* PFNGLTEXCOORD2FNORMAL3FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLTEXCOORD2FNORMAL3FVERTEX3FVSUNPROC) (GLfloat *tc, GLfloat *n, GLfloat *v);
typedef void (* PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLfloat s, GLfloat t, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLTEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC) (GLfloat *tc, GLfloat *c, GLfloat *n, GLfloat *v);
typedef void (* PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FSUNPROC) (GLfloat s, GLfloat t, GLfloat p, GLfloat q, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLTEXCOORD4FCOLOR4FNORMAL3FVERTEX4FVSUNPROC) (GLfloat *tc, GLfloat *c, GLfloat *n, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUIVERTEX3FSUNPROC) (GLuint rc, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUIVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FSUNPROC) (GLuint rc, GLubyte r, GLubyte g, GLubyte b, GLubyte a, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUICOLOR4UBVERTEX3FVSUNPROC) (GLuint *rc, GLubyte *c, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FSUNPROC) (GLuint rc, GLfloat r, GLfloat g, GLfloat b, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUICOLOR3FVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *c, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUINORMAL3FVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *n, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUICOLOR4FNORMAL3FVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *c, GLfloat *n, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FSUNPROC) (GLuint rc, GLfloat s, GLfloat t, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUITEXCOORD2FVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *tc, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat s, GLfloat t, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUITEXCOORD2FNORMAL3FVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *tc, GLfloat *n, GLfloat *v);
typedef void (* PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FSUNPROC) (GLuint rc, GLfloat s, GLfloat t, GLfloat r, GLfloat g, GLfloat b, GLfloat a, GLfloat nx, GLfloat ny, GLfloat nz, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLREPLACEMENTCODEUITEXCOORD2FCOLOR4FNORMAL3FVERTEX3FVSUNPROC) (GLuint *rc, GLfloat *tc, GLfloat *c, GLfloat *n, GLfloat *v);

/* GL_NV_vertex_array_range */
typedef void (* PFNGLFLUSHVERTEXARRAYRANGENVPROC) ();
typedef void (* PFNGLVERTEXARRAYRANGENVPROC) (GLsizei length, GLvoid *pointer);

/* GL_NV_primitive_restart */
typedef void (* PFNGLPRIMITIVERESTARTNVPROC) ();
typedef void (* PFNGLPRIMITIVERESTARTINDEXNVPROC) (GLuint index);

/* GL_NV_register_combiners */
typedef void (* PFNGLCOMBINERPARAMETERFVNVPROC) (GLenum pname, GLfloat *params);
typedef void (* PFNGLCOMBINERPARAMETERFNVPROC) (GLenum pname, GLfloat param);
typedef void (* PFNGLCOMBINERPARAMETERIVNVPROC) (GLenum pname, GLint *params);
typedef void (* PFNGLCOMBINERPARAMETERINVPROC) (GLenum pname, GLint param);
typedef void (* PFNGLCOMBINERINPUTNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (* PFNGLCOMBINEROUTPUTNVPROC) (GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum);
typedef void (* PFNGLFINALCOMBINERINPUTNVPROC) (GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage);
typedef void (* PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC) (GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint *params);
typedef void (* PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC) (GLenum stage, GLenum portion, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC) (GLenum stage, GLenum portion, GLenum pname, GLint *params);
typedef void (* PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC) (GLenum variable, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC) (GLenum variable, GLenum pname, GLint *params);

/* GL_NV_fragment_program */
typedef void (* PFNGLPROGRAMNAMEDPARAMETER4FNVPROC) (GLuint id, GLsizei len, GLubyte *name, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLPROGRAMNAMEDPARAMETER4DNVPROC) (GLuint id, GLsizei len, GLubyte *name, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLPROGRAMNAMEDPARAMETER4FVNVPROC) (GLuint id, GLsizei len, GLubyte *name, GLfloat v[]);
typedef void (* PFNGLPROGRAMNAMEDPARAMETER4DVNVPROC) (GLuint id, GLsizei len, GLubyte *name, GLdouble v[]);
typedef void (* PFNGLGETPROGRAMNAMEDPARAMETERFVNVPROC) (GLuint id, GLsizei len, GLubyte *name, GLfloat *params);
typedef void (* PFNGLGETPROGRAMNAMEDPARAMETERDVNVPROC) (GLuint id, GLsizei len, GLubyte *name, GLdouble *params);

/* GL_NV_half_float */
typedef ushort GLhalf;
typedef void (* PFNGLVERTEX2HNVPROC) (GLhalf x, GLhalf y);
typedef void (* PFNGLVERTEX2HVNVPROC) (GLhalf *v);
typedef void (* PFNGLVERTEX3HNVPROC) (GLhalf x, GLhalf y, GLhalf z);
typedef void (* PFNGLVERTEX3HVNVPROC) (GLhalf *v);
typedef void (* PFNGLVERTEX4HNVPROC) (GLhalf x, GLhalf y, GLhalf z, GLhalf w);
typedef void (* PFNGLVERTEX4HVNVPROC) (GLhalf *v);
typedef void (* PFNGLNORMAL3HNVPROC) (GLhalf nx, GLhalf ny, GLhalf nz);
typedef void (* PFNGLNORMAL3HVNVPROC) (GLhalf *v);
typedef void (* PFNGLCOLOR3HNVPROC) (GLhalf red, GLhalf green, GLhalf blue);
typedef void (* PFNGLCOLOR3HVNVPROC) (GLhalf *v);
typedef void (* PFNGLCOLOR4HNVPROC) (GLhalf red, GLhalf green, GLhalf blue, GLhalf alpha);
typedef void (* PFNGLCOLOR4HVNVPROC) (GLhalf *v);
typedef void (* PFNGLTEXCOORD1HNVPROC) (GLhalf s);
typedef void (* PFNGLTEXCOORD1HVNVPROC) (GLhalf *v);
typedef void (* PFNGLTEXCOORD2HNVPROC) (GLhalf s, GLhalf t);
typedef void (* PFNGLTEXCOORD2HVNVPROC) (GLhalf *v);
typedef void (* PFNGLTEXCOORD3HNVPROC) (GLhalf s, GLhalf t, GLhalf r);
typedef void (* PFNGLTEXCOORD3HVNVPROC) (GLhalf *v);
typedef void (* PFNGLTEXCOORD4HNVPROC) (GLhalf s, GLhalf t, GLhalf r, GLhalf q);
typedef void (* PFNGLTEXCOORD4HVNVPROC) (GLhalf *v);
typedef void (* PFNGLMULTITEXCOORD1HNVPROC) (GLenum target, GLhalf s);
typedef void (* PFNGLMULTITEXCOORD1HVNVPROC) (GLenum target, GLhalf *v);
typedef void (* PFNGLMULTITEXCOORD2HNVPROC) (GLenum target, GLhalf s, GLhalf t);
typedef void (* PFNGLMULTITEXCOORD2HVNVPROC) (GLenum target, GLhalf *v);
typedef void (* PFNGLMULTITEXCOORD3HNVPROC) (GLenum target, GLhalf s, GLhalf t, GLhalf r);
typedef void (* PFNGLMULTITEXCOORD3HVNVPROC) (GLenum target, GLhalf *v);
typedef void (* PFNGLMULTITEXCOORD4HNVPROC) (GLenum target, GLhalf s, GLhalf t, GLhalf r, GLhalf q);
typedef void (* PFNGLMULTITEXCOORD4HVNVPROC) (GLenum target, GLhalf *v);
typedef void (* PFNGLFOGCOORDHNVPROC) (GLhalf fog);
typedef void (* PFNGLFOGCOORDHVNVPROC) (GLhalf *fog);
typedef void (* PFNGLSECONDARYCOLOR3HNVPROC) (GLhalf red, GLhalf green, GLhalf blue);
typedef void (* PFNGLSECONDARYCOLOR3HVNVPROC) (GLhalf *v);
typedef void (* PFNGLVERTEXWEIGHTHNVPROC) (GLhalf weight);
typedef void (* PFNGLVERTEXWEIGHTHVNVPROC) (GLhalf *weight);
typedef void (* PFNGLVERTEXATTRIB1HNVPROC) (GLuint index, GLhalf x);
typedef void (* PFNGLVERTEXATTRIB1HVNVPROC) (GLuint index, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIB2HNVPROC) (GLuint index, GLhalf x, GLhalf y);
typedef void (* PFNGLVERTEXATTRIB2HVNVPROC) (GLuint index, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIB3HNVPROC) (GLuint index, GLhalf x, GLhalf y, GLhalf z);
typedef void (* PFNGLVERTEXATTRIB3HVNVPROC) (GLuint index, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIB4HNVPROC) (GLuint index, GLhalf x, GLhalf y, GLhalf z, GLhalf w);
typedef void (* PFNGLVERTEXATTRIB4HVNVPROC) (GLuint index, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIBS1HVNVPROC) (GLuint index, GLsizei n, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIBS2HVNVPROC) (GLuint index, GLsizei n, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIBS3HVNVPROC) (GLuint index, GLsizei n, GLhalf *v);
typedef void (* PFNGLVERTEXATTRIBS4HVNVPROC) (GLuint index, GLsizei n, GLhalf *v);

/* GL_NV_stencil_two_side */
typedef void (* PFNGLACTIVESTENCILFACENVPROC) (GLenum face);

/* GL_MESA_resize_buffers */
typedef void (* PFNGLRESIZEBUFFERSMESAPROC) ();

/* GL_MESA_window_pos */
typedef void (* PFNGLWINDOWPOS2DMESAPROC) (GLdouble x, GLdouble y);
typedef void (* PFNGLWINDOWPOS2DVMESAPROC) (GLdouble *v);
typedef void (* PFNGLWINDOWPOS2FMESAPROC) (GLfloat x, GLfloat y);
typedef void (* PFNGLWINDOWPOS2FVMESAPROC) (GLfloat *v);
typedef void (* PFNGLWINDOWPOS2IMESAPROC) (GLint x, GLint y);
typedef void (* PFNGLWINDOWPOS2IVMESAPROC) (GLint *v);
typedef void (* PFNGLWINDOWPOS2SMESAPROC) (GLshort x, GLshort y);
typedef void (* PFNGLWINDOWPOS2SVMESAPROC) (GLshort *v);
typedef void (* PFNGLWINDOWPOS3DMESAPROC) (GLdouble x, GLdouble y, GLdouble z);
typedef void (* PFNGLWINDOWPOS3DVMESAPROC) (GLdouble *v);
typedef void (* PFNGLWINDOWPOS3FMESAPROC) (GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLWINDOWPOS3FVMESAPROC) (GLfloat *v);
typedef void (* PFNGLWINDOWPOS3IMESAPROC) (GLint x, GLint y, GLint z);
typedef void (* PFNGLWINDOWPOS3IVMESAPROC) (GLint *v);
typedef void (* PFNGLWINDOWPOS3SMESAPROC) (GLshort x, GLshort y, GLshort z);
typedef void (* PFNGLWINDOWPOS3SVMESAPROC) (GLshort *v);
typedef void (* PFNGLWINDOWPOS4DMESAPROC) (GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLWINDOWPOS4DVMESAPROC) (GLdouble *v);
typedef void (* PFNGLWINDOWPOS4FMESAPROC) (GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLWINDOWPOS4FVMESAPROC) (GLfloat *v);
typedef void (* PFNGLWINDOWPOS4IMESAPROC) (GLint x, GLint y, GLint z, GLint w);
typedef void (* PFNGLWINDOWPOS4IVMESAPROC) (GLint *v);
typedef void (* PFNGLWINDOWPOS4SMESAPROC) (GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (* PFNGLWINDOWPOS4SVMESAPROC) (GLshort *v);

/* GL_IBM_multimode_draw_arrays */
typedef void (* PFNGLMULTIMODEDRAWARRAYSIBMPROC) (GLenum mode, GLint *first, GLsizei *count, GLsizei primcount, GLint modestride);
typedef void (* PFNGLMULTIMODEDRAWELEMENTSIBMPROC) (GLenum *mode, GLsizei *count, GLenum type, GLvoid* *indices, GLsizei primcount, GLint modestride);

/* GL_IBM_vertex_array_lists */
typedef void (* PFNGLCOLORPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);
typedef void (* PFNGLSECONDARYCOLORPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);
typedef void (* PFNGLEDGEFLAGPOINTERLISTIBMPROC) (GLint stride, GLboolean* *pointer, GLint ptrstride);
typedef void (* PFNGLFOGCOORDPOINTERLISTIBMPROC) (GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);
typedef void (* PFNGLINDEXPOINTERLISTIBMPROC) (GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);
typedef void (* PFNGLNORMALPOINTERLISTIBMPROC) (GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);
typedef void (* PFNGLTEXCOORDPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);
typedef void (* PFNGLVERTEXPOINTERLISTIBMPROC) (GLint size, GLenum type, GLint stride, GLvoid* *pointer, GLint ptrstride);

/* GL_3DFX_tbuffer */
typedef void (* PFNGLTBUFFERMASK3DFXPROC) (GLuint mask);

/* GL_SGIS_texture_color_mask */
typedef void (* PFNGLTEXTURECOLORMASKSGISPROC) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);

/* GL_SGIX_igloo_interface */
typedef void (* PFNGLIGLOOINTERFACESGIXPROC) (GLenum pname, GLvoid *params);

/* GL_NV_fence */
typedef void (* PFNGLDELETEFENCESNVPROC) (GLsizei n, GLuint *fences);
typedef void (* PFNGLGENFENCESNVPROC) (GLsizei n, GLuint *fences);
typedef GLboolean (* PFNGLISFENCENVPROC) (GLuint fence);
typedef GLboolean (* PFNGLTESTFENCENVPROC) (GLuint fence);
typedef void (* PFNGLGETFENCEIVNVPROC) (GLuint fence, GLenum pname, GLint *params);
typedef void (* PFNGLFINISHFENCENVPROC) (GLuint fence);
typedef void (* PFNGLSETFENCENVPROC) (GLuint fence, GLenum condition);

/* GL_NV_element_array */
typedef void (* PFNGLELEMENTPOINTERNVPROC) (GLenum type, GLvoid *pointer);
typedef void (* PFNGLDRAWELEMENTARRAYNVPROC) (GLenum mode, GLint first, GLsizei count);
typedef void (* PFNGLDRAWRANGEELEMENTARRAYNVPROC) (GLenum mode, GLuint start, GLuint end, GLint first, GLsizei count);
typedef void (* PFNGLMULTIDRAWELEMENTARRAYNVPROC) (GLenum mode, GLint *first, GLsizei *count, GLsizei primcount);
typedef void (* PFNGLMULTIDRAWRANGEELEMENTARRAYNVPROC) (GLenum mode, GLuint start, GLuint end, GLint *first, GLsizei *count, GLsizei primcount);

/* GL_NV_evaluators */
typedef void (* PFNGLMAPCONTROLPOINTSNVPROC) (GLenum target, GLuint index, GLenum type, GLsizei ustride, GLsizei vstride, GLint uorder, GLint vorder, GLboolean packed, GLvoid *points);
typedef void (* PFNGLMAPPARAMETERIVNVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLMAPPARAMETERFVNVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETMAPCONTROLPOINTSNVPROC) (GLenum target, GLuint index, GLenum type, GLsizei ustride, GLsizei vstride, GLboolean packed, GLvoid *points);
typedef void (* PFNGLGETMAPPARAMETERIVNVPROC) (GLenum target, GLenum pname, GLint *params);
typedef void (* PFNGLGETMAPPARAMETERFVNVPROC) (GLenum target, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETMAPATTRIBPARAMETERIVNVPROC) (GLenum target, GLuint index, GLenum pname, GLint *params);
typedef void (* PFNGLGETMAPATTRIBPARAMETERFVNVPROC) (GLenum target, GLuint index, GLenum pname, GLfloat *params);
typedef void (* PFNGLEVALMAPSNVPROC) (GLenum target, GLenum mode);

/* GL_NV_register_combiners2 */
typedef void (* PFNGLCOMBINERSTAGEPARAMETERFVNVPROC) (GLenum stage, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETCOMBINERSTAGEPARAMETERFVNVPROC) (GLenum stage, GLenum pname, GLfloat *params);

/* GL_NV_vertex_program */
typedef GLboolean (* PFNGLAREPROGRAMSRESIDENTNVPROC) (GLsizei n, GLuint *programs, GLboolean *residences);
typedef void (* PFNGLBINDPROGRAMNVPROC) (GLenum target, GLuint id);
typedef void (* PFNGLDELETEPROGRAMSNVPROC) (GLsizei n, GLuint *programs);
typedef void (* PFNGLEXECUTEPROGRAMNVPROC) (GLenum target, GLuint id, GLfloat *params);
typedef void (* PFNGLGENPROGRAMSNVPROC) (GLsizei n, GLuint *programs);
typedef void (* PFNGLGETPROGRAMPARAMETERDVNVPROC) (GLenum target, GLuint index, GLenum pname, GLdouble *params);
typedef void (* PFNGLGETPROGRAMPARAMETERFVNVPROC) (GLenum target, GLuint index, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETPROGRAMIVNVPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (* PFNGLGETPROGRAMSTRINGNVPROC) (GLuint id, GLenum pname, GLubyte *program);
typedef void (* PFNGLGETTRACKMATRIXIVNVPROC) (GLenum target, GLuint address, GLenum pname, GLint *params);
typedef void (* PFNGLGETVERTEXATTRIBDVNVPROC) (GLuint index, GLenum pname, GLdouble *params);
typedef void (* PFNGLGETVERTEXATTRIBFVNVPROC) (GLuint index, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETVERTEXATTRIBIVNVPROC) (GLuint index, GLenum pname, GLint *params);
typedef void (* PFNGLGETVERTEXATTRIBPOINTERVNVPROC) (GLuint index, GLenum pname, GLvoid* *pointer);
typedef GLboolean (* PFNGLISPROGRAMNVPROC) (GLuint id);
typedef void (* PFNGLLOADPROGRAMNVPROC) (GLenum target, GLuint id, GLsizei len, GLubyte *program);
typedef void (* PFNGLPROGRAMPARAMETER4DNVPROC) (GLenum target, GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLPROGRAMPARAMETER4DVNVPROC) (GLenum target, GLuint index, GLdouble *v);
typedef void (* PFNGLPROGRAMPARAMETER4FNVPROC) (GLenum target, GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLPROGRAMPARAMETER4FVNVPROC) (GLenum target, GLuint index, GLfloat *v);
typedef void (* PFNGLPROGRAMPARAMETERS4DVNVPROC) (GLenum target, GLuint index, GLuint count, GLdouble *v);
typedef void (* PFNGLPROGRAMPARAMETERS4FVNVPROC) (GLenum target, GLuint index, GLuint count, GLfloat *v);
typedef void (* PFNGLREQUESTRESIDENTPROGRAMSNVPROC) (GLsizei n, GLuint *programs);
typedef void (* PFNGLTRACKMATRIXNVPROC) (GLenum target, GLuint address, GLenum matrix, GLenum transform);
typedef void (* PFNGLVERTEXATTRIBPOINTERNVPROC) (GLuint index, GLint fsize, GLenum type, GLsizei stride, GLvoid *pointer);
typedef void (* PFNGLVERTEXATTRIB1DNVPROC) (GLuint index, GLdouble x);
typedef void (* PFNGLVERTEXATTRIB1DVNVPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB1FNVPROC) (GLuint index, GLfloat x);
typedef void (* PFNGLVERTEXATTRIB1FVNVPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB1SNVPROC) (GLuint index, GLshort x);
typedef void (* PFNGLVERTEXATTRIB1SVNVPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB2DNVPROC) (GLuint index, GLdouble x, GLdouble y);
typedef void (* PFNGLVERTEXATTRIB2DVNVPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB2FNVPROC) (GLuint index, GLfloat x, GLfloat y);
typedef void (* PFNGLVERTEXATTRIB2FVNVPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB2SNVPROC) (GLuint index, GLshort x, GLshort y);
typedef void (* PFNGLVERTEXATTRIB2SVNVPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB3DNVPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z);
typedef void (* PFNGLVERTEXATTRIB3DVNVPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB3FNVPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLVERTEXATTRIB3FVNVPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB3SNVPROC) (GLuint index, GLshort x, GLshort y, GLshort z);
typedef void (* PFNGLVERTEXATTRIB3SVNVPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB4DNVPROC) (GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLVERTEXATTRIB4DVNVPROC) (GLuint index, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIB4FNVPROC) (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLVERTEXATTRIB4FVNVPROC) (GLuint index, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIB4SNVPROC) (GLuint index, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (* PFNGLVERTEXATTRIB4SVNVPROC) (GLuint index, GLshort *v);
typedef void (* PFNGLVERTEXATTRIB4UBNVPROC) (GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w);
typedef void (* PFNGLVERTEXATTRIB4UBVNVPROC) (GLuint index, GLubyte *v);
typedef void (* PFNGLVERTEXATTRIBS1DVNVPROC) (GLuint index, GLsizei count, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIBS1FVNVPROC) (GLuint index, GLsizei count, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIBS1SVNVPROC) (GLuint index, GLsizei count, GLshort *v);
typedef void (* PFNGLVERTEXATTRIBS2DVNVPROC) (GLuint index, GLsizei count, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIBS2FVNVPROC) (GLuint index, GLsizei count, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIBS2SVNVPROC) (GLuint index, GLsizei count, GLshort *v);
typedef void (* PFNGLVERTEXATTRIBS3DVNVPROC) (GLuint index, GLsizei count, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIBS3FVNVPROC) (GLuint index, GLsizei count, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIBS3SVNVPROC) (GLuint index, GLsizei count, GLshort *v);
typedef void (* PFNGLVERTEXATTRIBS4DVNVPROC) (GLuint index, GLsizei count, GLdouble *v);
typedef void (* PFNGLVERTEXATTRIBS4FVNVPROC) (GLuint index, GLsizei count, GLfloat *v);
typedef void (* PFNGLVERTEXATTRIBS4SVNVPROC) (GLuint index, GLsizei count, GLshort *v);
typedef void (* PFNGLVERTEXATTRIBS4UBVNVPROC) (GLuint index, GLsizei count, GLubyte *v);

/* GL_ATI_envmap_bumpmap */
typedef void (* PFNGLTEXBUMPPARAMETERIVATIPROC) (GLenum pname, GLint *param);
typedef void (* PFNGLTEXBUMPPARAMETERFVATIPROC) (GLenum pname, GLfloat *param);
typedef void (* PFNGLGETTEXBUMPPARAMETERIVATIPROC) (GLenum pname, GLint *param);
typedef void (* PFNGLGETTEXBUMPPARAMETERFVATIPROC) (GLenum pname, GLfloat *param);

/* GL_ATI_fragment_shader */
typedef GLuint (* PFNGLGENFRAGMENTSHADERSATIPROC) (GLuint range);
typedef void (* PFNGLBINDFRAGMENTSHADERATIPROC) (GLuint id);
typedef void (* PFNGLDELETEFRAGMENTSHADERATIPROC) (GLuint id);
typedef void (* PFNGLBEGINFRAGMENTSHADERATIPROC) ();
typedef void (* PFNGLENDFRAGMENTSHADERATIPROC) ();
typedef void (* PFNGLPASSTEXCOORDATIPROC) (GLuint dst, GLuint coord, GLenum swizzle);
typedef void (* PFNGLSAMPLEMAPATIPROC) (GLuint dst, GLuint interp, GLenum swizzle);
typedef void (* PFNGLCOLORFRAGMENTOP1ATIPROC) (GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef void (* PFNGLCOLORFRAGMENTOP2ATIPROC) (GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef void (* PFNGLCOLORFRAGMENTOP3ATIPROC) (GLenum op, GLuint dst, GLuint dstMask, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef void (* PFNGLALPHAFRAGMENTOP1ATIPROC) (GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod);
typedef void (* PFNGLALPHAFRAGMENTOP2ATIPROC) (GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod);
typedef void (* PFNGLALPHAFRAGMENTOP3ATIPROC) (GLenum op, GLuint dst, GLuint dstMod, GLuint arg1, GLuint arg1Rep, GLuint arg1Mod, GLuint arg2, GLuint arg2Rep, GLuint arg2Mod, GLuint arg3, GLuint arg3Rep, GLuint arg3Mod);
typedef void (* PFNGLSETFRAGMENTSHADERCONSTANTATIPROC) (GLuint dst, GLfloat *value);

/* GL_ATI_pn_triangles */
typedef void (* PFNGLPNTRIANGLESIATIPROC) (GLenum pname, GLint param);
typedef void (* PFNGLPNTRIANGLESFATIPROC) (GLenum pname, GLfloat param);

/* GL_ATI_vertex_array_object */
typedef GLuint (* PFNGLNEWOBJECTBUFFERATIPROC) (GLsizei size, GLvoid *pointer, GLenum usage);
typedef GLboolean (* PFNGLISOBJECTBUFFERATIPROC) (GLuint buffer);
typedef void (* PFNGLUPDATEOBJECTBUFFERATIPROC) (GLuint buffer, GLuint offset, GLsizei size, GLvoid *pointer, GLenum preserve);
typedef void (* PFNGLGETOBJECTBUFFERFVATIPROC) (GLuint buffer, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETOBJECTBUFFERIVATIPROC) (GLuint buffer, GLenum pname, GLint *params);
typedef void (* PFNGLDELETEOBJECTBUFFERATIPROC) (GLuint buffer);
typedef void (* PFNGLARRAYOBJECTATIPROC) (GLenum array, GLint size, GLenum type, GLsizei stride, GLuint buffer, GLuint offset);
typedef void (* PFNGLGETARRAYOBJECTFVATIPROC) (GLenum array, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETARRAYOBJECTIVATIPROC) (GLenum array, GLenum pname, GLint *params);
typedef void (* PFNGLVARIANTARRAYOBJECTATIPROC) (GLuint id, GLenum type, GLsizei stride, GLuint buffer, GLuint offset);
typedef void (* PFNGLGETVARIANTARRAYOBJECTFVATIPROC) (GLuint id, GLenum pname, GLfloat *params);
typedef void (* PFNGLGETVARIANTARRAYOBJECTIVATIPROC) (GLuint id, GLenum pname, GLint *params);

/* GL_ATI_vertex_streams */
typedef void (* PFNGLVERTEXSTREAM1SATIPROC) (GLenum stream, GLshort x);
typedef void (* PFNGLVERTEXSTREAM1SVATIPROC) (GLenum stream, GLshort *coords);
typedef void (* PFNGLVERTEXSTREAM1IATIPROC) (GLenum stream, GLint x);
typedef void (* PFNGLVERTEXSTREAM1IVATIPROC) (GLenum stream, GLint *coords);
typedef void (* PFNGLVERTEXSTREAM1FATIPROC) (GLenum stream, GLfloat x);
typedef void (* PFNGLVERTEXSTREAM1FVATIPROC) (GLenum stream, GLfloat *coords);
typedef void (* PFNGLVERTEXSTREAM1DATIPROC) (GLenum stream, GLdouble x);
typedef void (* PFNGLVERTEXSTREAM1DVATIPROC) (GLenum stream, GLdouble *coords);
typedef void (* PFNGLVERTEXSTREAM2SATIPROC) (GLenum stream, GLshort x, GLshort y);
typedef void (* PFNGLVERTEXSTREAM2SVATIPROC) (GLenum stream, GLshort *coords);
typedef void (* PFNGLVERTEXSTREAM2IATIPROC) (GLenum stream, GLint x, GLint y);
typedef void (* PFNGLVERTEXSTREAM2IVATIPROC) (GLenum stream, GLint *coords);
typedef void (* PFNGLVERTEXSTREAM2FATIPROC) (GLenum stream, GLfloat x, GLfloat y);
typedef void (* PFNGLVERTEXSTREAM2FVATIPROC) (GLenum stream, GLfloat *coords);
typedef void (* PFNGLVERTEXSTREAM2DATIPROC) (GLenum stream, GLdouble x, GLdouble y);
typedef void (* PFNGLVERTEXSTREAM2DVATIPROC) (GLenum stream, GLdouble *coords);
typedef void (* PFNGLVERTEXSTREAM3SATIPROC) (GLenum stream, GLshort x, GLshort y, GLshort z);
typedef void (* PFNGLVERTEXSTREAM3SVATIPROC) (GLenum stream, GLshort *coords);
typedef void (* PFNGLVERTEXSTREAM3IATIPROC) (GLenum stream, GLint x, GLint y, GLint z);
typedef void (* PFNGLVERTEXSTREAM3IVATIPROC) (GLenum stream, GLint *coords);
typedef void (* PFNGLVERTEXSTREAM3FATIPROC) (GLenum stream, GLfloat x, GLfloat y, GLfloat z);
typedef void (* PFNGLVERTEXSTREAM3FVATIPROC) (GLenum stream, GLfloat *coords);
typedef void (* PFNGLVERTEXSTREAM3DATIPROC) (GLenum stream, GLdouble x, GLdouble y, GLdouble z);
typedef void (* PFNGLVERTEXSTREAM3DVATIPROC) (GLenum stream, GLdouble *coords);
typedef void (* PFNGLVERTEXSTREAM4SATIPROC) (GLenum stream, GLshort x, GLshort y, GLshort z, GLshort w);
typedef void (* PFNGLVERTEXSTREAM4SVATIPROC) (GLenum stream, GLshort *coords);
typedef void (* PFNGLVERTEXSTREAM4IATIPROC) (GLenum stream, GLint x, GLint y, GLint z, GLint w);
typedef void (* PFNGLVERTEXSTREAM4IVATIPROC) (GLenum stream, GLint *coords);
typedef void (* PFNGLVERTEXSTREAM4FATIPROC) (GLenum stream, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
typedef void (* PFNGLVERTEXSTREAM4FVATIPROC) (GLenum stream, GLfloat *coords);
typedef void (* PFNGLVERTEXSTREAM4DATIPROC) (GLenum stream, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
typedef void (* PFNGLVERTEXSTREAM4DVATIPROC) (GLenum stream, GLdouble *coords);
typedef void (* PFNGLNORMALSTREAM3BATIPROC) (GLenum stream, GLbyte nx, GLbyte ny, GLbyte nz);
typedef void (* PFNGLNORMALSTREAM3BVATIPROC) (GLenum stream, GLbyte *coords);
typedef void (* PFNGLNORMALSTREAM3SATIPROC) (GLenum stream, GLshort nx, GLshort ny, GLshort nz);
typedef void (* PFNGLNORMALSTREAM3SVATIPROC) (GLenum stream, GLshort *coords);
typedef void (* PFNGLNORMALSTREAM3IATIPROC) (GLenum stream, GLint nx, GLint ny, GLint nz);
typedef void (* PFNGLNORMALSTREAM3IVATIPROC) (GLenum stream, GLint *coords);
typedef void (* PFNGLNORMALSTREAM3FATIPROC) (GLenum stream, GLfloat nx, GLfloat ny, GLfloat nz);
typedef void (* PFNGLNORMALSTREAM3FVATIPROC) (GLenum stream, GLfloat *coords);
typedef void (* PFNGLNORMALSTREAM3DATIPROC) (GLenum stream, GLdouble nx, GLdouble ny, GLdouble nz);
typedef void (* PFNGLNORMALSTREAM3DVATIPROC) (GLenum stream, GLdouble *coords);
typedef void (* PFNGLCLIENTACTIVEVERTEXSTREAMATIPROC) (GLenum stream);
typedef void (* PFNGLVERTEXBLENDENVIATIPROC) (GLenum pname, GLint param);
typedef void (* PFNGLVERTEXBLENDENVFATIPROC) (GLenum pname, GLfloat param);

/* GL_ATI_element_array */
typedef void (* PFNGLELEMENTPOINTERATIPROC) (GLenum type, GLvoid *pointer);
typedef void (* PFNGLDRAWELEMENTARRAYATIPROC) (GLenum mode, GLsizei count);
typedef void (* PFNGLDRAWRANGEELEMENTARRAYATIPROC) (GLenum mode, GLuint start, GLuint end, GLsizei count);

/* GL_SUN_mesh_array */
typedef void (* PFNGLDRAWMESHARRAYSSUNPROC) (GLenum mode, GLint first, GLsizei count, GLsizei width);

/* GL_NV_occlusion_query */
typedef void (* PFNGLGENOCCLUSIONQUERIESNVPROC) (GLsizei n, GLuint *ids);
typedef void (* PFNGLDELETEOCCLUSIONQUERIESNVPROC) (GLsizei n, GLuint *ids);
typedef GLboolean (* PFNGLISOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (* PFNGLBEGINOCCLUSIONQUERYNVPROC) (GLuint id);
typedef void (* PFNGLENDOCCLUSIONQUERYNVPROC) ();
typedef void (* PFNGLGETOCCLUSIONQUERYIVNVPROC) (GLuint id, GLenum pname, GLint *params);
typedef void (* PFNGLGETOCCLUSIONQUERYUIVNVPROC) (GLuint id, GLenum pname, GLuint *params);

/* GL_NV_pixel_data_range */
typedef void (* PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, GLvoid *pointer);
typedef void (* PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);

/* GL_NV_point_sprite */
typedef void (* PFNGLPOINTPARAMETERINVPROC) (GLenum pname, GLint param);
typedef void (* PFNGLPOINTPARAMETERIVNVPROC) (GLenum pname, GLint *params);
