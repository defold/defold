#include <graphics/glfw/glfw.h>
#include <graphics/glfw/glfw_native.h>

#include <dlib/log.h>

namespace dmGraphics
{
	bool NativeInit()
    {
        bool ret = glfwInit() ? true : false;
        if (!ret) {
            dmLogError("Could not initialize glfw.");
        }
    }

    void NativeExit()
    {
        glfwTerminate();
    }

    WindowResult OpenWindow(HContext context, WindowParams *params)
    {
        assert(context);
        assert(params);

        if (context->m_WindowOpened) return WINDOW_RESULT_ALREADY_OPENED;

        if (params->m_HighDPI) {
            glfwOpenWindowHint(GLFW_WINDOW_HIGH_DPI, 1);
        }

        glfwOpenWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        glfwOpenWindowHint(GLFW_FSAA_SAMPLES, params->m_Samples);

#if defined(GL_HAS_RENDERDOC_SUPPORT)
        if (context->m_RenderDocSupport)
        {
            glfwOpenWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
            glfwOpenWindowHint(GLFW_OPENGL_VERSION_MINOR, 2);
            glfwOpenWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        }
#endif

        int mode = GLFW_WINDOW;
        if (params->m_Fullscreen)
            mode = GLFW_FULLSCREEN;
        if (!glfwOpenWindow(params->m_Width, params->m_Height, 8, 8, 8, 8, 32, 8, mode))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

#if defined (_WIN32)
#define GET_PROC_ADDRESS(function, name, type)\
        function = (type)wglGetProcAddress(name);\
        if (function == 0x0)\
        {\
            function = (type)wglGetProcAddress(name "ARB");\
        }\
        if (function == 0x0)\
        {\
            function = (type)wglGetProcAddress(name "EXT");\
        }\
        if (function == 0x0)\
        {\
            dmLogError("Could not find gl function '%s'.", name);\
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;\
        }

        GET_PROC_ADDRESS(glGenProgramsARB, "glGenPrograms", PFNGLGENPROGRAMARBPROC);
        GET_PROC_ADDRESS(glBindProgramARB, "glBindProgram", PFNGLBINDPROGRAMARBPROC);
        GET_PROC_ADDRESS(glDeleteProgramsARB, "glDeletePrograms", PFNGLDELETEPROGRAMSARBPROC);
        GET_PROC_ADDRESS(glProgramStringARB, "glProgramString", PFNGLPROGRAMSTRINGARBPROC);
        GET_PROC_ADDRESS(glProgramLocalParameter4fARB, "glProgramLocalParameter4f", PFNGLVERTEXPARAMFLOAT4ARBPROC);
        GET_PROC_ADDRESS(glEnableVertexAttribArray, "glEnableVertexAttribArray", PFNGLVERTEXATTRIBSETPROC);
        GET_PROC_ADDRESS(glDisableVertexAttribArray, "glDisableVertexAttribArray", PFNGLVERTEXATTRIBSETPROC);
        GET_PROC_ADDRESS(glVertexAttribPointer, "glVertexAttribPointer", PFNGLVERTEXATTRIBPTRPROC);
        GET_PROC_ADDRESS(glCompressedTexImage2D, "glCompressedTexImage2D", PFNGLTEXPARAM2DPROC);
        GET_PROC_ADDRESS(glCompressedTexSubImage2D, "glCompressedTexSubImage2D", PFNGLCOMPRTEXSUB2DPROC);
        GET_PROC_ADDRESS(glGenBuffersARB, "glGenBuffers", PFNGLGENBUFFERSPROC);
        GET_PROC_ADDRESS(glDeleteBuffersARB, "glDeleteBuffers", PFNGLDELETEBUFFERSPROC);
        GET_PROC_ADDRESS(glBindBufferARB, "glBindBuffer", PFNGLBINDBUFFERPROC);
        GET_PROC_ADDRESS(glBufferDataARB, "glBufferData", PFNGLBUFFERDATAPROC);
        GET_PROC_ADDRESS(glGenRenderbuffers, "glGenRenderbuffers", PFNGLGENRENDERBUFFERSPROC);
        GET_PROC_ADDRESS(glBindRenderbuffer, "glBindRenderbuffer", PFNGLBINDRENDERBUFFERPROC);
        GET_PROC_ADDRESS(glRenderbufferStorage, "glRenderbufferStorage", PFNGLRENDERBUFFERSTORAGEPROC);
        GET_PROC_ADDRESS(glFramebufferTexture2D, "glFramebufferTexture2D", PFNGLRENDERBUFFERTEXTURE2DPROC);
        GET_PROC_ADDRESS(glFramebufferRenderbuffer, "glFramebufferRenderbuffer", PFNGLFRAMEBUFFERRENDERBUFFERPROC);
        GET_PROC_ADDRESS(glGenFramebuffers, "glGenFramebuffers", PFNGLGENFRAMEBUFFERSPROC);
        GET_PROC_ADDRESS(glBindFramebuffer, "glBindFramebuffer", PFNGLBINDFRAMEBUFFERPROC);
        GET_PROC_ADDRESS(glDeleteFramebuffers, "glDeleteFramebuffers", PFNGLDELETEFRAMEBUFFERSPROC);
        GET_PROC_ADDRESS(glDeleteRenderbuffers, "glDeleteRenderbuffers", PFNGLDELETERENDERBUFFERSPROC);
        GET_PROC_ADDRESS(glBufferSubDataARB, "glBufferSubData", PFNGLBUFFERSUBDATAPROC);
        GET_PROC_ADDRESS(glMapBufferARB, "glMapBuffer", PFNGLMAPBUFFERPROC);
        GET_PROC_ADDRESS(glUnmapBufferARB, "glUnmapBuffer", PFNGLUNMAPBUFFERPROC);
        GET_PROC_ADDRESS(glActiveTexture, "glActiveTexture", PFNGLACTIVETEXTUREPROC);
        GET_PROC_ADDRESS(glCheckFramebufferStatus, "glCheckFramebufferStatus", PFNGLCHECKFRAMEBUFFERSTATUSPROC);
        GET_PROC_ADDRESS(glGetAttribLocation, "glGetAttribLocation", PFNGLGETATTRIBLOCATIONPROC);
        GET_PROC_ADDRESS(glCreateShader, "glCreateShader", PFNGLCREATESHADERPROC);
        GET_PROC_ADDRESS(glShaderSource, "glShaderSource", PFNGLSHADERSOURCEPROC);
        GET_PROC_ADDRESS(glCompileShader, "glCompileShader", PFNGLCOMPILESHADERPROC);
        GET_PROC_ADDRESS(glGetShaderiv, "glGetShaderiv", PFNGLGETSHADERIVPROC);
        GET_PROC_ADDRESS(glGetShaderInfoLog, "glGetShaderInfoLog", PFNGLGETSHADERINFOLOGPROC);
        GET_PROC_ADDRESS(glGetProgramInfoLog, "glGetProgramInfoLog", PFNGLGETPROGRAMINFOLOGPROC);
        GET_PROC_ADDRESS(glDeleteShader, "glDeleteShader", PFNGLDELETESHADERPROC);
        GET_PROC_ADDRESS(glCreateProgram, "glCreateProgram", PFNGLCREATEPROGRAMPROC);
        GET_PROC_ADDRESS(glAttachShader, "glAttachShader", PFNGLATTACHSHADERPROC);
        GET_PROC_ADDRESS(glLinkProgram, "glLinkProgram", PFNGLLINKPROGRAMPROC);
        GET_PROC_ADDRESS(glDeleteProgram, "glDeleteProgram", PFNGLDELETEPROGRAMPROC);
        GET_PROC_ADDRESS(glUseProgram, "glUseProgram", PFNGLUSEPROGRAMPROC);
        GET_PROC_ADDRESS(glGetProgramiv, "glGetProgramiv", PFNGLGETPROGRAMIVPROC);
        GET_PROC_ADDRESS(glGetActiveUniform, "glGetActiveUniform", PFNGLGETACTIVEUNIFORMPROC);
        GET_PROC_ADDRESS(glGetUniformLocation, "glGetUniformLocation", PFNGLGETUNIFORMLOCATIONPROC);
        GET_PROC_ADDRESS(glUniform4fv, "glUniform4fv", PFNGLUNIFORM4FVPROC);
        GET_PROC_ADDRESS(glUniformMatrix4fv, "glUniformMatrix4fv", PFNGLUNIFORMMATRIX4FVPROC);
        GET_PROC_ADDRESS(glUniform1i, "glUniform1i", PFNGLUNIFORM1IPROC);
#if defined(GL_HAS_RENDERDOC_SUPPORT)
        if (context->m_RenderDocSupport)
        {
            GET_PROC_ADDRESS(glGetStringi,"glGetStringi",PFNGLGETSTRINGIPROC);
            GET_PROC_ADDRESS(glGenVertexArrays, "glGenVertexArrays", PFNGLGENVERTEXARRAYSPROC);
            GET_PROC_ADDRESS(glBindVertexArray, "glBindVertexArray", PFNGLBINDVERTEXARRAYPROC);
        }
#endif

#undef GET_PROC_ADDRESS
#endif

#if !defined(__EMSCRIPTEN__)
        glfwSetWindowTitle(params->m_Title);
#endif

        glfwSetWindowSizeCallback(OnWindowResize);
        glfwSetWindowCloseCallback(OnWindowClose);
        glfwSetWindowFocusCallback(OnWindowFocus);
        glfwSetWindowIconifyCallback(OnWindowIconify);
        glfwSwapInterval(1);
        CHECK_GL_ERROR

        context->m_WindowResizeCallback           = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData   = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback            = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData    = params->m_CloseCallbackUserData;
        context->m_WindowFocusCallback            = params->m_FocusCallback;
        context->m_WindowFocusCallbackUserData    = params->m_FocusCallbackUserData;
        context->m_WindowIconifyCallback          = params->m_IconifyCallback;
        context->m_WindowIconifyCallbackUserData  = params->m_IconifyCallbackUserData;
        context->m_WindowOpened = 1;
        context->m_Width = params->m_Width;
        context->m_Height = params->m_Height;
        // read back actual window size
        int width, height;
        glfwGetWindowSize(&width, &height);
        context->m_WindowWidth = (uint32_t)width;
        context->m_WindowHeight = (uint32_t)height;
        context->m_Dpi = 0;

        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: OpenGL");
            dmLogInfo("Renderer: %s\n", (char *) glGetString(GL_RENDERER));
            dmLogInfo("Version: %s\n", (char *) glGetString(GL_VERSION));
            dmLogInfo("Vendor: %s\n", (char *) glGetString(GL_VENDOR));
            dmLogInfo("Extensions: %s\n", (char *) glGetString(GL_EXTENSIONS));
        }

#if defined(__MACH__) && !( defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR) )
        ProcessSerialNumber psn;
        OSErr err;

        // Move window to front. Required if running without application bundle.
        err = GetCurrentProcess( &psn );
        if (err == noErr)
            (void) SetFrontProcess( &psn );
#endif

#if defined(GL_HAS_RENDERDOC_SUPPORT)
        char* extensions_ptr = 0;
        if (context->m_RenderDocSupport)
        {
            GLint n;
            glGetIntegerv(GL_NUM_EXTENSIONS, &n);
            if (n > 0)
            {
                int max_len = 0;
                int cursor = 0;

                for (GLint i = 0; i < n; i++)
                {
                    char* ext = (char*) glGetStringi(GL_EXTENSIONS,i);
                    max_len += (int) strlen((const char*)ext) + 1;
                }

                extensions_ptr = (char*) malloc(max_len);

                for (GLint i = 0; i < n; i++)
                {
                    char* ext = (char*) glGetStringi(GL_EXTENSIONS,i);
                    int str_len = (int) strlen((const char*)ext);

                    strcpy(extensions_ptr + cursor, ext);

                    cursor += str_len;
                    extensions_ptr[cursor] = ' ';

                    cursor += 1;
                }
            }
        }
        else
        {
            extensions_ptr = (char*) glGetString(GL_EXTENSIONS);
        }

        const GLubyte* extensions = (const GLubyte*) extensions_ptr;
#else
        // Check texture format support
        const GLubyte* extensions = glGetString(GL_EXTENSIONS);
#endif

        DMGRAPHICS_GET_PROC_ADDRESS_EXT(PFN_glInvalidateFramebuffer, "glDiscardFramebuffer", "discard_framebuffer", "glInvalidateFramebuffer", DM_PFNGLINVALIDATEFRAMEBUFFERPROC, extensions);

        if (IsExtensionSupported("GL_IMG_texture_compression_pvrtc", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_PVRTC_4BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1;
        }
        if (IsExtensionSupported("GL_EXT_texture_compression_dxt1", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_DXT1;
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_DXT1;
        }
        if (IsExtensionSupported("GL_EXT_texture_compression_dxt3", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_DXT3;
        }
        if (IsExtensionSupported("GL_EXT_texture_compression_dxt5", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_DXT5;
        }
        if (IsExtensionSupported("GL_OES_compressed_ETC1_RGB8_texture", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
        }

#if defined (__EMSCRIPTEN__)
        // webgl GL_DEPTH_STENCIL_ATTACHMENT for stenciling and GL_DEPTH_COMPONENT16 for depth only by specifications, even though it reports 24-bit depth and no packed depth stencil extensions.
        context->m_PackedDepthStencil = 1;
        context->m_DepthBufferBits = 16;
#else

        if ((IsExtensionSupported("GL_OES_packed_depth_stencil", extensions)) || (IsExtensionSupported("GL_EXT_packed_depth_stencil", extensions)))
        {
            context->m_PackedDepthStencil = 1;
        }
        GLint depth_buffer_bits;
        glGetIntegerv( GL_DEPTH_BITS, &depth_buffer_bits );
        context->m_DepthBufferBits = (uint32_t) depth_buffer_bits;
#endif

        GLint gl_max_texture_size = 1024;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);
        context->m_MaxTextureSize = gl_max_texture_size;
        CLEAR_GL_ERROR

#if (defined(__arm__) || defined(__arm64__)) || defined(ANDROID) || defined(IOS_SIMULATOR)
        // Hardcoded values for iOS and Android for now. The value is a hint, max number of vertices will still work with performance penalty
        // The below seems to be the reported sweet spot for modern or semi-modern hardware
        context->m_MaxElementVertices = 1024*1024;
        context->m_MaxElementIndices = 1024*1024;
#else
        // We don't accept values lower than 65k. It's a trade-off on drawcalls vs bufferdata upload
        GLint gl_max_elem_verts = 65536;
        glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &gl_max_elem_verts);
        context->m_MaxElementVertices = dmMath::Max(65536, gl_max_elem_verts);
        CLEAR_GL_ERROR

        GLint gl_max_elem_indices = 65536;
        glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &gl_max_elem_indices);
        context->m_MaxElementIndices = dmMath::Max(65536, gl_max_elem_indices);
        CLEAR_GL_ERROR
#endif

        if (IsExtensionSupported("GL_OES_compressed_ETC1_RGB8_texture", extensions))
        {
            context->m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
        }

#if defined(__ANDROID__) || defined(__arm__) || defined(__arm64__) || defined(__EMSCRIPTEN__)
        if ((IsExtensionSupported("GL_OES_element_index_uint", extensions)))
        {
            context->m_IndexBufferFormatSupport |= 1 << INDEXBUFFER_FORMAT_32;
        }
#else
        context->m_IndexBufferFormatSupport |= 1 << INDEXBUFFER_FORMAT_32;
#endif

        JobQueueInitialize();
        if(JobQueueIsAsync())
        {
            if(!ValidateAsyncJobProcessing(context))
            {
                dmLogDebug("AsyncInitialize: Failed to verify async job processing. Fallback to single thread processing.");
                JobQueueFinalize();
            }
        }

#if defined(GL_HAS_RENDERDOC_SUPPORT)
        if (context->m_RenderDocSupport)
        {
            if (extensions_ptr)
            {
                free(extensions_ptr);
            }

            GLuint vao;
            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
        }
#endif

        return WINDOW_RESULT_OK;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            JobQueueFinalize();
            PostDeleteTextures(true);
            glfwCloseWindow();
            context->m_WindowResizeCallback = 0x0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
            context->m_WindowOpened = 0;
        }
    }

    void IconifyWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            glfwIconifyWindow();
        }
    }


    uint32_t GetWindowState(HContext context, WindowState state)
    {
        assert(context);
        if (context->m_WindowOpened)
            return glfwGetWindowParam(state);
        else
            return 0;
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
            return glfwGetWindowRefreshRate();
        else
            return 0;
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        assert(context);
        return context->m_Dpi;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        assert(context);
        return context->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        assert(context);
        return context->m_WindowHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_Width = width;
            context->m_Height = height;
            glfwSetWindowSize((int)width, (int)height);
            int window_width, window_height;
            glfwGetWindowSize(&window_width, &window_height);
            context->m_WindowWidth = window_width;
            context->m_WindowHeight = window_height;
            // The callback is not called from glfw when the size is set manually
            if (context->m_WindowResizeCallback)
            {
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, window_width, window_height);
            }
        }
    }

    void ResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            glfwSetWindowSize((int)width, (int)height);
        }
    }

    void SwapBuffers()
    {
        glfwSwapBuffers();
    }

    void SetSwapInterval(HContext context, uint32_t swap_interval)
    {
        glfwSwapInterval(swap_interval);
    }

    id GetNativeiOSUIWindow()               { return glfwGetiOSUIWindow(); }
    id GetNativeiOSUIView()                 { return glfwGetiOSUIView(); }
    id GetNativeiOSEAGLContext()            { return glfwGetiOSEAGLContext(); }
    id GetNativeOSXNSWindow()               { return glfwGetOSXNSWindow(); }
    id GetNativeOSXNSView()                 { return glfwGetOSXNSView(); }
    id GetNativeOSXNSOpenGLContext()        { return glfwGetOSXNSOpenGLContext(); }
    HWND GetNativeWindowsHWND()             { return glfwGetWindowsHWND(); }
    HGLRC GetNativeWindowsHGLRC()           { return glfwGetWindowsHGLRC(); }
    EGLContext GetNativeAndroidEGLContext() { return glfwGetAndroidEGLContext(); }
    EGLSurface GetNativeAndroidEGLSurface() { return glfwGetAndroidEGLSurface(); }
    JavaVM* GetNativeAndroidJavaVM()        { return glfwGetAndroidJavaVM(); }
    jobject GetNativeAndroidActivity()      { return glfwGetAndroidActivity(); }
    android_app* GetNativeAndroidApp()      { return glfwGetAndroidApp(); }
    Window GetNativeX11Window()             { return glfwGetX11Window(); }
    GLXContext GetNativeX11GLXContext()     { return glfwGetX11GLXContext(); }
}
