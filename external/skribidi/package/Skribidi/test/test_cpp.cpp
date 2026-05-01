// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

// This test is currently set up just to test the that the headers compile when included from C++.
// The test can be elaborated if C++ related issues arise.
#include "test_macros.h"
#include "skb_canvas.h"
#include "skb_common.h"
#include "skb_editor.h"
#include "skb_font_collection.h"
#include "skb_icon_collection.h"
#include "skb_layout.h"
#include "skb_layout_cache.h"
#include "skb_rasterizer.h"
#include "skb_image_atlas.h"

extern "C" int cpp_tests(void)
{
	return 0;
}
