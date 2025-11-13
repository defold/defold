#include <stdio.h>

#include "budoux.h"

int main(void)
{
	char const sentence[] = "私はその人を常に先生と呼んでいた。\n"
								 "だからここでもただ先生と書くだけで本名は打ち明けない。\n"
								 "これは世間を憚かる遠慮というよりも、その方が私にとって自然だからである。";

	int32_t range_start = 0, range_end = 0;
	boundary_iterator_t iter;
	
	// test utf-8
	printf("utf-8:\n");
	iter = boundary_iterator_init_ja_utf8(sentence, -1);
	while (boundary_iterator_next(&iter, &range_start, &range_end)) {
		for (int i = range_start; i < range_end; i++)
			printf("%c", sentence[i]);
		printf("|");
	}

	uint32_t sentence32[sizeof(sentence) / sizeof(sentence[0])];
	utf8_to_utf32(sentence, sentence32);

	// test utf-8
	printf("\n\nutf-32:\n");
	iter = boundary_iterator_init_ja_utf32(sentence32, -1);
	while (boundary_iterator_next(&iter, &range_start, &range_end)) {
		for (int i = range_start; i < range_end; i++) {
			uint32_t cp = sentence32[i];
			if (cp < 0x80) printf("%c", cp);
			else if (cp < 0x800) printf("%c%c", (0xc0 | (cp >> 6)), (0x80 | (cp & 0x3f)));
			else if (cp < 0x10000) printf("%c%c%c", (0xe0 | (cp >> 12)), (0x80 | ((cp >> 6) & 0x3f)), (0x80 | (cp & 0x3f)));
			else if (cp < 0x200000) printf("%c%c%c%c", (0xf0 | (cp >> 18)), (0x80 | ((cp >> 12) & 0x3f)), (0x80 | ((cp >> 6)  & 0x3f)), (0x80 | (cp & 0x3f)));
		}
		printf("|");
	}
	
	return 0;
}