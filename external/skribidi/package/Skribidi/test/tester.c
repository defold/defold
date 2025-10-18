// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"

int basic_tests(void);
int tempalloc_tests(void);
int hashtable_tests(void);
int canvas_tests(void);
int font_collection_tests(void);
int icon_collection_tests(void);
int editor_tests(void);
int layout_tests(void);
int layout_cache_tests(void);
int rasterizer_tests(void);
int image_atlas_tests(void);
int cpp_tests(void);

int main( void )
{

	printf( "Starting Skribidi unit tests\n" );
	printf( "======================================\n" );

	RUN_TEST(basic_tests);
	RUN_TEST(tempalloc_tests);
	RUN_TEST(hashtable_tests);
	RUN_TEST(canvas_tests);
	RUN_TEST(font_collection_tests);
	RUN_TEST(icon_collection_tests);
	RUN_TEST(editor_tests);
	RUN_TEST(layout_tests);
	RUN_TEST(layout_cache_tests);
	RUN_TEST(rasterizer_tests);
	RUN_TEST(image_atlas_tests);
	RUN_TEST(cpp_tests);

	printf( "======================================\n" );
	printf( "All tests passed!\n" );

	return 0;
}
