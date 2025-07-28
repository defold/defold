// SPDX-FileCopyrightText: 2025 Mikko Mononen
// SPDX-License-Identifier: MIT

#include "test_macros.h"

static int test0(void)
{
	ENSURE(12 > 5);

	return 0;
}

int basic_tests(void)
{
	RUN_SUBTEST(test0);
	return 0;
}
