/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gles_util.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>

namespace vkquality {

void GLESUtil::GetGLESStrings(std::string& renderer, std::string& version, std::string& vendor) {
  renderer = "";
  version = "";
  vendor = "";
  EGLDisplay egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (!eglInitialize(egl_display, nullptr, nullptr)) {
      return;
  }

  EGLConfig egl_config;
  // Initialize an RGBA8 window and pbuffer surface
  constexpr EGLint config_attributes[] = {EGL_RED_SIZE,     8,
                                          EGL_GREEN_SIZE,   8,
                                          EGL_BLUE_SIZE,    8,
                                          EGL_ALPHA_SIZE,   8,
                                          EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
                                          EGL_NONE};

  EGLint config_count;
  if (eglChooseConfig(egl_display, config_attributes, &egl_config, 1, &config_count) != EGL_TRUE) {
    eglTerminate(egl_display);
    return;
  }
  if (config_count != 1) {
    eglTerminate(egl_display);
    return;
  }

  EGLint surfaceType = 0;
  eglGetConfigAttrib(egl_display, egl_config, EGL_SURFACE_TYPE, &surfaceType);
  bool supports_pbuffers    = (surfaceType & EGL_PBUFFER_BIT) != 0;
  EGLint bindToTextureRGBA = 0;
  eglGetConfigAttrib(egl_display, egl_config, EGL_BIND_TO_TEXTURE_RGBA, &bindToTextureRGBA);
  bool supports_bind_tex_image = (bindToTextureRGBA == EGL_TRUE);

  const EGLint pbuffer_attributes[] = {
      EGL_WIDTH,          64,
      EGL_HEIGHT,         64,
      EGL_TEXTURE_FORMAT, supports_pbuffers ? EGL_TEXTURE_RGBA : EGL_NO_TEXTURE,
      EGL_TEXTURE_TARGET, supports_bind_tex_image ? EGL_TEXTURE_2D : EGL_NO_TEXTURE,
      EGL_NONE,           EGL_NONE,
  };

  EGLSurface egl_surface = eglCreatePbufferSurface(egl_display, egl_config, pbuffer_attributes);;
  const EGLint eglError = eglGetError();
  if (eglError != EGL_SUCCESS)
  {
    eglTerminate(egl_display);
    return;
  }
  const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  EGLContext egl_context = eglCreateContext(egl_display, egl_config, nullptr, context_attribs);
  if (eglError != EGL_SUCCESS)
  {
    eglDestroySurface(egl_display, egl_surface);
    eglTerminate(egl_display);
    return;
  }

  if (eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context) == EGL_TRUE) {
    renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  }

  eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(egl_display, egl_context);
  eglDestroySurface(egl_display, egl_surface);
  eglTerminate(egl_display);
}

} // namespace vkquality