#include "atlasc.h"

#include <stdio.h>

extern "C" {
    #include <atlaspacker/atlaspacker.h>
    #include <atlaspacker/binpacker.h>
    #include <atlaspacker/convexhull.h>
    #include <atlaspacker/tilepacker.h>
}
namespace dmAtlasc
{

static char g_ErrorString[1024] = {};

Options::Options()
{
    memset(this, 0, sizeof(*this));

    // Let's use the good defaults from the atlaspacker
    {
        apTilePackerOptions packer_options;
        apTilePackerSetDefaultOptions(&packer_options);
        m_PackerNoRotate = packer_options.no_rotate;
        m_TilePackerTileSize = packer_options.tile_size;
        m_TilePackerPadding = packer_options.padding;
        m_TIlePackerAlphaThreshold = packer_options.alpha_threshold;
    }
    {
        // apBinPackerOptions packer_options;
        // apBinPackerSetDefaultOptions(&packer_options);
        // Currently no unique settings
    }
}

static int CompareImages(const SourceImage** _a, const SourceImage** _b)
{
    const SourceImage* a = *_a;
    const SourceImage* b = *_b;
    int a_w = a->m_Size.x;
    int a_h = a->m_Size.y;
    int b_w = b->m_Size.x;
    int b_h = b->m_Size.y;
    int area_a = a_w * a_h;
    int area_b = b_w * b_h;

    int max_a = a_w > a_h ? a_w : a_h;
    int min_a = a_w < a_h ? a_w : a_h;
    int max_b = b_w > b_h ? b_w : b_h;
    int min_b = b_w < b_h ? b_w : b_h;

    float square_a = ((float)max_a / (float)min_a) * (float)area_a;
    float square_b = ((float)max_b / (float)min_b) * (float)area_b;
    if (square_a == square_b)
    {
        if (a->m_Path && b->m_Path)
            return strcmp(a->m_Path, b->m_Path);
        return 0;
    }
    return (square_a <= square_b) ? 1 : -1;
}

typedef int (*QsortFn)(const void*, const void*);
static void SortImages(SourceImage* images, uint32_t num_images)
{
    qsort(images, num_images, sizeof(SourceImage), (QsortFn)CompareImages);
}

static apPacker* CreateTilePacker(const Options& options)
{
    // typedef struct
    // {
    //     int     no_rotate;
    //     int     tile_size;          // The size in texels. Default 16
    //     int     padding;            // Internal padding for each image. Default 1
    //     int     alpha_threshold;    // values below or equal to this threshold are considered transparent. (range 0-255)
    // } apTilePackerOptions;

    apTilePackerOptions packer_options;
    apTilePackerSetDefaultOptions(&packer_options);
    packer_options.no_rotate        = options.m_PackerNoRotate;
    packer_options.tile_size        = options.m_TilePackerTileSize;
    packer_options.padding          = options.m_TilePackerPadding;
    packer_options.alpha_threshold  = options.m_TIlePackerAlphaThreshold;

    printf("options.no_rotate: %d\n", packer_options.no_rotate);
    printf("options.tile_size: %d\n", packer_options.tile_size);
    printf("options.padding: %d\n", packer_options.padding);
    printf("options.alpha_threshold: %d\n", packer_options.alpha_threshold);

    return apTilePackerCreate(&packer_options);
}

static apPacker* CreateBinPacker(const Options& options)
{
    apBinPackerOptions packer_options;
    apBinPackerSetDefaultOptions(&packer_options);
    packer_options.no_rotate = options.m_PackerNoRotate;
    return apBinPackerCreate(&packer_options);
}

Atlas* CreateAtlas(const Options& atlas_options, SourceImage* source_images, uint32_t num_source_images)
{
    SortImages(source_images, num_source_images);

    apPacker* packer = 0;
    if (atlas_options.m_Algorithm == PA_BINPACK_SKYLINE_BL)
        packer = CreateBinPacker(atlas_options);
    else
        packer = CreateTilePacker(atlas_options);

    if (packer == 0)
    {

        return 0;
    }

    apOptions options;
    options.page_size = atlas_options.m_PageSize;



    apContext* ctx = apCreate(&options, packer);

    for (uint32_t i = 0; i < num_source_images; ++i)
    {
        SourceImage* image = &source_images[i];

        printf("Adding image: %s, %d x %d  \t\tarea: %d\n", image->m_Path, image->m_Size.x, image->m_Size.y, image->m_Size.x * image->m_Size.y);
        apImage* apimage = apAddImage(ctx, image->m_Path, image->m_Size.x, image->m_Size.y, image->m_NumChannels, image->m_Data);

    }


    apPackImages(ctx);

    // Create the atlas info

    apDestroy(ctx);
    return 0;
}

void DestroyAtlas(Atlas* atlas)
{

}

const char* GetLastError()
{
    return g_ErrorString;
}

} // namespace
