// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

#include <atlaspacker/atlaspacker.h>
#include <atlaspacker/binpacker.h>

#include <assert.h>
#include <math.h> // sqrtf
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // printf

#if defined(_WIN32)
    #include <Windows.h> // QPC
#else
    #include <sys/time.h>
#endif

void apSetDefaultOptions(apOptions* options)
{
    memset(options, 0, sizeof(apOptions));
    options->page_size = 0; // can grow dynamically
}

apContext* apCreate(apOptions* options, apPacker* packer)
{
    assert(options != 0);
    assert(packer != 0);

    apContext* ctx = (apContext*)malloc(sizeof(apContext));
    memset(ctx, 0, sizeof(*ctx));
    ctx->options = *options;
    ctx->packer = packer;

    return ctx;
}

void apDestroy(apContext* ctx)
{
    for (int i = 0; i < ctx->num_images; ++i)
    {
        free((void*)ctx->images[i]->vertices);
        ctx->packer->destroyImage(ctx->packer, ctx->images[i]);
    }
    free((void*)ctx->images);
    free((void*)ctx);
}

apImage* apAddImage(apContext* ctx, const char* path, int width, int height, int channels, const uint8_t* data)
{
    if (channels > ctx->num_channels)
        ctx->num_channels = channels;
    apImage* image = ctx->packer->createImage(ctx->packer, path, width, height, channels, data);
    image->path = path;
    image->dimensions.width = width;
    image->dimensions.height = height;
    image->data = data;
    image->width = width;
    image->height = height;
    image->channels = channels;
    memset(&image->placement, 0, sizeof(image->placement));

    ctx->num_images++;
    ctx->images = (apImage**)realloc(ctx->images, sizeof(apImage**)*ctx->num_images);
    ctx->images[ctx->num_images-1] = image;

    return image;
}

void apPackImages(apContext* ctx)
{
    ctx->packer->packImages(ctx->packer, ctx);
}

int apGetNumPages(apContext* ctx)
{
    return ctx->num_pages;
}

apPage* apGetPage(apContext* ctx, int index)
{
    apPage* page = ctx->pages;
    while (page)
    {
        if (page->index == index)
            return page;
        page = page->next;
    }
    return 0;
}

apPage* apAllocPage(apContext* ctx)
{
    apPage* page = (apPage*)malloc(sizeof(apPage));
    memset(page, 0, sizeof(apPage));
    page->index = ctx->num_pages++;

    if (ctx->pages == 0)
        ctx->pages = page;
    else
    {
        apPage* prev = ctx->pages;
        while (prev && prev->next)
            prev = prev->next;
        prev->next = page;
    }
    return page;
}

void apPageAddImage(apPage* page, apImage* image)
{
    image->page = page->index;

    if (!page->last_image)
    {
        page->first_image = (apImage*)image;
        page->last_image = (apImage*)image;
    }
    else
    {
        page->last_image->next = (apImage*)image;
        page->last_image = (apImage*)image;
    }
}

apImage* apPageGetFirstImage(apPage* page)
{
    return page->first_image;
}

apPos apRotate(int x, int y, int width, int height, int rotation)
{
    apPos pos;
    if (rotation == 90)
    {
        pos.x = y;
        pos.y = width - 1 - x;
    }
    else if (rotation == 180)
    {
        pos.x = width - 1 - x;
        pos.y = height - 1 - y;
    }
    else if (rotation == 270)
    {
        pos.x = height - 1 - y;
        pos.y = x;
    }
    else {
        pos.x = x;
        pos.y = y;
    }
    return pos;
}

// Copy the source image into the target image
// Can handle cases where the target texel is outside of the destination
// Transparent source texels are ignored
void apCopyRGBA(uint8_t* dest, int dest_width, int dest_height, int dest_channels,
                        const uint8_t* source, int source_width, int source_height, int source_channels,
                        int dest_x, int dest_y, int rotation)
{
    for (int sy = 0; sy < source_height; ++sy)
    {
        for (int sx = 0; sx < source_width; ++sx, source += source_channels)
        {
            // Skip copying texels with alpha=0
            if (source_channels == 4 && source[3] == 0)
                continue;

            // Map the current coord into the rotated space
            apPos rotated_pos = apRotate(sx, sy, source_width, source_height, rotation);

            int target_x = dest_x + rotated_pos.x;
            int target_y = dest_y + rotated_pos.y;

            // If the target is outside of the destination area, we skip it
            if (target_x < 0 || target_x >= dest_width ||
                target_y < 0 || target_y >= dest_height)
                continue;

            int dest_index = target_y * dest_width * dest_channels + target_x * dest_channels;

            uint8_t color[4] = {255,255,255,255};
            for (int c = 0; c < source_channels; ++c)
                color[c] = source[c];

            int alphathreshold = 8;
            if (alphathreshold >= 0 && color[3] <= alphathreshold)
                continue; // Skip texels that are <= the alpha threshold

            for (int c = 0; c < dest_channels; ++c)
                dest[dest_index+c] = color[c];

            if (color[3] > 0 && color[3] < 255)
            {
                uint32_t r = dest[dest_index+0] + 48;
                dest[dest_index+0] = (uint8_t)(r > 255 ? 255 : r);
                dest[dest_index+1] = dest[dest_index+1] / 2;
                dest[dest_index+2] = dest[dest_index+2] / 2;

                uint32_t a = dest[dest_index+3] + 128;
                dest[dest_index+3] = (uint8_t)(a > 255 ? 255 : a);
            }
        }
    }
}

// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
uint32_t apNextPowerOfTwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static int apTexelNonZeroKernel(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int x, int y, int kernelsize)
{
    int start_x = x < kernelsize ? 0 : x - kernelsize;
    int start_y = y < kernelsize ? 0 : y - kernelsize;
    int end_x = (x + kernelsize) > (width-1) ? width-1 : x + kernelsize;
    int end_y = (y + kernelsize) > (height-1) ? height-1 : y + kernelsize;

    for (int yy = start_y; yy <= end_y; ++yy)
    {
        for (int xx = start_x; xx <= end_x; ++xx)
        {
            uint8_t color[] = { 255, 255, 255, 255 };

            int index = yy*width*num_channels + xx*num_channels;
            uint8_t bits = 0;
            for (int c = 0; c < num_channels; ++c)
            {
                color[c] = image[index+c];
                bits |= color[c];
            }

            if (num_channels == 4 && color[3] == 0)
                continue; // completely transparent

            if (bits)
            {
                return 1; // non zero
            }
        }
    }
    return 0;
}

static int apTexelAlphaNonZero(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int x, int y)
{
    int index = y*width*num_channels + x*num_channels;
    return image[index+3] > 0 ? 1 : 0;
}

apPosf apMathNormalize(apPosf a)
{
    float length = sqrtf(a.x*a.x + a.y*a.y);
    apPosf v = { a.x / length, a.y / length};
    return v;
}

float apMathDot(apPosf a, apPosf b)
{
    return a.x * b.x + a.y * b.y;
}

apPosf apMathSub(apPosf a, apPosf b)
{
    apPosf v = { a.x - b.x, a.y - b.y };
    return v;
}
apPosf apMathAdd(apPosf a, apPosf b)
{
    apPosf v = { a.x + b.x, a.y + b.y };
    return v;
}
apPosf apMathScale(apPosf a, float s)
{
    apPosf v = { a.x * s, a.y * s};
    return v;
}
apPosf apMathMul(apPosf a, apPosf b)
{
    apPosf v = { a.x * b.x, a.y * b.y };
    return v;
}
float apMathMax(float a, float b)
{
    return a > b ? a : b;
}
float apMathMin(float a, float b)
{
    return a < b ? a : b;
}
int apMathRoundUp(int x, int multiple)
{
    return ((x + multiple - 1) / multiple) * multiple;
}

static int apOverlapAxisTest2D(apPosf axis, const apPosf* a, int sizea, const apPosf* b, int sizeb)
{
    float mina = 100000.0f;
    float maxa = -100000.0f;
    float minb = 100000.0f;
    float maxb = -100000.0f;

    for (int j = 0; j < sizea; ++j)
    {
        float d = apMathDot(axis, a[j]);
        maxa = apMathMax(maxa, d);
        mina = apMathMin(mina, d);
    }

    for (int j = 0; j < sizeb; ++j)
    {
        float d = apMathDot(axis, b[j]);
        maxb = apMathMax(maxb, d);
        minb = apMathMin(minb, d);
    }

    if (maxa < minb || maxb < mina)
        return 0;
    return 1;
}

// Vertices are in CCW order
int apOverlapTest2D(const apPosf* a, int sizea, const apPosf* b, int sizeb)
{
    for (int i = 0; i < sizea; ++i)
    {
        apPosf diff = apMathNormalize(apMathSub(a[(i+1)%sizea], a[i]));
        apPosf n = { -diff.y, diff.x };
        if (!apOverlapAxisTest2D(n, a, sizea, b, sizeb))
            return 0;
    }
    for (int i = 0; i < sizeb; ++i)
    {
        apPosf diff = apMathNormalize(apMathSub(b[(i+1)%sizeb], b[i]));
        apPosf n = { -diff.y, diff.x };
        if (!apOverlapAxisTest2D(n, a, sizea, b, sizeb))
            return 0;
    }
    return 1;
}

// Creates a black and white image (optinally dilated), for easier convex hull creation
uint8_t* apCreateHullImage(const uint8_t* image, uint32_t width, uint32_t height, uint32_t num_channels, int dilate)
{
    uint8_t* out = (uint8_t*)malloc(width*height);
    memset(out, 0, width*height);

    if (dilate)
    {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int value = apTexelNonZeroKernel(image, width, height, num_channels, x, y, dilate);
                out[y*width + x] = value*255;
            }
        }
    }
    else {
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                int value = apTexelAlphaNonZero(image, width, height, num_channels, x, y);
                out[y*width + x] = value*255;
            }
        }
    }
    return out;
}

uint64_t apGetTime()
{
#if defined(_WIN32)
    LARGE_INTEGER tickPerSecond;
    LARGE_INTEGER tick;
    QueryPerformanceFrequency(&tickPerSecond);
    QueryPerformanceCounter(&tick);
    return (uint64_t)(tick.QuadPart / (tickPerSecond.QuadPart / 1000000));
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)(tv.tv_sec) * 1000000U + (uint64_t)(tv.tv_usec);
#endif
}