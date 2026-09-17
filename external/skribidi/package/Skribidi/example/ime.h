// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#ifndef IME_H
#define IME_H

#include <stdint.h>
#include <stdbool.h>
#include "skb_common.h"

typedef struct GLFWwindow GLFWwindow;

//
// Minimal IME implementation which allows us to test the editor IME functionality.
//

typedef enum {
	IME_EVENT_COMPOSITION,
	IME_EVENT_COMMIT,
	IME_EVENT_CANCEL,
} ime_event_t;

typedef void ime_event_handler_func_t(ime_event_t event, const uint32_t* text, int32_t text_length, int32_t caret_position, void* context);

bool ime_init(GLFWwindow* window);
void ime_set_handler(ime_event_handler_func_t* handler, void* context);
void ime_set_input_rect(skb_rect2i_t rect);
void ime_cancel(void);
void ime_terminate(void);

#endif // IME_H
