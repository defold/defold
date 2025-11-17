//
// From Box2D project
//
// SPDX-FileCopyrightText: 2023 Erin Catto
// SPDX-License-Identifier: MIT

#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#define RUN_TEST( T )                                                                                                            \
	do                                                                                                                           \
	{                                                                                                                            \
		int result = T();                                                                                                        \
		if ( result == 1 )                                                                                                       \
		{                                                                                                                        \
			printf( "test FAILED: " #T "\n" );                                                                                   \
			return 1;                                                                                                            \
		}                                                                                                                        \
		else                                                                                                                     \
		{                                                                                                                        \
			printf( "test passed: " #T "\n" );                                                                                   \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( false )

#define RUN_SUBTEST( T )                                                                                                         \
	do                                                                                                                           \
	{                                                                                                                            \
		int result = T();                                                                                                        \
		if ( result == 1 )                                                                                                       \
		{                                                                                                                        \
			printf( "  subtest FAILED: " #T "\n" );                                                                              \
			return 1;                                                                                                            \
		}                                                                                                                        \
		else                                                                                                                     \
		{                                                                                                                        \
			printf( "  subtest passed: " #T "\n" );                                                                              \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( false )

#define ENSURE( C )                                                                                                              \
	do                                                                                                                           \
	{                                                                                                                            \
		if ( ( C ) == false )                                                                                                    \
		{                                                                                                                        \
			printf( "%s:%d: condition FAILED: %s\n", __FILE__, __LINE__, #C );                                                   \
			/*assert( false );*/                                                                                                 \
			return 1;                                                                                                            \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( false )

#define ENSURE_SMALL( C, tol )                                                                                                   \
	do                                                                                                                           \
	{                                                                                                                            \
		if ( ( C ) < -( tol ) || ( tol ) < ( C ) )                                                                               \
		{                                                                                                                        \
			printf( "condition FAILED: abs(" #C ") < %g\n", tol );                                                               \
			/*assert( false );*/                                                                                                 \
			return 1;                                                                                                            \
		}                                                                                                                        \
	}                                                                                                                            \
	while ( false )

#endif // TEST_MACROS_H