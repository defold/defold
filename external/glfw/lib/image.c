//========================================================================
// GLFW - An OpenGL framework
// Platform:    Any
// API version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

//========================================================================
// Description:
//
// This module acts as an interface for different image file formats (the
// image file format is detected automatically).
//
// By default the loaded image is rescaled (using bilinear interpolation)
// to the next higher 2^N x 2^M resolution, unless it has a valid
// 2^N x 2^M resolution. The interpolation is quite slow, even if the
// routine has been optimized for speed (a 200x200 RGB image is scaled to
// 256x256 in ~30 ms on a P3-500).
//
// Paletted images are converted to RGB/RGBA images.
//
// A convenience function is also included (glfwLoadTexture2D), which
// loads a texture image from a file directly to OpenGL texture memory,
// with an option to generate all mipmap levels. GL_SGIS_generate_mipmap
// is used whenever available, which should give an optimal mipmap
// generation speed (possibly performed in hardware). A software fallback
// method is included when GL_SGIS_generate_mipmap is not supported (it
// generates all mipmaps of a 256x256 RGB texture in ~3 ms on a P3-500).
//
//========================================================================


#include "internal.h"


// We want to support automatic mipmap generation
#ifndef GL_SGIS_generate_mipmap
 #define GL_GENERATE_MIPMAP_SGIS       0x8191
 #define GL_GENERATE_MIPMAP_HINT_SGIS  0x8192
 #define GL_SGIS_generate_mipmap    1
#endif // GL_SGIS_generate_mipmap


//************************************************************************
//****                  GLFW internal functions                       ****
//************************************************************************

//========================================================================
// Upsample image, from size w1 x h1 to w2 x h2
//========================================================================

static void UpsampleImage( unsigned char *src, unsigned char *dst,
    int w1, int h1, int w2, int h2, int bpp )
{
    int m, n, k, x, y, col8;
    float dx, dy, xstep, ystep, col, col1, col2;
    unsigned char *src1, *src2, *src3, *src4;

    // Calculate scaling factor
    xstep = (float)(w1-1) / (float)(w2-1);
    ystep = (float)(h1-1) / (float)(h2-1);

    // Copy source data to destination data with bilinear interpolation
    // Note: The rather strange look of this routine is a direct result of
    // my attempts at optimizing it. Improvements are welcome!
    dy = 0.0f;
    y = 0;
    for( n = 0; n < h2; n ++ )
    {
        dx = 0.0f;
        src1 = &src[ y*w1*bpp ];
        src3 = y < (h1-1) ? src1 + w1*bpp : src1;
        src2 = src1 + bpp;
        src4 = src3 + bpp;
        x = 0;
        for( m = 0; m < w2; m ++ )
        {
            for( k = 0; k < bpp; k ++ )
            {
                col1 = *src1 ++;
                col2 = *src2 ++;
                col = col1 + (col2 - col1) * dx;
                col1 = *src3 ++;
                col2 = *src4 ++;
                col2 = col1 + (col2 - col1) * dx;
                col += (col2 - col) * dy;
                col8 = (int) (col + 0.5);
                if( col8 >= 256 ) col8 = 255;
                *dst++ = (unsigned char) col8;
            }
            dx += xstep;
            if( dx >= 1.0f )
            {
                x ++;
                dx -= 1.0f;
                if( x >= (w1-1) )
                {
                    src2 = src1;
                    src4 = src3;
                }
            }
            else
            {
                src1 -= bpp;
                src2 -= bpp;
                src3 -= bpp;
                src4 -= bpp;
            }
        }
        dy += ystep;
        if( dy >= 1.0f )
        {
            y ++;
            dy -= 1.0f;
        }
    }
}


//========================================================================
// Build the next mip-map level
//========================================================================

static int HalveImage( GLubyte *src, int *width, int *height,
    int components )
{
    int     halfwidth, halfheight, m, n, k, idx1, idx2;
    GLubyte *dst;

    // Last level?
    if( *width <= 1 && *height <= 1 )
    {
        return GL_FALSE;
    }

    // Calculate new width and height (handle 1D case)
    halfwidth  = *width > 1 ? *width / 2 : 1;
    halfheight = *height > 1 ? *height / 2 : 1;

    // Downsample image with a simple box-filter
    dst = src;
    if( *width == 1 || *height == 1 )
    {
        // 1D case
        for( m = 0; m < halfwidth+halfheight-1; m ++ )
        {
            for( k = 0; k < components; k ++ )
            {
                *dst ++ = (GLubyte) (((int)*src +
                                      (int)src[components] + 1) >> 1);
                src ++;
            }
            src += components;
        }
    }
    else
    {
        // 2D case
        idx1 = *width*components;
        idx2 = (*width+1)*components;
        for( m = 0; m < halfheight; m ++ )
        {
            for( n = 0; n < halfwidth; n ++ )
            {
                for( k = 0; k < components; k ++ )
                {
                    *dst ++ = (GLubyte) (((int)*src +
                                          (int)src[components] +
                                          (int)src[idx1] +
                                          (int)src[idx2] + 2) >> 2);
                    src ++;
                }
                src += components;
            }
            src += components * (*width);
        }
    }

    // Return new width and height
    *width = halfwidth;
    *height = halfheight;

    return GL_TRUE;
}


//========================================================================
// Rescales an image into power-of-two dimensions
//========================================================================

static int RescaleImage( GLFWimage* image )
{
    int     width, height, log2, newsize;
    unsigned char *data;

    // Calculate next larger 2^N width
    for( log2 = 0, width = image->Width; width > 1; width >>= 1, log2 ++ )
      ;

    width  = (int) 1 << log2;
    if( width < image->Width )
    {
        width <<= 1;
    }

    // Calculate next larger 2^M height
    for( log2 = 0, height = image->Height; height > 1; height >>= 1, log2 ++ )
      ;

    height = (int) 1 << log2;
    if( height < image->Height )
    {
        height <<= 1;
    }

    // Do we really need to rescale?
    if( width != image->Width || height != image->Height )
    {
        // Allocate memory for new (upsampled) image data
        newsize = width * height * image->BytesPerPixel;
        data = (unsigned char *) malloc( newsize );
        if( data == NULL )
        {
            free( image->Data );
            return GL_FALSE;
        }

        // Copy old image data to new image data with interpolation
        UpsampleImage( image->Data, data, image->Width, image->Height,
                       width, height, image->BytesPerPixel );

        // Free memory for old image data (not needed anymore)
        free( image->Data );

        // Set pointer to new image data, and set new image dimensions
        image->Data   = data;
        image->Width  = width;
        image->Height = height;
    }

    return GL_TRUE;
}


//************************************************************************
//****                    GLFW user functions                         ****
//************************************************************************

//========================================================================
// Read an image from a named file
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwReadImage( const char *name, GLFWimage *img,
    int flags )
{
    _GLFWstream stream;

    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return GL_FALSE;
    }

    // Start with an empty image descriptor
    img->Width         = 0;
    img->Height        = 0;
    img->BytesPerPixel = 0;
    img->Data          = NULL;

    // Open file
    if( !_glfwOpenFileStream( &stream, name, "rb" ) )
    {
        return GL_FALSE;
    }

    // We only support TGA files at the moment
    if( !_glfwReadTGA( &stream, img, flags ) )
    {
        _glfwCloseStream( &stream );
        return GL_FALSE;
    }

    // Close stream
    _glfwCloseStream( &stream );

    // Should we rescale the image to closest 2^N x 2^M resolution?
    if( !(flags & GLFW_NO_RESCALE_BIT) )
    {
        if( !RescaleImage( img ) )
        {
            return GL_FALSE;
        }
    }

    // Interpret BytesPerPixel as an OpenGL format
    switch( img->BytesPerPixel )
    {
        default:
        case 1:
            if( flags & GLFW_ALPHA_MAP_BIT )
            {
                img->Format = GL_ALPHA;
            }
            else
            {
                img->Format = GL_LUMINANCE;
            }
            break;
        case 3:
            img->Format = GL_RGB;
            break;
        case 4:
            img->Format = GL_RGBA;
            break;
    }

    return GL_TRUE;
}


//========================================================================
// Read an image file from a memory buffer
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwReadMemoryImage( const void *data, long size, GLFWimage *img, int flags )
{
    _GLFWstream stream;

    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return GL_FALSE;
    }

    // Start with an empty image descriptor
    img->Width         = 0;
    img->Height        = 0;
    img->BytesPerPixel = 0;
    img->Data          = NULL;

    // Open buffer
    if( !_glfwOpenBufferStream( &stream, (void*) data, size ) )
    {
        return GL_FALSE;
    }

    // We only support TGA files at the moment
    if( !_glfwReadTGA( &stream, img, flags ) )
    {
        _glfwCloseStream( &stream );
        return GL_FALSE;
    }

    // Close stream
    _glfwCloseStream( &stream );

    // Should we rescale the image to closest 2^N x 2^M resolution?
    if( !(flags & GLFW_NO_RESCALE_BIT) )
    {
        if( !RescaleImage( img ) )
        {
            return GL_FALSE;
        }
    }

    // Interpret BytesPerPixel as an OpenGL format
    switch( img->BytesPerPixel )
    {
        default:
        case 1:
            if( flags & GLFW_ALPHA_MAP_BIT )
            {
                img->Format = GL_ALPHA;
            }
            else
            {
                img->Format = GL_LUMINANCE;
            }
            break;
        case 3:
            img->Format = GL_RGB;
            break;
        case 4:
            img->Format = GL_RGBA;
            break;
    }

    return GL_TRUE;
}


//========================================================================
// Free allocated memory for an image
//========================================================================

GLFWAPI void GLFWAPIENTRY glfwFreeImage( GLFWimage *img )
{
    // Is GLFW initialized?
    if( !_glfwInitialized )
    {
        return;
    }

    // Free memory
    if( img->Data != NULL )
    {
        free( img->Data );
        img->Data = NULL;
    }

    // Clear all fields
    img->Width         = 0;
    img->Height        = 0;
    img->Format        = 0;
    img->BytesPerPixel = 0;
}


//========================================================================
// Read an image from a file, and upload it to texture memory
//========================================================================

GLFWAPI int GLFWAPIENTRY glfwLoadTexture2D( const char *name, int flags )
{
    GLFWimage img;

    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return GL_FALSE;
    }

    // Force rescaling if necessary
    if( !_glfwWin.has_GL_ARB_texture_non_power_of_two )
    {
        flags &= (~GLFW_NO_RESCALE_BIT);
    }

    // Read image from file
    if( !glfwReadImage( name, &img, flags ) )
    {
        return GL_FALSE;
    }

    if( !glfwLoadTextureImage2D( &img, flags ) )
    {
        return GL_FALSE;
    }

    // Data buffer is not needed anymore
    glfwFreeImage( &img );

    return GL_TRUE;
}


//========================================================================
// Read an image from a buffer, and upload it to texture memory
//========================================================================

GLFWAPI int  GLFWAPIENTRY glfwLoadMemoryTexture2D( const void *data, long size, int flags )
{
    GLFWimage img;

    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return GL_FALSE;
    }

    // Force rescaling if necessary
    if( !_glfwWin.has_GL_ARB_texture_non_power_of_two )
    {
        flags &= (~GLFW_NO_RESCALE_BIT);
    }

    // Read image from file
    if( !glfwReadMemoryImage( data, size, &img, flags ) )
    {
        return GL_FALSE;
    }

    if( !glfwLoadTextureImage2D( &img, flags ) )
    {
        return GL_FALSE;
    }

    // Data buffer is not needed anymore
    glfwFreeImage( &img );

    return GL_TRUE;
}


//========================================================================
// Upload an image object to texture memory
//========================================================================

GLFWAPI int  GLFWAPIENTRY glfwLoadTextureImage2D( GLFWimage *img, int flags )
{
    GLint   UnpackAlignment, GenMipMap;
    int     level, format, AutoGen, newsize, n;
    unsigned char *data, *dataptr;

    // Is GLFW initialized?
    if( !_glfwInitialized || !_glfwWin.opened )
    {
        return GL_FALSE;
    }

    // TODO: Use GL_MAX_TEXTURE_SIZE or GL_PROXY_TEXTURE_2D to determine
    //       whether the image size is valid.
    // NOTE: May require box filter downsampling routine.

    // Do we need to convert the alpha map to RGBA format (OpenGL 1.0)?
    if( (_glfwWin.glMajor == 1) && (_glfwWin.glMinor == 0) &&
        (img->Format == GL_ALPHA) )
    {
        // We go to RGBA representation instead
        img->BytesPerPixel = 4;

        // Allocate memory for new RGBA image data
        newsize = img->Width * img->Height * img->BytesPerPixel;
        data = (unsigned char *) malloc( newsize );
        if( data == NULL )
        {
            free( img->Data );
            return GL_FALSE;
        }

        // Convert Alpha map to RGBA
        dataptr = data;
        for( n = 0; n < (img->Width*img->Height); ++ n )
        {
            *dataptr ++ = 255;
            *dataptr ++ = 255;
            *dataptr ++ = 255;
            *dataptr ++ = img->Data[n];
        }

        // Free memory for old image data (not needed anymore)
        free( img->Data );

        // Set pointer to new image data
        img->Data = data;
    }

    // Set unpack alignment to one byte
    glGetIntegerv( GL_UNPACK_ALIGNMENT, &UnpackAlignment );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );

    // Should we use automatic mipmap generation?
    AutoGen = ( flags & GLFW_BUILD_MIPMAPS_BIT ) &&
              _glfwWin.has_GL_SGIS_generate_mipmap;

    // Enable automatic mipmap generation
    if( AutoGen )
    {
        glGetTexParameteriv( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
            &GenMipMap );
        glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
            GL_TRUE );
    }

    // Format specification is different for OpenGL 1.0
    if( _glfwWin.glMajor == 1 && _glfwWin.glMinor == 0 )
    {
        format = img->BytesPerPixel;
    }
    else
    {
        format = img->Format;
    }

    // Upload to texture memeory
    level = 0;
    do
    {
        // Upload this mipmap level
        glTexImage2D( GL_TEXTURE_2D, level, format,
            img->Width, img->Height, 0, format,
            GL_UNSIGNED_BYTE, (void*) img->Data );

        // Build next mipmap level manually, if required
        if( ( flags & GLFW_BUILD_MIPMAPS_BIT ) && !AutoGen )
        {
            level = HalveImage( img->Data, &img->Width,
                        &img->Height, img->BytesPerPixel ) ?
                    level + 1 : 0;
        }
    }
    while( level != 0 );

    // Restore old automatic mipmap generation state
    if( AutoGen )
    {
        glTexParameteri( GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
            GenMipMap );
    }

    // Restore old unpack alignment
    glPixelStorei( GL_UNPACK_ALIGNMENT, UnpackAlignment );

    return GL_TRUE;
}

