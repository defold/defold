// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "debug_draw.h"
#include "utils.h"
#include "ime.h"

#include "skb_common.h"

GLFWwindow* g_window = NULL;
int g_done = 0;
int g_last_key_mods = 0;

typedef struct example_stub_t {
	example_create_t* create;
	const char* name;
} example_stub_t;

void* richtext_create(void);
void* testbed_create(void);
void* icons_create(void);
void* cached_create(void);
void* fallback_create(void);
void* decorations_create(void);
void* aligns_create(void);
void* inlineobj_create(void);

static example_stub_t g_examples[] = {
	{ .create = richtext_create, .name = "Rich text" },
	{ .create = testbed_create, .name = "Testbed" },
	{ .create = icons_create, .name = "Icons" },
	{ .create = cached_create, .name = "Cached" },
	{ .create = fallback_create, .name = "Font Fallback" },
	{ .create = decorations_create, .name = "Text Decorations" },
	{ .create = aligns_create, .name = "Layout Align" },
	{ .create = inlineobj_create, .name = "Inline Objects" },
};
static const int32_t g_examples_count = SKB_COUNTOF(g_examples);
int32_t g_example_idx = SKB_INVALID_INDEX;
example_t* g_example = NULL;

static void main_loop(void*);

static void glfw_error_callback(int error, const char* description)
{
	skb_debug_log("GLFW Error %d: %s\n", error, description);
}

static int32_t skb__wrap(int32_t i, int32_t n)
{
	i = i % n;
	return i < 0 ? i+n : i;
}

static void set_example(int32_t example_idx)
{
	if (g_example && g_example->destroy) {
		g_example->destroy(g_example);
		g_example = NULL;
	}

	g_example_idx = skb__wrap(example_idx, g_examples_count);

	g_example = g_examples[g_example_idx].create();
	assert(g_example);
}

static float g_pixel_scale_x = 1.f;
static float g_pixel_scale_y = 1.f;

static void update_pixel_scale(GLFWwindow* window)
{
	int32_t win_width, win_height;
	int32_t fb_width, fb_height;
	glfwGetWindowSize(g_window, &win_width, &win_height);
	glfwGetFramebufferSize(g_window, &fb_width, &fb_height);

	g_pixel_scale_x = (float)fb_width / (float)win_width;
	g_pixel_scale_y = (float)fb_height / (float)win_height;
}

static void resize_callback(GLFWwindow* window, int width, int height)
{
	update_pixel_scale(window);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	(void)scancode;
	g_last_key_mods = mods;

	if (action == GLFW_PRESS) {
		if (key == GLFW_KEY_F1)
			set_example(g_example_idx + 1);
	}

	if (g_example && g_example->on_key)
		g_example->on_key(g_example, window, key, action, mods);
}

static void char_callback(GLFWwindow* window, unsigned int codepoint)
{
	if (g_example && g_example->on_char)
		g_example->on_char(g_example, codepoint);
}

static void mouse_scale_pos_to_screen_coords(GLFWwindow* window, double* xpos, double* ypos)
{
	*xpos *= g_pixel_scale_x;
	*ypos *= g_pixel_scale_y;
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	(void)window;

	g_last_key_mods = mods;

	double xpos, ypos;
	glfwGetCursorPos(g_window, &xpos, &ypos);
	mouse_scale_pos_to_screen_coords(window, &xpos, &ypos);

	if (g_example && g_example->on_mouse_button)
		g_example->on_mouse_button(g_example, (float)xpos, (float)ypos, button, action, mods);
}

static void mouse_move_callback(GLFWwindow* window, double xpos, double ypos)
{
	mouse_scale_pos_to_screen_coords(window, &xpos, &ypos);
	if (g_example && g_example->on_mouse_move)
		g_example->on_mouse_move(g_example, (float)xpos, (float)ypos);
}

static void mouse_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	double xpos, ypos;
	glfwGetCursorPos(g_window, &xpos, &ypos);
	mouse_scale_pos_to_screen_coords(window, &xpos, &ypos);

	if (g_example && g_example->on_mouse_scroll)
		g_example->on_mouse_scroll(g_example, (float)xpos, (float)ypos, (float)xoffset, (float)yoffset, g_last_key_mods);
}


int main(int argc, char** args)
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
		return -1;

	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

	// GL 3.0 + GLSL 130
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_STENCIL_BITS, 8);
	// MSAA is not necessary for the font rendering. In these examples, it is mostly used to get antialiased lines.
	// Though, MSAA or multi sampling in shader, _does_ improve the rendering of small glyphs, resulting more consistent stem widths.
	glfwWindowHint(GLFW_SAMPLES, 4);

	g_window = glfwCreateWindow(mode->width - 200, mode->height - 200, "Skribidi", NULL, NULL);
	if (!g_window) {
		glfwTerminate();
		return -1;
	}

	if (!ime_init(g_window)) {
		skb_debug_log("Failed to initialize IME.");
		glfwTerminate();
		return -1;
	}

	glfwSetWindowSizeCallback(g_window, resize_callback);
	glfwSetKeyCallback(g_window, key_callback);
	glfwSetCharCallback(g_window, char_callback);
	glfwSetMouseButtonCallback(g_window, mouse_button_callback);
	glfwSetCursorPosCallback(g_window, mouse_move_callback);
	glfwSetScrollCallback(g_window, mouse_scroll_callback);


	glfwMakeContextCurrent(g_window);
	glfwSwapInterval(1); // Enable vsync
	update_pixel_scale(g_window);

	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0) {
		skb_debug_log("Failed to initialize OpenGL context\n");
		return -1;
	}

	if (!draw_init()) {
		skb_debug_log("Failed to initialize draw\n");
		return -1;
	}

	// Init first example
	set_example(0);

	while (!g_done) {
		main_loop(0);
	}

	if (g_example && g_example->destroy) {
		g_example->destroy(g_example);
		g_example = NULL;
	}

	draw_terminate();
	ime_terminate();
	glfwDestroyWindow(g_window);
	glfwTerminate();

	return 0;
}

static void main_loop(void* arg)
{
	if (glfwWindowShouldClose(g_window)) {
		g_done = 1;
		return;
	}

	int32_t win_width, win_height;
	int32_t fb_width, fb_height;
	glfwGetWindowSize(g_window, &win_width, &win_height);
	glfwGetFramebufferSize(g_window, &fb_width, &fb_height);

	// Update and render
	glViewport(0, 0, fb_width, fb_height);

	glClearColor(0.9f, 0.91f, 0.92f, 1.0f);
	glClearStencil(0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	if (g_example && g_example->on_update)
		g_example->on_update(g_example, fb_width, fb_height);

	draw_filled_rect(0, (float)fb_height - 35.f, (float)fb_width, 35.f, skb_rgba(255,255,255,128));
	draw_text(15.f, (float)fb_height - 15.f, 12.f, 0.f, skb_rgba(0,0,0,255), "F1: Next Example (%s)   ESC: Exit", g_examples[skb__wrap(g_example_idx+1, g_examples_count)].name);

	float title_width = draw_text_width(15.f, g_examples[g_example_idx].name);
	draw_filled_rect(0, 0, 25+title_width+25, 30.f, skb_rgba(0,0,0,128));
	draw_line_width(2.f);
	draw_text(20.f, 20.f, 15.f, 0.0f, skb_rgba(255,255,255,255), g_examples[g_example_idx].name);
	draw_line_width(1.f);

	draw_flush(fb_width, fb_height);

	glEnable(GL_DEPTH_TEST);
	glfwSwapBuffers(g_window);

	glfwWaitEventsTimeout(1.0);
}


// Single header lib implementations
#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
