
#ifndef HB_CUSTOM_CONFIG_OVERRIDE_H
#define HB_CUSTOM_CONFIG_OVERRIDE_H

// We want HB_TINY, but can't due to Skribidi
// so we need to undefine these

#undef HB_NO_COLOR
#undef HB_NO_FACE_COLLECT_UNICODES
#undef HB_NO_METRICS
#undef HB_NO_OT_LAYOUT
#undef HB_NO_LAYOUT_FEATURE_PARAMS
#undef HB_NO_STYLE

// The full text-layout engine uses HarfBuzz as its sole font parser. DRAW
// exposes glyph outlines, CFF decodes OpenType CFF1/CFF2 charstrings, and VAR
// supplies the variation machinery used by CFF2.
#undef HB_NO_DRAW
#undef HB_NO_CFF
#undef HB_NO_VAR

// Keep HarfBuzz's per-font lookup caches. They add a small amount of memory
// after shaping a font, but substantially reduce repeated layout time.
#undef HB_MINIMIZE_MEMORY_USAGE

// Bob compiles multiple fonts in parallel, and the engine may also use fonts
// from multiple threads. HB_TINY disables HarfBuzz's atomics and mutexes, which
// makes shared lazy loaders such as the OpenType font functions unsafe.
#undef HB_NO_MT

#endif // HB_CUSTOM_CONFIG_OVERRIDE_H
