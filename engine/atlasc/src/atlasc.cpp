#include "atlasc.h"

#include <stdio.h>
#include <stdlib.h> // qsort, malloc

extern "C" {
    #include <atlaspacker/atlaspacker.h>
    #include <atlaspacker/binpacker.h>
    #include <atlaspacker/convexhull.h>
    #include <atlaspacker/tilepacker.h>
}

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb/stb_image_write.h>
namespace dmAtlasc
{

SourceImage::SourceImage()
{
    memset(this, 0, sizeof(*this));
}

SourceImage::~SourceImage()
{
    delete[] m_Data;
    free((void*)m_Path);
}

PackedImage::PackedImage()
: m_Rotation(0)
, m_Width(0)
, m_Height(0)
, m_NumChannels(0)
, m_Path(0)
, m_Data(0)
, m_DataCount(0)
{
}

PackedImage::~PackedImage()
{
    delete[] m_Data;
    free((void*)m_Path);
}

AtlasPage::AtlasPage()
: m_NumChannels(0)
, m_Index(0)
{
}

AtlasPage::~AtlasPage()
{
    for (uint32_t i = 0; i < m_Images.Size(); ++i)
    {
        delete m_Images[i];
    }
    m_Images.SetSize(0);
}

Atlas::Atlas()
{
}

Atlas::~Atlas()
{
    for (uint32_t i = 0; i < m_Pages.Size(); ++i)
    {
        delete m_Pages[i];
    }
    m_Pages.SetSize(0);
}

RenderedPage::RenderedPage()
{
    memset(this, 0, sizeof(*this));
}
RenderedPage::~RenderedPage()
{
    delete[] m_Data;
}

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

static int CompareImages(const SourceImage* a, const SourceImage* b)
{
    int a_w = a->m_Size.width;
    int a_h = a->m_Size.height;
    int b_w = b->m_Size.width;
    int b_h = b->m_Size.height;
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
    apTilePackerOptions packer_options;
    apTilePackerSetDefaultOptions(&packer_options);
    packer_options.no_rotate        = options.m_PackerNoRotate;
    packer_options.tile_size        = options.m_TilePackerTileSize;
    packer_options.padding          = options.m_TilePackerPadding;
    packer_options.alpha_threshold  = options.m_TIlePackerAlphaThreshold;

    return apTilePackerCreate(&packer_options);
}

static apPacker* CreateBinPacker(const Options& options)
{
    apBinPackerOptions packer_options;
    apBinPackerSetDefaultOptions(&packer_options);
    packer_options.no_rotate = options.m_PackerNoRotate;
    return apBinPackerCreate(&packer_options);
}

static int CreateHullImage(apPacker* packer, SourceImage* image, apImage* apimage)
{
    int num_planes = 8;

    uint8_t* hull_image = 0;

    int num_vertices = 0;
    apPosf* vertices = 0;

    int num_triangles = 0;
    apPosf* triangles = 0;

// TODO: Move this code into the tilepacker itself

    int dilate = 0;
    hull_image = apCreateHullImage(image->m_Data, (uint32_t)image->m_Size.width, (uint32_t)image->m_Size.height, (uint32_t)image->m_NumChannels, dilate);

    vertices = apConvexHullFromImage(num_planes, hull_image, image->m_Size.width, image->m_Size.height, &num_vertices);
    if (!vertices)
    {
        printf("Failed to generate hull for %s\n", image->m_Path);
        return 0;
    }

    // Triangulate a convex hull
    num_triangles = num_vertices - 2;
    triangles = (apPosf*)malloc(sizeof(apPosf) * (size_t)num_triangles * 3);
    for (int t = 0; t < num_triangles; ++t)
    {
        triangles[t*3+0] = vertices[0];
        triangles[t*3+1] = vertices[1+t+0];
        triangles[t*3+2] = vertices[1+t+1];
    }

    apimage->vertices = triangles;
    apimage->num_vertices = num_triangles*3;

    apTilePackerCreateTileImageFromTriangles(packer, apimage, triangles, num_triangles*3);

    free((void*)vertices);
    free((void*)hull_image);

    return 1;
}

static inline Vec2i APPos(const apPos& v)
{
    Vec2i out = {v.x, v.y};
    return out;
}

static inline Vec2f APPosf(const apPosf& v)
{
    Vec2f out = {v.x, v.y};
    return out;
}

static inline Sizei APSize(const apSize& sz)
{
    Sizei out = {sz.width, sz.height};
    return out;
}

static inline Rect APRect(const apRect& rect)
{
    Rect out;
    out.m_Pos = APPos(rect.pos);
    out.m_Size = APSize(rect.size);
    return out;
}

static void PrintError(void*, Result result, const char* msg)
{
    printf("Error: %d %s\n", result, msg);
}

static PackedImage* APToAtlasImage(apImage* ap_image)
{
    PackedImage* image = new PackedImage;
    image->m_Placement = APRect(ap_image->placement);
    image->m_Rotation = ap_image->rotation;
    image->m_Width = ap_image->width;
    image->m_Height = ap_image->height;
    image->m_NumChannels = ap_image->channels;
    image->m_Path = strdup(ap_image->path);

    image->m_Data = ap_image->data;
    image->m_DataCount = image->m_Width * image->m_Height * image->m_NumChannels;

    int num_vertices = ap_image->num_vertices;
    image->m_Vertices.SetCapacity(num_vertices);
    image->m_Vertices.SetSize(num_vertices);
    for (int i = 0; i < num_vertices; ++i)
    {
        image->m_Vertices[i] = APPosf(ap_image->vertices[i]);
    }
    return image;
}

static AtlasPage* APToAtlasPage(int index, apPage* ap_page, int num_channels)
{
    AtlasPage* page = new AtlasPage;
    page->m_Index = index;
    page->m_Dimensions = APSize(ap_page->dimensions);
    page->m_NumChannels = num_channels;
    page->m_Images.SetCapacity(32);
    apImage* ap_image = apPageGetFirstImage(ap_page);
    while (ap_image)
    {
        PackedImage* image = APToAtlasImage(ap_image);

        if (page->m_Images.Full())
            page->m_Images.OffsetCapacity(32);
        page->m_Images.Push(image);

        ap_image = ap_image->next;
    }
    return page;
}

static Atlas* APToAtlas(apContext* ctx)
{
    Atlas* atlas = new Atlas;

    int num_pages = apGetNumPages(ctx);
    atlas->m_Pages.SetCapacity(num_pages);
    for (int i = 0; i < num_pages; ++i)
    {
        atlas->m_Pages.Push(APToAtlasPage(i, apGetPage(ctx, i), ctx->num_channels));
    }

    return atlas;
}

Atlas* CreateAtlas(const Options& atlas_options, SourceImage* source_images, uint32_t num_source_images, FOnError error_cbk, void* error_cbk_ctx)
{
    if (!error_cbk)
    {
        error_cbk = PrintError;
        error_cbk_ctx = 0;
    }

    SortImages(source_images, num_source_images);

    apPacker* packer = 0;
    if (atlas_options.m_Algorithm == PA_BINPACK_SKYLINE_BL)
        packer = CreateBinPacker(atlas_options);
    else
        packer = CreateTilePacker(atlas_options);

    if (packer == 0)
    {
        error_cbk(error_cbk_ctx, RESULT_INVAL, "Failed to create atlas packer.");
        return 0;
    }

    apOptions options;
    options.page_size = atlas_options.m_PageSize;

    apContext* ctx = apCreate(&options, packer);
    int result = 1;

    for (uint32_t i = 0; i < num_source_images; ++i)
    {
        SourceImage* image = &source_images[i];
        apImage* apimage = apAddImage(ctx, image->m_Path, image->m_Size.width, image->m_Size.height, image->m_NumChannels, image->m_Data);
        if (!apimage)
        {
            result = 0;
            break;
        }

        if (atlas_options.m_Algorithm == dmAtlasc::PA_TILEPACK_CONVEXHULL)
        {
            int result = CreateHullImage(packer, image, apimage);
            if (!result)
            {
                result = 0;
                break;
            }
        }

    }

    Atlas* atlas = 0;

    if (result)
    {
        apPackImages(ctx);

        // Create the atlas info
        atlas = APToAtlas(ctx);
    }

    apDestroy(ctx);
    return atlas;
}

void DestroyAtlas(Atlas* atlas)
{

}

void RenderPage(const AtlasPage* ap_page, RenderedPage* output)
{
    output->m_Dimensions = ap_page->m_Dimensions;
    output->m_NumChannels = ap_page->m_NumChannels;
    output->m_DataCount = ap_page->m_Dimensions.width * ap_page->m_Dimensions.height * ap_page->m_NumChannels;

    output->m_Data = (uint8_t*)malloc(output->m_DataCount);

// DEBUG BACKGROUND RENDERING
    // int tile_size = 16;
    // for (int y = 0; y < height; ++y)
    // {
    //     for (int x = 0; x < width; ++x)
    //     {
    //         int tx = x / tile_size;
    //         int ty = y / tile_size;
    //         int odd = ((tx&1) && !(ty&1)) | (!(tx&1) && (ty&1));

    //         // uint8_t color_odd[4] = {255,255,255,128};
    //         // uint8_t color_even[4] = {0,0,0,128};
    //         uint8_t color_odd[4] = {32,32,32,255};
    //         uint8_t color_even[4] = {16,16,16,255};
    //         // uint8_t color_odd[4] = {64,96,64,255};
    //         // uint8_t color_even[4] = {32,64,32,255};

    //         uint8_t* color = color_even;

    //         if (odd)
    //             color = color_odd;

    //         for (int i = 0; i < channels; ++i)
    //             output[y * (width*channels) + (x*channels) + i ] = color[i];
    //     }
    // }

    int width = output->m_Dimensions.width;
    int height = output->m_Dimensions.height;
    int num_channels = output->m_NumChannels;

    dmArray<PackedImage*> m_Images;
    for (uint32_t i = 0; i < ap_page->m_Images.Size(); ++i)
    {
        PackedImage* image = ap_page->m_Images[i];

        apCopyRGBA(output->m_Data, width, height, num_channels,
                   image->m_Data, image->m_Width, image->m_Height, image->m_NumChannels,
                   image->m_Placement.m_Pos.x, image->m_Placement.m_Pos.y, image->m_Rotation);
    }
}

int SavePage(const char* path, const AtlasPage* page)
{
    RenderedPage rendered;
    RenderPage(page, &rendered);

    int result = stbi_write_tga(path, rendered.m_Dimensions.width, rendered.m_Dimensions.height, rendered.m_NumChannels, rendered.m_Data);
    return result;
}


static void DebugPrintIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
        printf("    ");
}

static void DebugPrintPackedImage(PackedImage* packed_image, int indent)
{
    DebugPrintIndent(indent);
    printf("image: %d, %d, %d, %d\n", packed_image->m_Placement.m_Pos.x, packed_image->m_Placement.m_Pos.y, packed_image->m_Placement.m_Size.width, packed_image->m_Placement.m_Size.height);
}

static void DebugPrintPage(AtlasPage* page, int indent)
{
    DebugPrintIndent(indent);
    printf("Page %d\n", page->m_Index);

    for (uint32_t i = 0; i < page->m_Images.Size(); ++i)
    {
        DebugPrintPackedImage(page->m_Images[i], indent+1);
    }
}

void DebugPrintAtlas(Atlas* atlas)
{
    for (uint32_t i = 0; i < atlas->m_Pages.Size(); ++i)
    {
        DebugPrintPage(atlas->m_Pages[i], 0);
    }
}

} // namespace
