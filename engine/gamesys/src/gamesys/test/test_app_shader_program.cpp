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

// Reproduces the WebGL error:
//   TypeError: WebGL2RenderingContext.attachShader: Argument 2 is not an object.
//
// This is a dedicated gamesys test because it exercises the production shader
// resource callbacks in addition to the graphics backend. It writes ShaderDesc
// resources to disk, then verifies creation, reload, compiler/linker diagnostics,
// and recovery after repeated failures using a real OpenGL/OpenGL ES context.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <testmain/testmain.h>

#include <ddf/ddf.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <graphics/graphics.h>
#include <platform/window.hpp>
#include <resource/resource.h>

#include "../resources/res_shader_program.h"

struct TestContext
{
    HWindow              m_Window;
    dmGraphics::HContext m_GraphicsContext;
    HJobContext          m_JobContext;
    bool                 m_Failed;
};

class ShaderErrorLogCapture;
static ShaderErrorLogCapture* g_ShaderErrorLogCapture = 0;

static void                   CaptureShaderErrorLog(LogSeverity severity, const char* domain, const char* formatted_string);

class ShaderErrorLogCapture
{
    public:
    ShaderErrorLogCapture()
    {
        assert(g_ShaderErrorLogCapture == 0);
        g_ShaderErrorLogCapture = this;
        dmLogRegisterListener(CaptureShaderErrorLog);
        Clear();
    }

    ~ShaderErrorLogCapture()
    {
        dmLogUnregisterListener(CaptureShaderErrorLog);
        g_ShaderErrorLogCapture = 0;
    }

    void Append(const char* formatted_string)
    {
        uint32_t length = (uint32_t)strlen(formatted_string);
        m_Output.OffsetCapacity(length + 1);
        m_Output.PushArray(formatted_string, length);
    }

    void Clear()
    {
        WaitForPendingLogs();
        m_Output.SetSize(0);
    }

    bool Contains(const char* text)
    {
        return strstr(Output(), text) != 0;
    }

    const char* Output()
    {
        WaitForPendingLogs();
        if (m_Output.Size() == 0)
            return "";
        if (m_Output[m_Output.Size() - 1] != 0)
        {
            m_Output.OffsetCapacity(1);
            m_Output.Push(0);
        }
        return m_Output.Begin();
    }

    private:
    void WaitForPendingLogs()
    {
        uint64_t stop_time = dmTime::GetMonotonicTime() + 5000000;
        while (dmLog::GetPendingLogCount() != 0 && dmTime::GetMonotonicTime() < stop_time)
            dmTime::Sleep(1000);
    }

    dmArray<char> m_Output;
};

static void CaptureShaderErrorLog(LogSeverity severity, const char* domain, const char* formatted_string)
{
    if (severity >= LOG_SEVERITY_ERROR && g_ShaderErrorLogCapture)
        g_ShaderErrorLogCapture->Append(formatted_string);
}

static void AddShader(dmGraphics::ShaderDesc* shader_desc, dmGraphics::ShaderDesc::ShaderType type, dmGraphics::ShaderDesc::Language language, const char* source)
{
    uint32_t count = shader_desc->m_Shaders.m_Count;
    shader_desc->m_Shaders.m_Data = (dmGraphics::ShaderDesc::Shader*)realloc(
    shader_desc->m_Shaders.m_Data, sizeof(dmGraphics::ShaderDesc::Shader) * (count + 1));

    dmGraphics::ShaderDesc::Shader* shader = &shader_desc->m_Shaders.m_Data[count];
    memset(shader, 0, sizeof(*shader));
    shader->m_Language = language;
    shader->m_ShaderType = type;
    shader->m_Source.m_Data = (uint8_t*)source;
    shader->m_Source.m_Count = (uint32_t)strlen(source);
    shader_desc->m_Shaders.m_Count++;
}

static void DeleteShaderDesc(dmGraphics::ShaderDesc* shader_desc)
{
    free(shader_desc->m_Shaders.m_Data);
    shader_desc->m_Shaders.m_Data = 0;
    shader_desc->m_Shaders.m_Count = 0;
}

static void AddGraphicsShaderVariants(dmGraphics::ShaderDesc* shader_desc, dmGraphics::ShaderDesc::ShaderType type, const char* glsl, const char* gles300, const char* gles100)
{
    AddShader(shader_desc, type, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM330, glsl);
    AddShader(shader_desc, type, dmGraphics::ShaderDesc::LANGUAGE_GLES_SM300, gles300);
    AddShader(shader_desc, type, dmGraphics::ShaderDesc::LANGUAGE_GLES_SM100, gles100);
}

static void MakeGraphicsShaderDesc(dmGraphics::ShaderDesc* shader_desc, bool invalid_vertex, bool invalid_fragment, bool link_mismatch, const char* vertex_path, const char* fragment_path)
{
    static const char* valid_glsl_vertex =
    "#version 330\nin vec4 position;\nout vec4 stage_value;\n"
    "void main() { stage_value = vec4(1.0); gl_Position = position; }\n";
    static const char* valid_gles300_vertex =
    "#version 300 es\nin vec4 position;\nout vec4 stage_value;\n"
    "void main() { stage_value = vec4(1.0); gl_Position = position; }\n";
    static const char* valid_gles100_vertex =
    "attribute vec4 position;\nvarying vec4 stage_value;\n"
    "void main() { stage_value = vec4(1.0); gl_Position = position; }\n";
    static const char* invalid_glsl_vertex = "#version 330\nthis is not valid GLSL\n";
    static const char* invalid_gles300_vertex = "#version 300 es\nthis is not valid GLSL ES\n";
    static const char* invalid_gles100_vertex = "this is not valid GLSL ES\n";

    static const char* valid_glsl_fragment =
    "#version 330\nin vec4 stage_value;\nout vec4 color;\nvoid main() { color = stage_value; }\n";
    static const char* valid_gles300_fragment =
    "#version 300 es\nprecision mediump float;\nin vec4 stage_value;\nout vec4 color;\n"
    "void main() { color = stage_value; }\n";
    static const char* valid_gles100_fragment =
    "precision mediump float;\nvarying vec4 stage_value;\nvoid main() { gl_FragColor = stage_value; }\n";
    static const char* invalid_glsl_fragment = "#version 330\nthis is not valid GLSL\n";
    static const char* invalid_gles300_fragment = "#version 300 es\nthis is not valid GLSL ES\n";
    static const char* invalid_gles100_fragment = "this is not valid GLSL ES\n";
    static const char* mismatch_glsl_fragment =
    "#version 330\nin vec3 stage_value;\nout vec4 color;\nvoid main() { color = vec4(stage_value, 1.0); }\n";
    static const char* mismatch_gles300_fragment =
    "#version 300 es\nprecision mediump float;\nin vec3 stage_value;\nout vec4 color;\n"
    "void main() { color = vec4(stage_value, 1.0); }\n";
    static const char* mismatch_gles100_fragment =
    "precision mediump float;\nvarying vec3 stage_value;\n"
    "void main() { gl_FragColor = vec4(stage_value, 1.0); }\n";

    memset(shader_desc, 0, sizeof(*shader_desc));
    shader_desc->m_VertexProgram = vertex_path;
    shader_desc->m_FragmentProgram = fragment_path;

    AddGraphicsShaderVariants(shader_desc, dmGraphics::ShaderDesc::SHADER_TYPE_VERTEX, invalid_vertex ? invalid_glsl_vertex : valid_glsl_vertex, invalid_vertex ? invalid_gles300_vertex : valid_gles300_vertex, invalid_vertex ? invalid_gles100_vertex : valid_gles100_vertex);
    AddGraphicsShaderVariants(shader_desc, dmGraphics::ShaderDesc::SHADER_TYPE_FRAGMENT, invalid_fragment ? invalid_glsl_fragment : (link_mismatch ? mismatch_glsl_fragment : valid_glsl_fragment), invalid_fragment ? invalid_gles300_fragment : (link_mismatch ? mismatch_gles300_fragment : valid_gles300_fragment), invalid_fragment ? invalid_gles100_fragment : (link_mismatch ? mismatch_gles100_fragment : valid_gles100_fragment));
}

static void MakeInvalidComputeShaderDesc(dmGraphics::ShaderDesc* shader_desc, const char* compute_path)
{
    static const char* invalid_compute = "#version 430\nthis is not valid GLSL\n";
    memset(shader_desc, 0, sizeof(*shader_desc));
    shader_desc->m_ComputeProgram = compute_path;
    AddShader(shader_desc, dmGraphics::ShaderDesc::SHADER_TYPE_COMPUTE, dmGraphics::ShaderDesc::LANGUAGE_GLSL_SM430, invalid_compute);
}

static bool WriteShaderResource(TestContext* context, const char* filename, dmGraphics::ShaderDesc* shader_desc)
{
    dmDDF::Result result = dmDDF::SaveMessageToFile(shader_desc, dmGraphics::ShaderDesc::m_DDFDescriptor, filename);
    DeleteShaderDesc(shader_desc);
    if (result == dmDDF::RESULT_OK)
        return true;

    dmLogError("Failed to write shader resource '%s': %d", filename, result);
    context->m_Failed = true;
    return false;
}

static void ExpectLog(TestContext* context, ShaderErrorLogCapture& log, const char* resource_path, const char* source_path, const char* operation)
{
    if (log.Contains(resource_path) &&
        log.Contains(source_path) &&
        log.Contains(operation) &&
        log.Contains("Variant") &&
        log.Contains("LANGUAGE_") &&
        log.Contains("(base)") &&
        log.Contains("Error:") &&
        !log.Contains("Error: Unknown"))
    {
        return;
    }

    dmLogError("Shader diagnostic did not contain the expected details:\n%s", log.Output());
    context->m_Failed = true;
}

static void ExpectCreateFailure(TestContext* context, dmResource::HFactory factory, ShaderErrorLogCapture& log, const char* resource_path, const char* source_path, const char* operation)
{
    log.Clear();
    dmGraphics::HProgram program = 0;
    dmResource::Result   result = dmResource::Get(factory, resource_path, (void**)&program);
    if (result == dmResource::RESULT_FORMAT_ERROR)
    {
        ExpectLog(context, log, resource_path, source_path, operation);
        return;
    }

    dmLogError("Loading invalid shader resource '%s' returned %d instead of RESULT_FORMAT_ERROR", resource_path, result);
    context->m_Failed = true;
    if (result == dmResource::RESULT_OK)
        dmResource::Release(factory, (void*)program);
}

static void ExpectValidProgram(TestContext* context, dmResource::HFactory factory, const char* resource_path)
{
    dmGraphics::HProgram program = 0;
    dmResource::Result   result = dmResource::Get(factory, resource_path, (void**)&program);
    if (result != dmResource::RESULT_OK || !program)
    {
        dmLogError("Loading valid shader resource '%s' failed with %d", resource_path, result);
        context->m_Failed = true;
        return;
    }
    dmResource::Release(factory, (void*)program);
}

static void DeleteTestResources()
{
    remove("shader_valid.spc");
    remove("shader_invalid_vertex.spc");
    remove("shader_invalid_fragment.spc");
    remove("shader_invalid_link.spc");
    remove("shader_invalid_reload.spc");
    remove("shader_invalid_compute.spc");
}

static void RunShaderFailureDiagnostics(TestContext* context)
{
    static const char* valid_file = "shader_valid.spc";
    static const char* valid_path = "/shader_valid.spc";
    static const char* invalid_vertex_file = "shader_invalid_vertex.spc";
    static const char* invalid_vertex_path = "/shader_invalid_vertex.spc";
    static const char* invalid_fragment_file = "shader_invalid_fragment.spc";
    static const char* invalid_fragment_path = "/shader_invalid_fragment.spc";
    static const char* invalid_link_file = "shader_invalid_link.spc";
    static const char* invalid_link_path = "/shader_invalid_link.spc";
    static const char* reload_file = "shader_invalid_reload.spc";
    static const char* reload_path = "/shader_invalid_reload.spc";
    static const char* invalid_compute_file = "shader_invalid_compute.spc";
    static const char* invalid_compute_path = "/shader_invalid_compute.spc";
    static const char* vertex_source_path = "/materials/diagnostic.vp";
    static const char* fragment_source_path = "/materials/diagnostic.fp";
    static const char* compute_source_path = "/materials/diagnostic.cp";

    DeleteTestResources();

    dmGraphics::ShaderDesc shader_desc;
    MakeGraphicsShaderDesc(&shader_desc, false, false, false, vertex_source_path, fragment_source_path);
    if (!WriteShaderResource(context, valid_file, &shader_desc))
        return;
    MakeGraphicsShaderDesc(&shader_desc, true, false, false, vertex_source_path, fragment_source_path);
    if (!WriteShaderResource(context, invalid_vertex_file, &shader_desc))
        return;
    MakeGraphicsShaderDesc(&shader_desc, false, true, false, vertex_source_path, fragment_source_path);
    if (!WriteShaderResource(context, invalid_fragment_file, &shader_desc))
        return;
    MakeGraphicsShaderDesc(&shader_desc, false, false, true, vertex_source_path, fragment_source_path);
    if (!WriteShaderResource(context, invalid_link_file, &shader_desc))
        return;
    MakeGraphicsShaderDesc(&shader_desc, false, false, false, vertex_source_path, fragment_source_path);
    if (!WriteShaderResource(context, reload_file, &shader_desc))
        return;

    dmResource::NewFactoryParams factory_params;
    factory_params.m_MaxResources = 16;
    factory_params.m_JobThreadContext = context->m_JobContext;
    dmResource::HFactory factory = dmResource::NewFactory(&factory_params, ".");
    if (!factory)
    {
        dmLogError("Failed to create resource factory");
        context->m_Failed = true;
        return;
    }

    dmResource::Result register_result = dmResource::RegisterType(factory, "spc", context->m_GraphicsContext, dmGameSystem::ResShaderProgramPreload, dmGameSystem::ResShaderProgramCreate, 0, dmGameSystem::ResShaderProgramDestroy, dmGameSystem::ResShaderProgramRecreate);
    if (register_result != dmResource::RESULT_OK)
    {
        dmLogError("Failed to register shader resource type: %d", register_result);
        context->m_Failed = true;
    }
    else
    {
        ShaderErrorLogCapture log;
        ExpectCreateFailure(context, factory, log, invalid_vertex_path, vertex_source_path, "Unable to compile vertex shader");
        ExpectValidProgram(context, factory, valid_path);
        ExpectCreateFailure(context, factory, log, invalid_fragment_path, fragment_source_path, "Unable to compile fragment shader");
        ExpectValidProgram(context, factory, valid_path);

        log.Clear();
        dmGraphics::HProgram program = 0;
        dmResource::Result   link_result = dmResource::Get(factory, invalid_link_path, (void**)&program);
        if (link_result == dmResource::RESULT_FORMAT_ERROR)
        {
            ExpectLog(context, log, invalid_link_path, vertex_source_path, "Unable to link shader program");
            if (!log.Contains(fragment_source_path))
                context->m_Failed = true;
        }
        else
        {
            dmLogError("Loading incompatible shader resource '%s' returned %d instead of RESULT_FORMAT_ERROR", invalid_link_path, link_result);
            context->m_Failed = true;
            if (link_result == dmResource::RESULT_OK)
                dmResource::Release(factory, (void*)program);
        }
        ExpectValidProgram(context, factory, valid_path);

        for (int i = 0; i < 4; ++i)
            ExpectCreateFailure(context, factory, log, invalid_vertex_path, vertex_source_path, "Unable to compile vertex shader");
        ExpectValidProgram(context, factory, valid_path);

        dmGraphics::HProgram reload_program = 0;
        dmResource::Result   load_reload_result = dmResource::Get(factory, reload_path, (void**)&reload_program);
        if (load_reload_result != dmResource::RESULT_OK)
        {
            dmLogError("Loading reload test resource '%s' failed with %d", reload_path, load_reload_result);
            context->m_Failed = true;
        }
        else
        {
            MakeGraphicsShaderDesc(&shader_desc, false, true, false, vertex_source_path, fragment_source_path);
            if (WriteShaderResource(context, reload_file, &shader_desc))
            {
                log.Clear();
                dmResource::Result reload_result = dmResource::ReloadResource(factory, reload_path, 0);
                if (reload_result == dmResource::RESULT_FORMAT_ERROR)
                {
                    ExpectLog(context, log, reload_path, fragment_source_path, "Unable to compile fragment shader");
                    if (!log.Contains("Failed to reload shader program"))
                        context->m_Failed = true;
                }
                else
                {
                    dmLogError("Reloading invalid shader resource '%s' returned %d instead of RESULT_FORMAT_ERROR", reload_path, reload_result);
                    context->m_Failed = true;
                }
            }
            dmResource::Release(factory, (void*)reload_program);
        }
        ExpectValidProgram(context, factory, valid_path);

        if (dmGraphics::GetInstalledAdapterFamily() == dmGraphics::ADAPTER_FAMILY_OPENGL &&
            dmGraphics::IsContextFeatureSupported(context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_COMPUTE_SHADER))
        {
            MakeInvalidComputeShaderDesc(&shader_desc, compute_source_path);
            if (WriteShaderResource(context, invalid_compute_file, &shader_desc))
            {
                ExpectCreateFailure(context, factory, log, invalid_compute_path, compute_source_path, "Unable to compile compute shader");
                ExpectValidProgram(context, factory, valid_path);
            }
        }
    }

    dmResource::DeleteFactory(factory);
    DeleteTestResources();
}

static dmGraphics::AdapterFamily GetAdapterFamily(int argc, char** argv)
{
#if defined(DM_TEST_APP_SHADER_PROGRAM_DEFAULT_OPENGLES)
    dmGraphics::AdapterFamily family = dmGraphics::ADAPTER_FAMILY_OPENGLES;
#else
    dmGraphics::AdapterFamily family = dmGraphics::ADAPTER_FAMILY_OPENGL;
#endif

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "opengl") == 0)
            family = dmGraphics::ADAPTER_FAMILY_OPENGL;
        else if (strcmp(argv[i], "opengles") == 0)
            family = dmGraphics::ADAPTER_FAMILY_OPENGLES;
    }
    return family;
}

static const char* GetAdapterName(dmGraphics::AdapterFamily family)
{
    return family == dmGraphics::ADAPTER_FAMILY_OPENGLES ? "opengles" : "opengl";
}

extern "C" void dmExportedSymbols();

int             main(int argc, char** argv)
{
    TestMainPlatformInit();
    dmExportedSymbols();

    dmLog::LogParams log_params;
    dmLog::LogInitialize(&log_params);

    TestContext               context = {};
    dmGraphics::AdapterFamily adapter_family = GetAdapterFamily(argc, argv);
    if (!dmGraphics::InstallAdapter(adapter_family))
    {
        dmLogError("Unable to install %s graphics adapter.", GetAdapterName(adapter_family));
        return 1;
    }

    context.m_Window = dmPlatform::NewWindow();
    WindowCreateParams window_params;
    WindowCreateParamsInitialize(&window_params);
    window_params.m_Width = 64;
    window_params.m_Height = 64;
    window_params.m_Title = "Shader Program Test";
    window_params.m_GraphicsApi = adapter_family == dmGraphics::ADAPTER_FAMILY_OPENGLES ? WINDOW_GRAPHICS_API_OPENGLES : WINDOW_GRAPHICS_API_OPENGL;

    WindowResult window_result = dmPlatform::OpenWindow(context.m_Window, window_params);
    if (window_result != WINDOW_RESULT_OK)
    {
        dmLogError("Failed to open window: %d", window_result);
        return 1;
    }

    JobSystemCreateParams job_params = {};
    job_params.m_ThreadCount = 1;
    context.m_JobContext = JobSystemCreate(&job_params);

    dmGraphics::ContextParams graphics_params = {};
    graphics_params.m_Window = context.m_Window;
    graphics_params.m_Width = window_params.m_Width;
    graphics_params.m_Height = window_params.m_Height;
    graphics_params.m_JobContext = context.m_JobContext;
    graphics_params.m_VerifyGraphicsCalls = 1;
    context.m_GraphicsContext = dmGraphics::NewContext(graphics_params);
    if (!context.m_GraphicsContext)
    {
        dmLogError("Failed to create graphics context");
        context.m_Failed = true;
    }
    else
    {
        RunShaderFailureDiagnostics(&context);
        dmGraphics::CloseWindow(context.m_GraphicsContext);
        dmGraphics::DeleteContext(context.m_GraphicsContext);
        dmGraphics::Finalize();
    }

    if (context.m_JobContext)
        JobSystemDestroy(context.m_JobContext);

    DeleteTestResources();
    return context.m_Failed ? 1 : 0;
}
