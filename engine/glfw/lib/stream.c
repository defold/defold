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

#define _CRT_SECURE_NO_WARNINGS

#include "internal.h"


//========================================================================
// Opens a GLFW stream with a file
//========================================================================

int _glfwOpenFileStream( _GLFWstream *stream, const char* name, const char* mode )
{
    memset( stream, 0, sizeof(_GLFWstream) );

    stream->file = fopen( name, mode );
    if( stream->file == NULL )
    {
        return GL_FALSE;
    }

    return GL_TRUE;
}


//========================================================================
// Opens a GLFW stream with a memory block
//========================================================================

int _glfwOpenBufferStream( _GLFWstream *stream, void *data, long size )
{
    memset( stream, 0, sizeof(_GLFWstream) );

    stream->data = data;
    stream->size = size;
    return GL_TRUE;
}


//========================================================================
// Reads data from a GLFW stream
//========================================================================

long _glfwReadStream( _GLFWstream *stream, void *data, long size )
{
    if( stream->file != NULL )
    {
        return (long) fread( data, 1, size, stream->file );
    }

    if( stream->data != NULL )
    {
        // Check for EOF
        if( stream->position == stream->size )
        {
            return 0;
        }

        // Clamp read size to available data
        if( stream->position + size > stream->size )
        {
            size = stream->size - stream->position;
        }

        // Perform data read
        memcpy( data, (unsigned char*) stream->data + stream->position, size );
        stream->position += size;
        return size;
    }

    return 0;
}


//========================================================================
// Returns the current position of a GLFW stream
//========================================================================

long _glfwTellStream( _GLFWstream *stream )
{
    if( stream->file != NULL )
    {
        return ftell( stream->file );
    }

    if( stream->data != NULL )
    {
        return stream->position;
    }

    return 0;
}


//========================================================================
// Sets the current position of a GLFW stream
//========================================================================

int _glfwSeekStream( _GLFWstream *stream, long offset, int whence )
{
    long position;

    if( stream->file != NULL )
    {
        if( fseek( stream->file, offset, whence ) != 0 )
        {
            return GL_FALSE;
        }

        return GL_TRUE;
    }

    if( stream->data != NULL )
    {
        position = offset;

        // Handle whence parameter
        if( whence == SEEK_CUR )
        {
            position += stream->position;
        }
        else if( whence == SEEK_END )
        {
            position += stream->size;
        }
        else if( whence != SEEK_SET )
        {
            return GL_FALSE;
        }

        // Clamp offset to buffer bounds and apply it
        if( position > stream->size )
        {
            stream->position = stream->size;
        }
        else if( position < 0 )
        {
            stream->position = 0;
        }
        else
        {
            stream->position = position;
        }

        return GL_TRUE;
    }

    return GL_FALSE;
}


//========================================================================
// Closes a GLFW stream
//========================================================================

void _glfwCloseStream( _GLFWstream *stream )
{
    if( stream->file != NULL )
    {
        fclose( stream->file );
    }

    // Nothing to be done about (user allocated) memory blocks

    memset( stream, 0, sizeof(_GLFWstream) );
}

