// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

#pragma once

#include <atlaspacker/atlaspacker.h>

#pragma pack(1)

typedef struct
{
    int     no_rotate;
    int     tile_size;          // The size in texels. Default 16
    int     padding;            // Internal padding for each image. Default 1
    int     alpha_threshold;    // values below or equal to this threshold are considered transparent. (range 0-255)
} apTilePackerOptions;

#pragma options align=reset

void      apTilePackerSetDefaultOptions(apTilePackerOptions* options);
apPacker* apTilePackerCreate(apTilePackerOptions* options);
void      apTilePackerDestroy(apPacker* packer);
int       apTilePackerGetTileSize(apPacker* packer);

// The list of vertices is a triangle list.
// Each vertex is in range [-0.5, 0.5]
void      apTilePackerCreateTileImageFromTriangles(apPacker* packer, apImage* image, apPosf* vertices, int num_vertices);

// Takes ownership of the memory!
void      apTilePackerSetTileImage(apPacker* _packer, apImage* _image, int twidth, int theight, uint8_t* timage);

uint8_t*  apTilePackerCreateTileImageFromImage(int tile_size, int width, int height, int channels, uint8_t* src_image, int* twidth, int* theight);

// Private (unit testing)

// Creates a grayscale image of same dimensions as the image (width*height*1)
uint8_t*  apTilePackerDebugCreateImageFromTileImage(apImage* image, int index, int tile_size);


/*

# Images

    * Each image should be pre-inflated to include any desired padding

# Bin Pack

    * Use the original rect
    * Use the "tight" rect

# Brute Force
    * Divide atlas into tiles of size NxN texels

    * Convert each added image into tiles of size NxN

    * Testing each piece

        * Sort the images using some heuristic
            - E.g. Squareness, Area, ...

        * Brute force search through the atlas for places to place the images
            - Rotate the piece
            - Possibly flip the piece (MVP2)

        * Use bitmask (32/64/128 bit) to test if segment fits in row

            - Atlas:    11100000 -> 0xE0
            - Piece:      111100 -> 0x3C
                -> won't fit (A & P != 0)

            - Atlas:    11100000 -> 0xE0
            - Piece:       11110 -> 0x1E
                -> will fit (A & P == 0)

        * Find index of first free bit
            - Atlas:    11100000 -> 5. Which means we should shift the piece 3 bits to the right

            A = ~ 11100000 = 00011111
            piece_shift = numbits(A) - MSB(A) = 8 - 5

        * Heuristic to determine the space left
            - Separate islands -> lower score
            - Rectangular -> higher score
            -- etc

            - Idea:
                * Divide each row into RLE segments
                * Base the score on combining RLE scores
                    Example 1:
                    A:
                        10000111    -> (1,1), (4,0), (3,1)
                        11100001    -> (3,1), (4,0), (1,1)
                    B:
                        10000111    -> (1,1), (4,0), (3,1)
                        10000111    -> (1,1), (4,0), (3,1)

                    Here B should score higher.
                    E.g. multiplying any overlapping segments, in this case a 2x2 vs a 4x4 grid of 0's.

                    IDEA: We could use this to figure out which rotation to

                    Example 2:
                    A:
                        10000111
                        11111111

                    B:
                        11111111
                        10000111

                    (we're assuming we're filling up from top left)
                    Here B should get a higher score




# Geometry

    * Output coord where in atlas space where the image's rectangle will be blitted

    * Output the vertices making up the polygon

        - Bin packer - Output the rectangle
        - Packer - Brute Force - Walk the tiles of each image. Merge vertices that are on the edge. Supports concave polygons.


    * Geometry should be passed into the atlas packer
        - Keeping the code separate will simplify things

    * Render an image of the geometry.
        - We'll use this to generate the tile image as we want to make sure
        that the geometry won't intersect other areas of the final atlas image

        - After renderint the geometry image, we'll use a dilate(texels) step, to make account for custom padding

*/
