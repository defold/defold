// https://github.com/JCash/atlaspacker
// License: MIT
// @2021-@2023 Mathias Westerdahl

#pragma once

#include <stdint.h>
#include <atlaspacker/atlaspacker.h>

// May return 0 if it couldn't calculate a hull (i.e. if the image is empty)
apPosf* apConvexHullFromImage(int num_planes, uint8_t* image, int width, int height, int* num_vertices);

typedef void (*APHullBoxCallback)(void* ctx, int x, int y, int width, int height);

// Takes a bitmap where 0 is empty, and 1 is occupied
// Temporarily modifies the bitmap (clears bits to 0 to designate the traversal being done)
void apHullFindLargestBoxes(int width, int height, uint8_t* data, APHullBoxCallback cbk, void* cbk_ctx);

// May return 0 if it couldn't calculate a hull (i.e. if the image is empty)
apPosf* apHullFromImage(uint8_t* image, int width, int height, int* num_vertices);

apPosf* apCreateBoxVertices(apPos pos, apSize size, int* num_vertices);
