#include <stdio.h>
#include <string.h>
#include <dlib/hash.h>
#include <graphics/glfw/glfw.h>
#include "../gui.h"

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

#ifdef __MACH__
#include <Carbon/Carbon.h>
#endif

#include <ddf/ddf.h>
#include <script/lua_source_ddf.h>

GLuint checker_texture;
dmGui::HContext g_GuiContext;

void OnWindowResize(int width, int height)
{
    dmGui::SetPhysicalResolution(g_GuiContext, (uint32_t)width, (uint32_t)height);
}

void MyRenderNodes(dmGui::HScene scene,
                  const dmGui::RenderEntry* nodes,
                  const Vectormath::Aos::Matrix4* node_transforms,
                  const float* node_opacities,
                  const dmGui::StencilScope** stencil_scopes,
                  uint32_t node_count,
                  void* context)
{
    for (uint32_t i = 0; i < node_count; ++i)
    {
        dmGui::HNode node = nodes[i].m_Node;
        Vector4 color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
        color = Vector4(color.getXYZ(), node_opacities[i]);
        glColor4fv((const GLfloat*)&color);

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, node);
        switch (blend_mode)
        {
            case dmGui::BLEND_MODE_ALPHA:
                glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;

            case dmGui::BLEND_MODE_ADD:
                glBlendFunc(GL_ONE, GL_ONE);
            break;

            case dmGui::BLEND_MODE_ADD_ALPHA:
                glBlendFunc(GL_ONE, GL_SRC_ALPHA);
                break;

            case dmGui::BLEND_MODE_MULT:
                glBlendFunc(GL_ZERO, GL_SRC_COLOR);
                break;

            default:
                printf("Unknown blend mode: %d\n", blend_mode);
                break;
        }

        if (dmGui::GetNodeTexture(scene, node))
        {
            glEnable(GL_TEXTURE_2D);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glBindTexture(GL_TEXTURE_2D, checker_texture);
        }
        else
        {
            glDisable(GL_TEXTURE_2D);
        }

        glPushMatrix();
        glMultMatrixf((const GLfloat*) &node_transforms[i]);

        glBegin(GL_TRIANGLE_STRIP);

        glTexCoord2f(0, 1);
        glVertex3f(0.0f, 0.0f, 0.0f);
        glTexCoord2f(0, 0);
        glVertex3f(0.0f, 1.0f, 0.0f);
        glTexCoord2f(1, 1);
        glVertex3f(1.0f, 0.0f, 0.0f);
        glTexCoord2f(1, 0);
        glVertex3f(1.0f, 1.0f, 0.0f);

        glEnd();

        glPopMatrix();
    }
}

const uint32_t SCENE_COUNT = 2;
const char* SCRIPTS[SCENE_COUNT] = {"src/test/guidemo_align.lua", "src/test/guidemo_anim.lua"};
dmGui::HScene g_Scenes[SCENE_COUNT];
uint32_t g_CurrentScene = 0;

void OnKey(int key, int state)
{
    dmGui::InputAction input_action;

    switch (key)
    {
    case GLFW_KEY_TAB:
        if (state)
        {
            g_CurrentScene = (g_CurrentScene + 1) % SCENE_COUNT;
        }
        break;
    case GLFW_KEY_SPACE:
        {
            input_action.m_ActionId = dmHashString64("SPACE");
            if (state)
            {
                input_action.m_Pressed = 1;
                input_action.m_Released = 0;
            }
            else
            {
                input_action.m_Pressed = 0;
                input_action.m_Released = 1;
            }
            bool consumed;
            dmGui::DispatchInput(g_Scenes[g_CurrentScene], &input_action, 1, &consumed);
        }
        break;
    }
}

int main(void)
{
    int running, x, y;
    int current_width = 700;
    int current_height = 700;
    const int width = 100;
    const int height = 100;
    float t;

    uint8_t checker_texture_data[] = {64, 64, 64, 255,
                                      128, 128, 128, 255,
                                      192, 192, 192, 255,
                                      255, 255, 255, 255};

    glfwInit();

    if (!glfwOpenWindow(current_width, current_height, 8, 8, 8, 8, 0, 0, GLFW_WINDOW))
    {
        glfwTerminate();
        return 1;
    }

    dmGui::NewContextParams context_params;
    context_params.m_ScriptContext = dmScript::NewContext(0, 0, true);
    g_GuiContext = dmGui::NewContext(&context_params);
    dmGui::HContext context = g_GuiContext;

    glfwSetWindowSizeCallback(OnWindowResize);

    glfwEnable(GLFW_STICKY_KEYS);
    glfwSwapInterval(1);

    glGenTextures(1, &checker_texture);
    glBindTexture(GL_TEXTURE_2D, checker_texture);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, &checker_texture_data[0]);

    dmGui::SetPhysicalResolution(context, current_width, current_height);

    dmGui::NewSceneParams params;
    params.m_MaxNodes = 256;
    params.m_MaxAnimations = 1024;
    for (uint32_t i = 0; i < SCENE_COUNT; ++i)
    {
        dmGui::HScene scene = dmGui::NewScene(context, &params);
        dmGui::SetSceneResolution(scene, width, height);
        dmGui::HScript script = dmGui::NewScript(context);
        dmGui::SetSceneScript(scene, script);
        g_Scenes[i] = scene;

        const char* script_file = SCRIPTS[i];
        FILE*f = fopen(script_file, "rb");
        if (!f)
        {
            printf("Unable to open: %s\n", script_file);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        uint32_t file_size = (uint32_t) ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = new char[file_size];
        fread(buf, 1, file_size, f);
        fclose(f);

        dmGui::AddTexture(scene, "checker", (void*)(uintptr_t) checker_texture, 0, 2, 2);

        dmLuaDDF::LuaSource luaSource;
        memset(&luaSource, 0x00, sizeof(luaSource));
        luaSource.m_Script.m_Data = (uint8_t*) buf;
        luaSource.m_Script.m_Count = file_size;
        luaSource.m_Filename = script_file;
        dmGui::SetScript(script, &luaSource);

        delete [] buf;
    }

#ifdef __MACH__
    ProcessSerialNumber psn;
    OSErr err;

    // Move window to front. Required if running without application bundle.
    err = GetCurrentProcess( &psn );
    if (err == noErr)
        (void) SetFrontProcess( &psn );
#endif
    glfwSetWindowTitle("GUI Demo");

    running = GL_TRUE;
    while (running)
    {
        dmGui::HScene scene = g_Scenes[g_CurrentScene];
        glfwSetKeyCallback(&OnKey);
        dmGui::UpdateScene(scene, 1.0f / 60.0f);

        t = (float) glfwGetTime();
        glfwGetMousePos(&x, &y);

        int width, height;
        glfwGetWindowSize(&width, &height);
        height = height > 0 ? height : 1;
        if (width != current_width || height != current_height)
        {
            dmGui::SetPhysicalResolution(context, width, height);
            current_width = width;
            current_height = height;
        }

        glViewport(0, 0, current_width, current_height);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear( GL_COLOR_BUFFER_BIT);

        glEnable (GL_BLEND);

        glMatrixMode( GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, current_width, 0, current_height);

        glMatrixMode( GL_MODELVIEW);
        glLoadIdentity();

        dmGui::RenderScene(scene, &MyRenderNodes, 0);

        glfwSwapBuffers();

        running = !glfwGetKey(GLFW_KEY_ESC) && glfwGetWindowParam(GLFW_OPENED);
    }

    for (uint32_t i = 0; i < SCENE_COUNT; ++i)
    {
        dmGui::DeleteScript(dmGui::GetSceneScript(g_Scenes[i]));
        dmGui::DeleteScene(g_Scenes[i]);
    }
    dmGui::DeleteContext(context, context_params.m_ScriptContext);
    dmScript::DeleteContext(context_params.m_ScriptContext);

    glfwCloseWindow();

    glfwTerminate();

    return 0;
}
