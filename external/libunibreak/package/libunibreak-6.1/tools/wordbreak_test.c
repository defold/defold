/* Compilation:
 * gcc wordbreak_test.c `pkg-config --libs --cflags libunibreak`
 *
 * Note: Output is not aligned nicely when used with non-ascii chars.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordbreak.h>

int
main(int argc, char *argv[])
{
   const char *lang = "";
   const char *text;
   size_t len;
   char *breaks;
   size_t i;

   if (argc != 2)
     {
        printf("Usage: %s <text>\n", argv[0]);
        exit(1);
     }

   text = argv[1];
   len = strlen(text);
   breaks = malloc(len);
   printf("%s\n", text);

   set_wordbreaks_utf8((const utf8_t *) text, len, lang, breaks);
   for (i = 0 ; i < len ; i++)
      printf("%d", (int) breaks[i]);
   printf("\n");

   return 0;
}
