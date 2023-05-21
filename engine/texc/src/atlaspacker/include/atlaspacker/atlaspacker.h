// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

/////////////////////////////////////////////////////////
#pragma once

#include <stdint.h>

#pragma pack(1)

typedef struct {
    int x, y;
} apPos;

typedef struct apPosf
{
    float x, y;
} apPosf;

typedef struct {
    int width, height;
} apSize;

typedef struct {
    apPos   pos;    // bottom left
    apSize  size;
} apRect;

typedef struct apImage {
    struct apImage* next;
    const char*     path;
    const uint8_t*  data;
    apSize          dimensions;  // Original dimensions
    apRect          placement;   // The placement in the atlas
    int             width;
    int             height;
    int             channels;
    int             rotation;    // Degrees CCW: 0, 90, 180, 270
    int             page;

    apPosf*         vertices;
    int             num_vertices;
} apImage;

typedef struct apPage {
    struct apPage*  next;
    struct apImage* first_image;
    struct apImage* last_image;
    apSize          dimensions;
    int             index;
} apPage;

typedef struct
{
    int page_size;
} apOptions;

typedef struct {
    apOptions           options;

    apImage**           images;
    int                 num_images;
    apPage*             pages;
    int                 num_pages;
    int                 num_channels; // Max of the number of channels of each image
    struct _apPacker*   packer;
} apContext;

typedef struct _apPacker
{
    const char* packer_type;
    apImage* (*createImage)(struct _apPacker* packer, const char* path, int width, int height, int channels, const uint8_t* data);
    void (*destroyImage)(struct _apPacker* packer, apImage* image);
    void (*packImages)(struct _apPacker* packer, apContext* ctx);
} apPacker;

#pragma options align=reset

/////////////////////////////////////////////////////////
void        apSetDefaultOptions(apOptions* options);
apContext*  apCreate(apOptions* options, apPacker* packer);
void        apDestroy(apContext* ctx);
apImage*    apAddImage(apContext* ctx, const char* path, int width, int height, int channels, const uint8_t* data);
void        apPackImages(apContext* ctx);

// Get number of pages
int         apGetNumPages(apContext* ctx);
apPage*     apGetPage(apContext* ctx, int page_index);
apImage*    apPageGetFirstImage(apPage* page);


// Render functions

void        apCopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                        const uint8_t* source, int source_width, int source_height, int source_channels,
                        int dest_x, int dest_y, int rotation);

// Creates an image where all the rgba -> 0 or 1.
// It also dilates the image if necessary.
// This image is used when creating hulls around the image
// Image dimensions are width*height*1
// Caller owns the returned memory
uint8_t*    apCreateHullImage(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int dilate);


/////////////////////////////////////////////////////////
// Internal

apPage*     apAllocPage(apContext* ctx);
// links the image with a certain page
void        apPageAddImage(apPage* page, apImage* image);

uint64_t    apGetTime(); // for profiling

// Math functions
uint32_t    apNextPowerOfTwo(uint32_t v);

// Rotates a coord (x,y) in fixed rotations of [0,90,180,270] degrees
apPos       apRotate(int x, int y, int width, int height, int rotation);

apPosf      apMathNormalize(apPosf v);
apPosf      apMathSub(apPosf a, apPosf b);
apPosf      apMathAdd(apPosf a, apPosf b);
apPosf      apMathMul(apPosf a, apPosf b);
float       apMathDot(apPosf a, apPosf b);
apPosf      apMathScale(apPosf a, float s);
float       apMathMax(float a, float b);
float       apMathMin(float a, float b);
int         apMathRoundUp(int x, int multiple);

// Collision (takes two arrays of vertices, each forming a polygon)
// Vertices are in CCW order
int         apOverlapTest2D(const apPosf* a, int sizea, const apPosf* b, int sizeb);
