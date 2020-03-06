#include <stdint.h>
#include <stdio.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <app/app.h>

TEST(dmApp, Test)
{
    printf("APP TEST!\n");
    ASSERT_TRUE(true);
}

TEST(dmApp, malloc)
{
	void* p = malloc(16*1024*1024);
    ASSERT_NE((void*)0, p);
    free(p);
}


TEST(dmApp, fopen)
{
	// Just to see that basic file mounts work
	FILE* f = fopen("cache://test.txt", "wb");
    ASSERT_NE((FILE*)0, f);
    fclose(f);
}


int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
