// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_SLUGC_H
#define DM_SLUGC_H

#include <stdint.h>
#include <dlib/shared_library.h>

// C bridge for Bob/editor compilation. Geometry, paints and numeric buffers
// are independent of SVG parsing and font decoding.
#ifdef __cplusplus
extern "C"
{
#endif
    typedef enum SlugResult
    {
        SLUG_RESULT_OK = 0,
        SLUG_RESULT_INVALID_ARGUMENT = -1,
        SLUG_RESULT_PATH_ERROR = -2,
        SLUG_RESULT_OUT_OF_MEMORY = -3,
        SLUG_RESULT_CAPACITY = -4,
    } SlugResult;

    // Owned numeric texture bytes, released with SlugFreeTexture. PixelCount
    // is the complete byte length, including padding to whole texture rows.
    typedef struct SlugTexture
    {
        uint8_t* m_Pixels;
        uint32_t m_PixelCount;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Channels;
    } SlugTexture;

    /* One filled path. Commands are seven floats each: operation (move=0,
     * line=1, quadratic=2, cubic=3, close=4), followed by up to three xy pairs.
     * Coordinates use the image viewport. Stops are offset,r,g,b,a in sRGB.
     * PaintTransform maps viewport coordinates to gradient coordinates, in
     * SVG matrix order (a,b,c,d,e,f). Paint types: solid=0, linear=1, radial=2;
     * spread modes: pad=0, repeat=1, reflect=2. Parameters are x1,y1,x2,y2 for
     * linear paint, and fx,fy,0,cx,cy,r for radial paint. All input is borrowed. */
    typedef struct SlugPath
    {
        const float* m_Commands;
        uint32_t     m_CommandCount;
        uint32_t     m_EvenOdd;
        uint32_t     m_PaintType;
        uint32_t     m_Spread;
        float        m_Parameters[6];
        float        m_PaintTransform[6];
        const float* m_Stops;
        uint32_t     m_StopCount;
    } SlugPath;

    /* Paths are in back-to-front paint order, with a top-left viewport origin. */
    typedef struct SlugImage
    {
        float           m_Width;
        float           m_Height;
        const SlugPath* m_Paths;
        uint32_t        m_PathCount;
    } SlugImage;

    /*# compile filled vector images into the shared Slug atlas format
     * Does not decode fonts or SVG. Outputs owned RGBA16F curves, R32UI bands
     * and RGBA32F paints, released with SlugFreeTexture. image_paints receives
     * one paint-header index per image. Failure leaves all textures empty.
     * @name SlugCompileAtlas
     * @param images [type: const SlugImage*] parsed image geometry
     * @param count [type: uint32_t] image count
     * @param curves [type: SlugTexture*] output curve texture
     * @param bands [type: SlugTexture*] output band texture
     * @param paints [type: SlugTexture*] output paint texture
     * @param image_paints [type: uint32_t*] count output paint indices
     * @return result [type: SlugResult] CAPACITY when numeric buffers cannot fit
     */
    DM_DLLEXPORT SlugResult SlugCompileAtlas(const SlugImage* images, uint32_t count, SlugTexture* curves, SlugTexture* bands, SlugTexture* paints, uint32_t* image_paints);

    // Releases and clears an owned texture. An empty texture is accepted.
    DM_DLLEXPORT void SlugFreeTexture(SlugTexture* texture);
#ifdef __cplusplus
}
#endif
#endif
