#include <stdio.h>
#include "test_ext.h"

extern "C"
{
	void Test()
	{
        printf("Hello Test\n");
	}
}
