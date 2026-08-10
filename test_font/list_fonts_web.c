#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
void on_font(const char* name, const char* style, int weight, const char* path);
void on_done(void);
#ifdef __cplusplus
}
#endif

int main(void) {
    puts("Click the button to request local font access.");
    return 0;
}

void on_font(const char* name, const char* style, int weight, const char* path) {
    printf("name=%s | style=%s | weight=%d | path=%s\n", name, style, weight, path);
}

void on_done(void) {
    puts("done");
}
