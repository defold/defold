///////////////////////////////////////////////////////////////////////////////////
//
//	graphics_device.h - Graphics device interface
//
///////////////////////////////////////////////////////////////////////////////////
#ifndef __GRAPHICSDEVICE_H__
#define __GRAPHICSDEVICE_H__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "opengl/opengl_device.h"



// primitive type
enum GFXPrimitiveType
{
    GFX_PRIMITIVE_POINTLIST         = GFX_DEVICE_PRIMITIVE_POINTLIST,
    GFX_PRIMITIVE_LINES             = GFX_DEVICE_PRIMITIVE_LINES,
    GFX_PRIMITIVE_LINE_LOOP         = GFX_DEVICE_PRIMITIVE_LINE_LOOP,
    GFX_PRIMITIVE_LINE_STRIP        = GFX_DEVICE_PRIMITIVE_LINE_STRIP,
    GFX_PRIMITIVE_TRIANGLES         = GFX_DEVICE_PRIMITIVE_TRIANGLES,
    GFX_PRIMITIVE_TRIANGLE_STRIP    = GFX_DEVICE_PRIMITIVE_TRIANGLE_STRIP,
    GFX_PRIMITIVE_TRIANGLE_FAN      = GFX_DEVICE_PRIMITIVE_TRIANGLE_FAN
};


// buffer clear types
enum GFXBufferClear
{
    GFX_CLEAR_RENDERBUFFER_R    = GFX_DEVICE_CLEAR_RENDERBUFFER_R,
    GFX_CLEAR_RENDERBUFFER_G    = GFX_DEVICE_CLEAR_RENDERBUFFER_G,
    GFX_CLEAR_RENDERBUFFER_B    = GFX_DEVICE_CLEAR_RENDERBUFFER_B,
    GFX_CLEAR_RENDERBUFFER_A    = GFX_DEVICE_CLEAR_RENDERBUFFER_A,
    GFX_CLEAR_RENDERBUFFER_ALL  = GFX_DEVICE_CLEAR_RENDERBUFFER_ALL,
    GFX_CLEAR_DEPTHBUFFER       = GFX_DEVICE_CLEAR_DEPTHBUFFER,
    GFX_CLEAR_STENCILBUFFER     = GFX_DEVICE_CLEAR_STENCILBUFFER
};

// bool states
enum GFXRenderState
{
    GFX_DEPTH_TEST               = GFX_DEVICE_STATE_DEPTH_TEST,
    GFX_ALPHA_BLEND              = GFX_DEVICE_STATE_ALPHA_TEST
};


enum GFXMatrixMode
{
    GFX_MATRIX_TYPE_WORLDVIEW   = GFX_DEVICE_MATRIX_TYPE_WORLDVIEW,
    GFX_MATRIX_TYPE_PROJECTION  = GFX_DEVICE_MATRIX_TYPE_PROJECTION
};



// Parameter structure for CreateDevice
struct GFXSCreateDeviceParams
{
    uint32_t        m_DisplayWidth;
    uint32_t        m_DisplayHeight;
    const char*	    m_AppTitle;
    bool            m_Fullscreen;
};

GFXHDevice GFXCreateDevice(int* argc, char** argv, GFXSCreateDeviceParams *params);

// clear/draw functions
void GFXFlip();
void GFXClear(GFXHContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil);
void GFXDrawTriangle2D(GFXHContext context, const float* vertices, const float* colours);
void GFXDrawTriangle3D(GFXHContext context, const float* vertices, const float* colours);

void GFXSetViewport(GFXHContext context, int width, int height, float field_of_view, float z_near, float z_far);

void GFXEnableState(GFXHContext context, GFXRenderState state);
void GFXDisableState(GFXHContext context, GFXRenderState state);
void GFXSetMatrix(GFXHContext context, GFXMatrixMode matrix_mode, const float* matrix);



#endif	// __GRAPHICSDEVICE_H__


