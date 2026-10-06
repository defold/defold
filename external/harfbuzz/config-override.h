// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

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
