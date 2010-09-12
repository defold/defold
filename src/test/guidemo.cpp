#include <stdio.h>
#include <graphics/glfw/GL/glfw.h>
#include "../gui.h"

void MyRenderNode(dmGui::HScene scene,
                  const dmGui::Node* nodes,
                  uint32_t node_count,
                  void* context)
{
    for (uint32_t i = 0; i < node_count; ++i)
    {
        const dmGui::Node* node = &nodes[i];

        Vector4 pos = node->m_Properties[dmGui::PROPERTY_POSITION];
        Vector4 ext = node->m_Properties[dmGui::PROPERTY_EXTENTS];
        Vector4 color = node->m_Properties[dmGui::PROPERTY_COLOR];
        glColor4f(color.getX(), color.getY(), color.getZ(), color.getW());

        float x = pos.getX();
        float y = pos.getY();
        float z = pos.getZ();
        float dx = ext.getX() * 0.5f;
        float dy = ext.getY() * 0.5f;
        glBegin(GL_QUADS);

        glVertex3f(x - dx, y - dy, z);
        glVertex3f(x + dx, y - dy, z);
        glVertex3f(x + dx, y + dy, z);
        glVertex3f(x - dx, y + dy, z);

        glEnd();
    }
}

int main(void)
{
    int width, height, running, x, y;
    float t;

    glfwInit();

    if (!glfwOpenWindow(700, 700, 0, 0, 0, 0, 0, 0, GLFW_WINDOW))
    {
        glfwTerminate();
        return 1;
    }

    glfwEnable(GLFW_STICKY_KEYS);

    glfwSwapInterval(1);

    dmGui::HGui gui = dmGui::New();
    dmGui::NewSceneParams params;
    params.m_MaxNodes = 256;
    params.m_MaxAnimations = 1024;
    dmGui::HScene scene = dmGui::NewScene(gui, &params);

    const char* script_file = "src/test/guidemo.lua";
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

    dmGui::SetSceneScript(scene, buf, file_size);

    running = GL_TRUE;
    while (running)
    {
        dmGui::UpdateScene(scene, 1.0f / 60.0f);

        t = glfwGetTime();
        glfwGetMousePos(&x, &y);

        glfwGetWindowSize(&width, &height);
        height = height > 0 ? height : 1;

        glViewport(0, 0, width, height);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear( GL_COLOR_BUFFER_BIT);

        glMatrixMode( GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0, 1, 0, 1);

        glMatrixMode( GL_MODELVIEW);
        glLoadIdentity();

        dmGui::RenderScene(scene, &MyRenderNode, 0);

        glfwSwapBuffers();

        running = !glfwGetKey(GLFW_KEY_ESC) && glfwGetWindowParam(GLFW_OPENED);
    }

    glfwTerminate();

    return 0;
}
