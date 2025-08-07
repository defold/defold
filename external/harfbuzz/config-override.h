
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

#endif // HB_CUSTOM_CONFIG_OVERRIDE_H
