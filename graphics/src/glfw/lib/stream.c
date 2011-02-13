//========================================================================
// GLFW - An OpenGL framework
// File:        stream.c
// Platform:    Any
// API version: 2.6
// WWW:         http://glfw.sourceforge.net
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Camilla Berglund
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


#include "internal.h"


//========================================================================
// Opens a GLFW stream with a file
//========================================================================

int _glfwOpenFileStream( _GLFWstream *stream, const char* name, const char* mode )
{
    memset( stream, 0, sizeof(_GLFWstream) );

    stream->File = fopen( name, mode );
    if( stream->File == NULL )
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

    stream->Data = data;
    stream->Size = size;
    return GL_TRUE;
}


//========================================================================
// Reads data from a GLFW stream
//========================================================================

long _glfwReadStream( _GLFWstream *stream, void *data, long size )
{
    if( stream->File != NULL )
    {
	return fread( data, 1, size, stream->File );
    }

    if( stream->Data != NULL )
    {
	// Check for EOF
	if( stream->Position == stream->Size )
	{
	    return 0;
	}

	// Clamp read size to available data
	if( stream->Position + size > stream->Size )
	{
	    size = stream->Size - stream->Position;
	}
	
	// Perform data read
	memcpy( data, (unsigned char*) stream->Data + stream->Position, size );
	stream->Position += size;
	return size;
    }

    return 0;
}


//========================================================================
// Returns the current position of a GLFW stream
//========================================================================

long _glfwTellStream( _GLFWstream *stream )
{
    if( stream->File != NULL )
    {
	return ftell( stream->File );
    }

    if( stream->Data != NULL )
    {
	return stream->Position;
    }

    return 0;
}


//========================================================================
// Sets the current position of a GLFW stream
//========================================================================

int _glfwSeekStream( _GLFWstream *stream, long offset, int whence )
{
    long position;

    if( stream->File != NULL )
    {
	if( fseek( stream->File, offset, whence ) != 0 )
	{
	    return GL_FALSE;
	}

	return GL_TRUE;
    }

    if( stream->Data != NULL )
    {
	position = offset;

	// Handle whence parameter
	if( whence == SEEK_CUR )
	{
	    position += stream->Position;
	}
	else if( whence == SEEK_END )
	{
	    position += stream->Size;
	}
	else if( whence != SEEK_SET )
	{
	    return GL_FALSE;
	}

	// Clamp offset to buffer bounds and apply it
	if( position > stream->Size )
	{
	    stream->Position = stream->Size;
	}
	else if( position < 0 )
	{
	    stream->Position = 0;
	}
	else
	{
	    stream->Position = position;
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
    if( stream->File != NULL )
    {
	fclose( stream->File );
    }

    // Nothing to be done about (user allocated) memory blocks

    memset( stream, 0, sizeof(_GLFWstream) );
}

