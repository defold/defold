#include <CoreText/CoreText.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *path;
    char *family;
    char *style;
    char *fullname;
} FontEntry;

typedef struct {
    char *path;
    FontEntry *entries;
    size_t count;
    size_t capacity;
} PathGroup;

static char *dup_cstr(const char *s) {
    size_t n = strlen(s) + 1;
    char *r = (char *)malloc(n);
    if (r) memcpy(r, s, n);
    return r;
}

static int cfstring_to_cstr(CFStringRef s, char *buf, size_t size) {
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    if (!s) return 0;
    return CFStringGetCString(s, buf, (CFIndex)size, kCFStringEncodingUTF8);
}

static int ends_with_ci(const char *s, const char *suffix) {
    size_t ls = strlen(s), le = strlen(suffix);
    if (le > ls) return 0;
    s += ls - le;
    for (size_t i = 0; i < le; ++i) {
        char a = s[i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int cmp_font_entry(const void *a, const void *b) {
    const FontEntry *fa = (const FontEntry *)a;
    const FontEntry *fb = (const FontEntry *)b;
    int c = strcmp(fa->path, fb->path);
    if (c) return c;
    c = strcmp(fa->family, fb->family);
    if (c) return c;
    c = strcmp(fa->style, fb->style);
    if (c) return c;
    return strcmp(fa->fullname, fb->fullname);
}

static int same_face(const FontEntry *a, const FontEntry *b) {
    return strcmp(a->path, b->path) == 0 &&
           strcmp(a->family, b->family) == 0 &&
           strcmp(a->style, b->style) == 0 &&
           strcmp(a->fullname, b->fullname) == 0;
}

static void group_append(PathGroup *g, FontEntry e) {
    if (g->count == g->capacity) {
        size_t newcap = g->capacity ? g->capacity * 2 : 8;
        FontEntry *p = (FontEntry *)realloc(g->entries, newcap * sizeof(FontEntry));
        if (!p) return;
        g->entries = p;
        g->capacity = newcap;
    }
    g->entries[g->count++] = e;
}

int main(void) {
    CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts(NULL);
    if (!collection) {
        fprintf(stderr, "Failed to create font collection\n");
        return 1;
    }

    CFArrayRef matches = CTFontCollectionCreateMatchingFontDescriptors(collection);
    CFRelease(collection);

    if (!matches) {
        fprintf(stderr, "Failed to get font descriptors\n");
        return 1;
    }

    CFIndex count = CFArrayGetCount(matches);
    FontEntry *all = (FontEntry *)calloc((size_t)count, sizeof(FontEntry));
    if (!all) {
        CFRelease(matches);
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    size_t n = 0;
    for (CFIndex i = 0; i < count; ++i) {
        CTFontDescriptorRef desc =
            (CTFontDescriptorRef)CFArrayGetValueAtIndex(matches, i);
        if (!desc) continue;

        CFStringRef family =
            (CFStringRef)CTFontDescriptorCopyAttribute(desc, kCTFontFamilyNameAttribute);
        CFStringRef style =
            (CFStringRef)CTFontDescriptorCopyAttribute(desc, kCTFontStyleNameAttribute);
        CFStringRef fullname =
            (CFStringRef)CTFontDescriptorCopyAttribute(desc, kCTFontNameAttribute);
        CFURLRef url =
            (CFURLRef)CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);

        if (!url) {
            if (family) CFRelease(family);
            if (style) CFRelease(style);
            if (fullname) CFRelease(fullname);
            continue;
        }

        char path[4096], family_buf[512], style_buf[512], fullname_buf[512];
        path[0] = family_buf[0] = style_buf[0] = fullname_buf[0] = '\0';

        if (!CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, sizeof(path))) {
            CFRelease(url);
            if (family) CFRelease(family);
            if (style) CFRelease(style);
            if (fullname) CFRelease(fullname);
            continue;
        }

        if (!ends_with_ci(path, ".ttf") && !ends_with_ci(path, ".ttc")) {
            CFRelease(url);
            if (family) CFRelease(family);
            if (style) CFRelease(style);
            if (fullname) CFRelease(fullname);
            continue;
        }

        cfstring_to_cstr(family, family_buf, sizeof(family_buf));
        cfstring_to_cstr(style, style_buf, sizeof(style_buf));
        cfstring_to_cstr(fullname, fullname_buf, sizeof(fullname_buf));

        all[n].path     = dup_cstr(path);
        all[n].family   = dup_cstr(family_buf[0]   ? family_buf   : "(unknown)");
        all[n].style    = dup_cstr(style_buf[0]    ? style_buf    : "(unknown)");
        all[n].fullname = dup_cstr(fullname_buf[0] ? fullname_buf : "(unknown)");
        if (all[n].path && all[n].family && all[n].style && all[n].fullname) {
            n++;
        }

        CFRelease(url);
        if (family) CFRelease(family);
        if (style) CFRelease(style);
        if (fullname) CFRelease(fullname);
    }

    CFRelease(matches);

    qsort(all, n, sizeof(FontEntry), cmp_font_entry);

    size_t unique_n = 0;
    for (size_t i = 0; i < n; ++i) {
        if (unique_n == 0 || !same_face(&all[i], &all[unique_n - 1])) {
            all[unique_n++] = all[i];
        } else {
            free(all[i].path);
            free(all[i].family);
            free(all[i].style);
            free(all[i].fullname);
        }
    }
    n = unique_n;

    PathGroup current = {0};

    for (size_t i = 0; i < n; ++i) {
        FontEntry *e = &all[i];

        if (!current.path || strcmp(current.path, e->path) != 0) {
            if (current.path) {
                if (ends_with_ci(current.path, ".ttf")) {
                    FontEntry *x = &current.entries[0];
                    printf("%s | %s | %s\n", x->family, x->style, current.path);
                } else if (ends_with_ci(current.path, ".ttc")) {
                    printf("%s\n", current.path);
                    for (size_t j = 0; j < current.count; ++j) {
                        FontEntry *x = &current.entries[j];
                        printf("  %zu, %s | %s | %s | %s\n",
                               j, x->family, x->style, x->fullname, current.path);
                    }
                }
                free(current.path);
                free(current.entries);
                current.path = NULL;
                current.entries = NULL;
                current.count = 0;
                current.capacity = 0;
            }
            current.path = dup_cstr(e->path);
        }

        group_append(&current, *e);
    }

    if (current.path) {
        if (ends_with_ci(current.path, ".ttf")) {
            FontEntry *x = &current.entries[0];
            printf("%s | %s | %s\n", x->family, x->style, current.path);
        } else if (ends_with_ci(current.path, ".ttc")) {
            printf("%s\n", current.path);
            for (size_t j = 0; j < current.count; ++j) {
                FontEntry *x = &current.entries[j];
                printf("  %zu, %s | %s | %s | %s\n",
                       j, x->family, x->style, x->fullname, current.path);
            }
        }
        free(current.path);
        free(current.entries);
    }

    for (size_t i = 0; i < n; ++i) {
        free(all[i].path);
        free(all[i].family);
        free(all[i].style);
        free(all[i].fullname);
    }
    free(all);

    return 0;
}
